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

// Hybrid Mode Detail Buffer:
// Allocated from heap on init (4,992 bytes, leaves .dram0.bss intact)
// Captures native 1:1 NES pixel region (X=64..191, even scanlines 132..208)
static constexpr int DETAIL_LINES = 39;
static uint8_t* s_detailIndices = nullptr;
static int s_detailCount = 0;

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
    s_detailCount = 0;

    // Allocate 1:1 detail buffer from heap (zero static BSS cost)
    if (!s_detailIndices) {
        s_detailIndices = (uint8_t*)malloc(DETAIL_LINES * 128);
    }

    if (tft) {
        // Boost SPI to 40 MHz for ST7735 hardware
        tft->setSPISpeed(40000000UL);
    }
    loadSavedDisplayMode();
    NesPalette::init();
    Serial.printf("[NesDisplayAdapter] Initialized | Mode: %s | Palette: Sat=+%d%%, Exp=-%d%%, Con=+%d%%\n",
                  (s_displayMode == NES_DISPLAY_HYBRID) ? "HYBRID" : "FULL",
                  NesPalette::getSaturationBoost(), NesPalette::getExposureReduce(), NesPalette::getContrastBoost());
}

void NesDisplayAdapter::clearBorders() {
    if (!tft) return;
    tft->setRotation(0); // 128x160 Portrait
    if (s_displayMode == NES_DISPLAY_FULL) {
        // Top 20px black letterbox
        tft->fillRect(0, 0, 128, 20, 0x0000);
        // Bottom 20px black letterbox
        tft->fillRect(0, 140, 128, 20, 0x0000);
    } else {
        // Hybrid fills full 128x160 area continuously
        tft->fillScreen(0x0000);
    }
}

void NesDisplayAdapter::startFrame() {
    if (!tft) return;
    tft->setRotation(0);
    tft->startWrite();

    if (s_displayMode == NES_DISPLAY_FULL) {
        // Single address window for 128x120 active area (Y=20..139)
        tft->setAddrWindow(0, 20, 128, 120);
    } else {
        // Single continuous address window for full 128x160 screen (Y=0..159)
        tft->setAddrWindow(0, 0, 128, 160);
    }

    bufferedLines = 0;
    s_detailCount = 0;
    inFrame = true;
}

void NesDisplayAdapter::renderScanlineIndexed(int scanline, const uint8_t* nesIndices256, const uint16_t* palette64) {
    if (!tft || !inFrame || !nesIndices256) return;
    // 2:1 integer vertical downsampling: only render even scanlines (0, 2, ... 238)
    if ((scanline & 1) != 0 || scanline >= 240) return;

    // Direct lookup from active enhanced palette
    const uint16_t* activePalette = NesPalette::getEnhancedPalette(0);

    uint32_t t0 = micros();

    // 1. Direct 2:1 horizontal downscaling into burst buffer for the base game view (128x120)
    uint16_t* lineDest = &burstBuffer[bufferedLines * 128];
    for (int x = 0; x < 128; x++) {
        lineDest[x] = activePalette[nesIndices256[x * 2]];
    }
    bufferedLines++;

    // 2. In Hybrid mode: sample 39 even scanlines from gameplay region (132..208) at 1:1 pixel scale
    // Horizontally centered: X=64..191 (128 native NES pixels mapped 1:1 to 128 TFT pixels)
    if (s_displayMode == NES_DISPLAY_HYBRID && s_detailIndices) {
        if (scanline >= 132 && scanline <= 208 && s_detailCount < DETAIL_LINES) {
            memcpy(&s_detailIndices[s_detailCount * 128], &nesIndices256[64], 128);
            s_detailCount++;
        }
    }

    uint32_t t1 = micros();
    s_renderTimeUs += (t1 - t0);

    // When 8 lines (1024 pixels) are buffered, issue one multi-line burst to SPI
    if (bufferedLines >= BURST_LINES) {
        uint32_t t2 = micros();
        tft->writePixels(burstBuffer, BURST_PIXELS);
        uint32_t t3 = micros();
        s_tftTimeUs += (t3 - t2);
        bufferedLines = 0;
    }
}

void NesDisplayAdapter::renderScanline(int scanline, const uint16_t* nesScanline256) {
    if (!tft || !inFrame || !nesScanline256) return;
    if ((scanline & 1) != 0 || scanline >= 240) return;

    uint32_t t0 = micros();
    uint16_t* lineDest = &burstBuffer[bufferedLines * 128];
    for (int x = 0; x < 128; x++) {
        lineDest[x] = NesPalette::enhanceColor(nesScanline256[x * 2]);
    }
    bufferedLines++;
    uint32_t t1 = micros();
    s_renderTimeUs += (t1 - t0);

    if (bufferedLines >= BURST_LINES) {
        uint32_t t2 = micros();
        tft->writePixels(burstBuffer, BURST_PIXELS);
        uint32_t t3 = micros();
        s_tftTimeUs += (t3 - t2);
        bufferedLines = 0;
    }
}

void NesDisplayAdapter::endFrame() {
    if (!tft || !inFrame) return;

    // Flush any remaining buffered scanlines for the 128x120 base view
    if (bufferedLines > 0) {
        uint32_t t0 = micros();
        tft->writePixels(burstBuffer, bufferedLines * 128);
        uint32_t t1 = micros();
        s_tftTimeUs += (t1 - t0);
        bufferedLines = 0;
    }

    // In Hybrid Mode: Stream Divider (1 line) + 1:1 Pixel-Perfect Detail Viewport (39 lines)
    if (s_displayMode == NES_DISPLAY_HYBRID) {
        const uint16_t* activePalette = NesPalette::getEnhancedPalette(0);
        uint32_t t0 = micros();

        // 1. Divider line (Row 120): sleek metallic accent separator
        for (int x = 0; x < 128; x++) {
            burstBuffer[x] = (x & 1) ? 0x39E7 : 0x2104; // Micro-textured 1px separator
        }
        int curBufLines = 1;

        // 2. Stream 39 detail lines (Rows 121..159) using multi-line bursts
        for (int l = 0; l < DETAIL_LINES; l++) {
            uint16_t* destLine = &burstBuffer[curBufLines * 128];
            if (s_detailIndices && l < s_detailCount) {
                const uint8_t* srcIndices = &s_detailIndices[l * 128];
                for (int x = 0; x < 128; x++) {
                    destLine[x] = activePalette[srcIndices[x]];
                }
            } else {
                memset(destLine, 0, 128 * sizeof(uint16_t)); // Pad with black if needed
            }
            curBufLines++;

            if (curBufLines >= BURST_LINES) {
                uint32_t t_burst_0 = micros();
                tft->writePixels(burstBuffer, curBufLines * 128);
                uint32_t t_burst_1 = micros();
                s_tftTimeUs += (t_burst_1 - t_burst_0);
                curBufLines = 0;
            }
        }

        // Flush remaining detail lines
        if (curBufLines > 0) {
            uint32_t t_burst_0 = micros();
            tft->writePixels(burstBuffer, curBufLines * 128);
            uint32_t t_burst_1 = micros();
            s_tftTimeUs += (t_burst_1 - t_burst_0);
            curBufLines = 0;
        }

        uint32_t t1 = micros();
        s_renderTimeUs += (t1 - t0);
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
