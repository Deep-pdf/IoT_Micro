#ifndef NES_DISPLAY_ADAPTER_H
#define NES_DISPLAY_ADAPTER_H

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>

enum NesDisplayMode : uint8_t {
    NES_DISPLAY_HYBRID = 0, // Top 128x120 Full 4:3 Game View + Divider + Bottom 39-line 1:1 Pixel-Perfect Viewport
    NES_DISPLAY_FULL   = 1  // Centered 128x120 Full 4:3 Game View (20px top & bottom letterbox)
};

class NesDisplayAdapter {
public:
    // Bind existing Adafruit_ST7735 display instance
    static void init(Adafruit_ST7735* display);

    // Display mode control & NVS persistence
    static void setDisplayMode(NesDisplayMode mode);
    static NesDisplayMode getDisplayMode();
    static NesDisplayMode loadSavedDisplayMode();
    static void saveDisplayMode(NesDisplayMode mode);

    // Clears letterbox bands / borders when needed
    static void clearBorders();

    // Sets up the active window on ST7735 (Full: Y=20..139, Hybrid: Y=0..159)
    static void startFrame();

    // High-performance indexed scanline callback:
    // Receives 256 NES palette indices and 64-entry RGB565 palette
    // Performs integer 2:1 downsampling directly into a multi-line burst buffer
    // and caches 1:1 detail region in Hybrid mode
    static void renderScanlineIndexed(int scanline, const uint8_t* nesIndices256, const uint16_t* palette64);

    // Legacy fallback scanline callback
    static void renderScanline(int scanline, const uint16_t* nesScanline256);

    // Completes the frame write (flushes remaining lines + Hybrid 1:1 detail region)
    static void endFrame();

    // Performance metrics (per-frame microseconds)
    static uint32_t getRenderTimeUs();
    static uint32_t getTftTimeUs();
    static void resetFrameMetrics();
};

#endif // NES_DISPLAY_ADAPTER_H
