#include "NesDisplayAdapter.h"

static Adafruit_ST7735* tft = nullptr;

// Line buffer: exactly 128 pixels * 2 bytes = 256 bytes in SRAM (no 120 KB framebuffer!)
static uint16_t lineBuffer[128];
static bool inFrame = false;

void NesDisplayAdapter::init(Adafruit_ST7735* display) {
    tft = display;
    inFrame = false;
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
    // NES active area centered vertically: Y from 20 to 139 (height 120)
    tft->setAddrWindow(0, 20, 128, 120);
    inFrame = true;
}

void NesDisplayAdapter::renderScanline(int scanline, const uint16_t* nesScanline256) {
    if (!tft || !inFrame || !nesScanline256) return;

    // Integer 2:1 downscaling: only render even scanlines (0, 2, 4, ... 238)
    if ((scanline & 1) != 0 || scanline >= 240) return;

    // Downscale 256 horizontal pixels to 128 pixels by taking every 2nd pixel
    for (int x = 0; x < 128; x++) {
        lineBuffer[x] = nesScanline256[x * 2];
    }

    tft->writePixels(lineBuffer, 128);
}

void NesDisplayAdapter::endFrame() {
    if (!tft || !inFrame) return;
    tft->endWrite();
    inFrame = false;
}

