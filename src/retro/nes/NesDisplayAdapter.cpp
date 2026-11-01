#include "NesDisplayAdapter.h"
#include "NesPalette.h"
#include <Preferences.h>

static Adafruit_ST7735* tft = nullptr;
static Preferences s_prefs;
static NesDisplayMode s_displayMode = NES_DISPLAY_HYBRID;
static bool s_prefsLoaded = false;

// Multi-line burst buffer configuration:
// 8 scanlines * 128 pixels * 2 bytes = 2048 bytes in SRAM (no full framebuffer!)
static constexpr int BURST_LINES = 8;
static constexpr int BURST_PIXELS = 128 * BURST_LINES;

static uint16_t burstBuffer[BURST_PIXELS];
static int bufferedLines = 0;
static bool inFrame = false;

// Hybrid Mode Proportional Zoom:
// Source viewport: 192 x 240 (centered X=32..223, all 240 scanlines)
// Destination: 128 x 160 (full panel, 0% aspect ratio distortion: Sx = Sy = 2/3)
// Zero dynamic heap, zero extra framebuffers
static uint8_t s_xLutHybrid[128];
static int s_hybridScanlineIdx = 0;

// Microsecond profiling accumulators per frame
static uint32_t s_renderTimeUs = 0;
static uint32_t s_tftTimeUs = 0;

NesDisplayMode NesDisplayAdapter::loadSavedDisplayMode() {
    s_prefs.begin("retro_cfg", true); // Read-only
    uint8_t val = s_prefs.getUChar("disp_mode", (uint8_t)NES_DISPLAY_HYBRID);
    s_prefs.end();
    if (val > (uint8_t)NES_DISPLAY_FULL) {
        val = (uint8_t)NES_DISPLAY_HYBRID;
    }
    s_displayMode = (NesDisplayMode)val;
    s_prefsLoaded = true;
    return s_displayMode;
}

void NesDisplayAdapter::saveDisplayMode(NesDisplayMode mode) {
    s_displayMode = mode;
    s_prefs.begin("retro_cfg", false); // Read-write
    s_prefs.putUChar("disp_mode", (uint8_t)mode);
    s_prefs.end();
    Serial.printf("[NesDisplayAdapter] Display mode saved: %s\n",
                  (mode == NES_DISPLAY_HYBRID) ? "HYBRID" : "FULL");
}

void NesDisplayAdapter::setDisplayMode(NesDisplayMode mode) {
    s_displayMode = mode;
}

NesDisplayMode NesDisplayAdapter::getDisplayMode() {
    if (!s_prefsLoaded) {
        loadSavedDisplayMode();
    }
    return s_displayMode;
}

void NesDisplayAdapter::init(Adafruit_ST7735* display) {
    tft = display;
    inFrame = false;
    bufferedLines = 0;
    s_hybridScanlineIdx = 0;

    if (tft) {
        // Boost SPI to 40 MHz for ST7735 hardware
        tft->setSPISpeed(40000000UL);
    }
    loadSavedDisplayMode();
    NesPalette::init();

    // Precompute 3:2 horizontal nearest-neighbor LUT for Hybrid mode
    // Maps 128 destination pixels to 192 source pixels centered in NES 256 width (X=32..223)
    for (int x = 0; x < 128; x++) {
        s_xLutHybrid[x] = 32 + (uint8_t)((x * 3) / 2);
    }

    Serial.printf("[NesDisplayAdapter] Initialized | Mode: %s | Palette: Sat=+%d%%, Exp=-%d%%, Con=+%d%%\n",
                  (s_displayMode == NES_DISPLAY_HYBRID) ? "HYBRID" : "FULL",
                  NesPalette::getSaturationBoost(), NesPalette::getExposureReduce(), NesPalette::getContrastBoost());
}

void NesDisplayAdapter::clearBorders() {
    if (!tft) return;
    tft->setRotation(0); // 128x160 Portrait
    if (s_displayMode == NES_DISPLAY_FULL) {
        // Full mode: Top and bottom 20px black letterbox
        tft->fillRect(0, 0, 128, 20, 0x0000);
        tft->fillRect(0, 140, 128, 20, 0x0000);
    } else {
        // Hybrid mode: fills the entire 128x160 screen continuously, blank screen once
        tft->fillScreen(0x0000);
    }
}

void NesDisplayAdapter::startFrame() {
    if (!tft) return;
    tft->setRotation(0);
    tft->startWrite();

    if (s_displayMode == NES_DISPLAY_FULL) {
        // Centered 128x120 active area (Y=20..139) with 20px black letterbox top & bottom
        tft->setAddrWindow(0, 20, 128, 120);
    } else {
        // Hybrid mode: fills full 128x160 active area (Y=0..159) with single proportional zoom
        tft->setAddrWindow(0, 0, 128, 160);
    }

    bufferedLines = 0;
    s_hybridScanlineIdx = 0;
    inFrame = true;
}

void NesDisplayAdapter::renderScanlineIndexed(int scanline, const uint8_t* nesIndices256, const uint16_t* palette64) {
    if (!tft || !inFrame || !nesIndices256) return;
    // Only even scanlines (0, 2, ..., 238) are processed
    if ((scanline & 1) != 0 || scanline >= 240) return;

    const uint16_t* activePalette = NesPalette::getEnhancedPalette(0);
    uint32_t t0 = micros();

    if (s_displayMode == NES_DISPLAY_FULL) {
        // =====================================================================
        // FULL GAME MODE (PRESERVED 100% UNCHANGED):
        // 2:1 integer downsampling into 128x120 centered viewport
        // =====================================================================
        uint16_t* lineDest = &burstBuffer[bufferedLines * 128];
        for (int x = 0; x < 128; x++) {
            lineDest[x] = activePalette[nesIndices256[x * 2]];
        }
        bufferedLines++;

        if (bufferedLines >= BURST_LINES) {
            uint32_t t2 = micros();
            tft->writePixels(burstBuffer, BURST_PIXELS);
            uint32_t t3 = micros();
            s_tftTimeUs += (t3 - t2);
            bufferedLines = 0;
        }
    } else {
        // =====================================================================
        // NEW HYBRID MODE: SINGLE INTENLLIGENT PROPORTIONAL ZOOM (192x240 -> 128x160)
        // 1. Horizontal: 192 source pixels (centered X=32..223) -> 128 dest pixels via LUT
        // 2. Vertical: 120 received scanlines -> 160 TFT lines (4:3 expansion, repeat line 0 of every 3)
        // Scale ratio: Sx = 128/192 = 2/3, Sy = 160/240 = 2/3 (Sx == Sy -> 0% aspect distortion!)
        // Single unified image, zero duplicate viewports, zero separators, fills entire 128x160 screen
        // =====================================================================
        uint16_t tempLine[128];
        for (int x = 0; x < 128; x++) {
            tempLine[x] = activePalette[nesIndices256[s_xLutHybrid[x]]];
        }

        // For every 3 received scanlines: line 0 repeated twice, line 1 once, line 2 once -> exactly 4 lines!
        // 120 received scanlines * 4 / 3 = exactly 160 destination lines!
        int repeatCount = (s_hybridScanlineIdx % 3 == 0) ? 2 : 1;
        s_hybridScanlineIdx++;

        for (int r = 0; r < repeatCount; r++) {
            memcpy(&burstBuffer[bufferedLines * 128], tempLine, 128 * sizeof(uint16_t));
            bufferedLines++;

            if (bufferedLines >= BURST_LINES) {
                uint32_t t2 = micros();
                tft->writePixels(burstBuffer, BURST_PIXELS);
                uint32_t t3 = micros();
                s_tftTimeUs += (t3 - t2);
                bufferedLines = 0;
            }
        }
    }

    uint32_t t1 = micros();
    s_renderTimeUs += (t1 - t0);
}

void NesDisplayAdapter::renderScanline(int scanline, const uint16_t* nesScanline256) {
    if (!tft || !inFrame || !nesScanline256) return;
    if ((scanline & 1) != 0 || scanline >= 240) return;

    uint32_t t0 = micros();

    if (s_displayMode == NES_DISPLAY_FULL) {
        uint16_t* lineDest = &burstBuffer[bufferedLines * 128];
        for (int x = 0; x < 128; x++) {
            lineDest[x] = NesPalette::enhanceColor(nesScanline256[x * 2]);
        }
        bufferedLines++;

        if (bufferedLines >= BURST_LINES) {
            uint32_t t2 = micros();
            tft->writePixels(burstBuffer, BURST_PIXELS);
            uint32_t t3 = micros();
            s_tftTimeUs += (t3 - t2);
            bufferedLines = 0;
        }
    } else {
        uint16_t tempLine[128];
        for (int x = 0; x < 128; x++) {
            tempLine[x] = NesPalette::enhanceColor(nesScanline256[s_xLutHybrid[x]]);
        }

        int repeatCount = (s_hybridScanlineIdx % 3 == 0) ? 2 : 1;
        s_hybridScanlineIdx++;

        for (int r = 0; r < repeatCount; r++) {
            memcpy(&burstBuffer[bufferedLines * 128], tempLine, 128 * sizeof(uint16_t));
            bufferedLines++;

            if (bufferedLines >= BURST_LINES) {
                uint32_t t2 = micros();
                tft->writePixels(burstBuffer, BURST_PIXELS);
                uint32_t t3 = micros();
                s_tftTimeUs += (t3 - t2);
                bufferedLines = 0;
            }
        }
    }

    uint32_t t1 = micros();
    s_renderTimeUs += (t1 - t0);
}

void NesDisplayAdapter::endFrame() {
    if (!tft || !inFrame) return;

    // Flush any remaining buffered scanlines
    if (bufferedLines > 0) {
        uint32_t t0 = micros();
        tft->writePixels(burstBuffer, bufferedLines * 128);
        uint32_t t1 = micros();
        s_tftTimeUs += (t1 - t0);
        bufferedLines = 0;
    }

    tft->endWrite();
    inFrame = false;
}

uint32_t NesDisplayAdapter::getRenderTimeUs() { return s_renderTimeUs; }
uint32_t NesDisplayAdapter::getTftTimeUs() { return s_tftTimeUs; }
void NesDisplayAdapter::resetFrameMetrics() {
    s_renderTimeUs = 0;
    s_tftTimeUs = 0;
}
