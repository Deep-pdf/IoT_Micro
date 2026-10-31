#include "NesDisplayAdapter.h"
#include "NesPalette.h"

static Adafruit_ST7735* tft = nullptr;

// Multi-line burst buffer configuration:
// 8 scanlines * 128 pixels * 2 bytes = 2048 bytes in SRAM (no full framebuffer!)
// 120 display rows / 8 lines = exactly 15 burst transfers per frame
static constexpr int BURST_LINES = 8;
static constexpr int BURST_PIXELS = 128 * BURST_LINES;

static uint16_t burstBuffer[BURST_PIXELS];
static int bufferedLines = 0;
static bool inFrame = false;

// Microsecond profiling accumulators per frame
static uint32_t s_renderTimeUs = 0;
static uint32_t s_tftTimeUs = 0;

void NesDisplayAdapter::init(Adafruit_ST7735* display) {
    tft = display;
    inFrame = false;
    bufferedLines = 0;
    if (tft) {
        // Boost SPI to 40 MHz for ST7735 hardware
        tft->setSPISpeed(40000000UL);
    }
    NesPalette::init();
    Serial.printf("[NesDisplayAdapter] Active Enhanced Palette bound directly (Sat=+%d%%, Exp=-%d%%, Con=+%d%%)\n",
                  NesPalette::getSaturationBoost(), NesPalette::getExposureReduce(), NesPalette::getContrastBoost());
}

void NesDisplayAdapter::clearBorders() {
    if (!tft) return;
    tft->setRotation(0); // 128x160 Portrait
    // Top 20px black letterbox
    tft->fillRect(0, 0, 128, 20, 0x0000);
    // Bottom 20px black letterbox
    tft->fillRect(0, 140, 128, 20, 0x0000);
}

void NesDisplayAdapter::startFrame() {
    if (!tft) return;
    tft->setRotation(0);
    tft->startWrite();
    // Single address window setup for the whole frame (128x120 active area)
    tft->setAddrWindow(0, 20, 128, 120);
    bufferedLines = 0;
    inFrame = true;
}

void NesDisplayAdapter::renderScanlineIndexed(int scanline, const uint8_t* nesIndices256, const uint16_t* palette64) {
    if (!tft || !inFrame || !nesIndices256) return;
    // 2:1 integer vertical downsampling: only render even scanlines (0, 2, ... 238)
    if ((scanline & 1) != 0 || scanline >= 240) return;

    // Guaranteed direct lookup from active enhanced palette
    const uint16_t* activePalette = NesPalette::getEnhancedPalette(0);

    uint32_t t0 = micros();

    // Direct 2:1 horizontal integer downscaling into burst buffer via direct palette LUT
    uint16_t* lineDest = &burstBuffer[bufferedLines * 128];
    for (int x = 0; x < 128; x++) {
        lineDest[x] = activePalette[nesIndices256[x * 2]];
    }
    bufferedLines++;

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
