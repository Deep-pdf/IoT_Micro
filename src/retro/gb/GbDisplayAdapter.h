#ifndef GB_DISPLAY_ADAPTER_H
#define GB_DISPLAY_ADAPTER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>

// Selectable Game Boy Display Palette Modes
enum GbPaletteMode {
    GB_PALETTE_ORIGINAL      = 0, // Original Pokémon Red GBC colorization (red/sepia tint)
    GB_PALETTE_HIGH_CONTRAST = 1, // High-contrast Game Boy Pocket monochrome (crisp B&W)
    GB_PALETTE_DMG_GREEN     = 2  // Classic DMG retro green
};

class GbDisplayAdapter {
public:
    // Physical display and emulator dimension constants
    static constexpr int GB_NATIVE_WIDTH   = 160;
    static constexpr int GB_NATIVE_HEIGHT  = 144;
    static constexpr int TFT_TARGET_WIDTH  = 128;
    static constexpr int TFT_TARGET_HEIGHT = 144; // 1:1 Native vertical scanlines (zero decimation)
    static constexpr int TFT_OFFSET_X      = 0;
    static constexpr int TFT_OFFSET_Y      = 8;   // Centered letterbox: (160 - 144) / 2 = 8
    static constexpr int TFT_TOTAL_WIDTH   = 128;
    static constexpr int TFT_TOTAL_HEIGHT  = 160;

    // Multi-line SPI burst transfer size (8 lines * 128 px * 2 bytes = 2048 bytes)
    static constexpr int BURST_LINES       = 8;
    static constexpr int BURST_PIXELS      = TFT_TARGET_WIDTH * BURST_LINES;

    // Initializes adapter, sets 40 MHz SPI, configures palette mode,
    // and allocates the single 160x144 8-bit framebuffer (23,040 bytes).
    static bool init(Adafruit_ST7735* display);

    // Releases the dynamic 23,040-byte framebuffer and clears references.
    static void deinit();

    // Returns true if display adapter has an active framebuffer allocated.
    static bool isInitialized();

    // Direct access to the 160x144 8-bit framebuffer owned by this adapter.
    static uint8_t* getFramebuffer();
    static size_t getFramebufferSize();

    // Clears the 8px top and 8px bottom letterbox bands to solid black.
    static void clearBorders();

    // Palette Mode Management
    static void setPaletteMode(GbPaletteMode mode);
    static GbPaletteMode getPaletteMode();
    static const char* getPaletteModeName();

    // Renders one completed 160x144 8-bit Game Boy frame to ST7735:
    // 1. Applies stroke-preserving horizontal decimation filter (160 -> 128).
    // 2. Maps all 144 native Game Boy scanlines 1:1 directly to ST7735 Y=8..151.
    // 3. Translates palette according to active GbPaletteMode.
    // 4. Sends chunks to ST7735 via hardware SPI burst transfers.
    static void renderFrame(const uint8_t* gbFrame160x144, const uint16_t* palette64);

    // Frame performance telemetry (in microseconds)
    static uint32_t getLastRenderTimeUs();
    static uint32_t getLastTftTimeUs();
    static uint32_t getTotalFramesRendered();
    static void resetMetrics();
};

#endif // GB_DISPLAY_ADAPTER_H
