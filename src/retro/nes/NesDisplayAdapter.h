#ifndef NES_DISPLAY_ADAPTER_H
#define NES_DISPLAY_ADAPTER_H

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>

class NesDisplayAdapter {
public:
    // Bind existing Adafruit_ST7735 display instance
    static void init(Adafruit_ST7735* display);

    // Clears the top and bottom 20px letterbox bands to pure black
    static void clearBorders();

    // Sets up the 128x120 active window on ST7735 (Y=20..139)
    static void startFrame();

    // High-performance indexed scanline callback:
    // Receives 256 NES palette indices and 64-entry RGB565 palette
    // Performs integer 2:1 downsampling directly into a multi-line burst buffer
    static void renderScanlineIndexed(int scanline, const uint8_t* nesIndices256, const uint16_t* palette64);

    // Legacy fallback scanline callback
    static void renderScanline(int scanline, const uint16_t* nesScanline256);

    // Completes the frame write (flushes any remaining buffered lines)
    static void endFrame();

    // Performance metrics (per-frame microseconds)
    static uint32_t getRenderTimeUs();
    static uint32_t getTftTimeUs();
    static void resetFrameMetrics();
};

#endif // NES_DISPLAY_ADAPTER_H
