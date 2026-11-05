#include "GbDisplayAdapter.h"
#include "config.h"
#include <Arduino.h>
#include <stdlib.h>
#include <string.h>

static Adafruit_ST7735* s_tft         = nullptr;
static uint8_t*         s_framebuffer = nullptr;
static bool             s_initialized = false;

#ifdef GB_DISPLAY_DEFAULT_PALETTE
static GbPaletteMode    s_paletteMode = (GbPaletteMode)GB_DISPLAY_DEFAULT_PALETTE;
#else
static GbPaletteMode    s_paletteMode = GB_PALETTE_HIGH_CONTRAST;
#endif

// Adapted palette storage for High-Contrast / DMG display modes (128 bytes in BSS)
static uint16_t         s_adaptedPalette[64];

// Multi-line burst buffer dynamically allocated from heap during init()
static uint16_t* s_burstBuffer = nullptr;

// Fallback Game Boy Pocket 4-shade grayscale palette (White, Light Gray, Dark Gray, Black)
static const uint16_t s_fallbackPalette[4] = {
    0xFFFF, // White
    0xAD55, // Light Gray
    0x52AA, // Dark Gray
    0x0000  // Black
};

// Telemetry counters
static uint32_t s_lastRenderTimeUs     = 0;
static uint32_t s_lastTftTimeUs        = 0;
static uint32_t s_totalFramesRendered  = 0;

bool GbDisplayAdapter::init(Adafruit_ST7735* display) {
    s_tft = display;

    // 1. Configure display hardware SPI to 40 MHz
    if (s_tft) {
        s_tft->setSPISpeed(40000000UL);
    }

#ifdef GB_DISPLAY_DEFAULT_PALETTE
    s_paletteMode = (GbPaletteMode)GB_DISPLAY_DEFAULT_PALETTE;
#else
    s_paletteMode = GB_PALETTE_HIGH_CONTRAST;
#endif

    // 2. Allocate multi-line burst buffer dynamically (8 lines * 128 px * 2 bytes = 2048 bytes)
    if (!s_burstBuffer) {
        s_burstBuffer = (uint16_t*)malloc(BURST_PIXELS * sizeof(uint16_t));
        if (!s_burstBuffer) {
            Serial.printf("[GbDisplayAdapter] ERROR: Failed to allocate %u-byte burst buffer!\n",
                          (unsigned)(BURST_PIXELS * sizeof(uint16_t)));
            return false;
        }
    }

    // 3. Allocate the single 160x144 8-bit framebuffer dynamically (23,040 bytes)
    if (!s_framebuffer) {
        s_framebuffer = (uint8_t*)calloc(1, GB_NATIVE_WIDTH * GB_NATIVE_HEIGHT);
        if (!s_framebuffer) {
            Serial.printf("[GbDisplayAdapter] ERROR: Failed to allocate %d-byte framebuffer!\n",
                          GB_NATIVE_WIDTH * GB_NATIVE_HEIGHT);
            if (s_burstBuffer) {
                free(s_burstBuffer);
                s_burstBuffer = nullptr;
            }
            return false;
        }
    }

    s_lastRenderTimeUs    = 0;
    s_lastTftTimeUs       = 0;
    s_totalFramesRendered = 0;
    s_initialized         = true;

    Serial.printf("[GbDisplayAdapter] Initialized | Viewport: %dx%d at (%d, %d) | Native Scanlines: 144 | Palette: %s\n",
                  TFT_TARGET_WIDTH, TFT_TARGET_HEIGHT, TFT_OFFSET_X, TFT_OFFSET_Y,
                  getPaletteModeName());
    return true;
}

void GbDisplayAdapter::deinit() {
    if (s_burstBuffer) {
        free(s_burstBuffer);
        s_burstBuffer = nullptr;
    }
    if (s_framebuffer) {
        free(s_framebuffer);
        s_framebuffer = nullptr;
    }
    s_tft = nullptr;
    s_initialized = false;
    Serial.println("[GbDisplayAdapter] Deinitialized & dynamic buffers released.");
}

bool GbDisplayAdapter::isInitialized() {
    return s_initialized && (s_framebuffer != nullptr) && (s_burstBuffer != nullptr);
}

uint8_t* GbDisplayAdapter::getFramebuffer() {
    return s_framebuffer;
}

size_t GbDisplayAdapter::getFramebufferSize() {
    return (size_t)(GB_NATIVE_WIDTH * GB_NATIVE_HEIGHT);
}

void GbDisplayAdapter::clearBorders() {
    if (!s_tft) return;
    s_tft->setRotation(0); // 128x160 portrait

    // Top letterbox band: 8 pixels tall (Y=0..7)
    s_tft->fillRect(0, 0, TFT_TOTAL_WIDTH, TFT_OFFSET_Y, 0x0000);

    // Bottom letterbox band: 8 pixels tall (Y=152..159)
    constexpr int bottomY = TFT_OFFSET_Y + TFT_TARGET_HEIGHT; // 8 + 144 = 152
    constexpr int bottomH = TFT_TOTAL_HEIGHT - bottomY;       // 160 - 152 = 8
    s_tft->fillRect(0, bottomY, TFT_TOTAL_WIDTH, bottomH, 0x0000);
}

void GbDisplayAdapter::setPaletteMode(GbPaletteMode mode) {
    s_paletteMode = mode;
}

GbPaletteMode GbDisplayAdapter::getPaletteMode() {
    return s_paletteMode;
}

const char* GbDisplayAdapter::getPaletteModeName() {
    switch (s_paletteMode) {
        case GB_PALETTE_ORIGINAL:      return "Original GBC (Red Tint)";
        case GB_PALETTE_HIGH_CONTRAST: return "High-Contrast Monochrome (Crisp B&W)";
        case GB_PALETTE_DMG_GREEN:     return "Classic DMG Green";
        default:                       return "Unknown";
    }
}

void GbDisplayAdapter::renderFrame(const uint8_t* gbFrame160x144, const uint16_t* palette64) {
    if (!s_tft || !gbFrame160x144 || !s_burstBuffer) return;

    const uint16_t* inPal = palette64 ? palette64 : s_fallbackPalette;
    const uint16_t* pal = inPal;

    // 1. Apply selected display palette mode
    if (s_paletteMode == GB_PALETTE_HIGH_CONTRAST) {
        for (int i = 0; i < 64; i++) {
            uint16_t c = inPal[i];
            uint32_t lum = (((c >> 11) & 0x1F) * 2) + (((c >> 5) & 0x3F) * 3) + (c & 0x1F);
            if (lum >= 200) {
                s_adaptedPalette[i] = 0xFFFF; // Pure White (#FFFFFF)
            } else if (lum >= 120) {
                s_adaptedPalette[i] = 0xBDF7; // Crisp Light Gray (75%)
            } else if (lum >= 50) {
                s_adaptedPalette[i] = 0x4208; // Dark Charcoal (25%)
            } else {
                s_adaptedPalette[i] = 0x0000; // Pure Black (#000000)
            }
        }
        pal = s_adaptedPalette;
    } else if (s_paletteMode == GB_PALETTE_DMG_GREEN) {
        for (int i = 0; i < 64; i++) {
            uint16_t c = inPal[i];
            uint32_t lum = (((c >> 11) & 0x1F) * 2) + (((c >> 5) & 0x3F) * 3) + (c & 0x1F);
            if (lum >= 200) {
                s_adaptedPalette[i] = 0x9E66; // DMG Lightest (#9BBC0F)
            } else if (lum >= 120) {
                s_adaptedPalette[i] = 0x8DE1; // DMG Light (#8BAC0F)
            } else if (lum >= 50) {
                s_adaptedPalette[i] = 0x3344; // DMG Dark (#306230)
            } else {
                s_adaptedPalette[i] = 0x09E1; // DMG Darkest (#0F380F)
            }
        }
        pal = s_adaptedPalette;
    }
    // When s_paletteMode == GB_PALETTE_ORIGINAL:
    // pal remains pointing to inPal directly (original GBC colorization).

    // 2. Precompute 64-entry luminance table for stroke-preserving horizontal filter
    uint8_t palLum[64];
    for (int i = 0; i < 64; i++) {
        uint16_t c = pal[i];
        palLum[i] = (uint8_t)((((c >> 11) & 0x1F) * 2) + (((c >> 5) & 0x3F) * 3) + (c & 0x1F));
    }

    uint32_t renderTimeTotal = 0;
    uint32_t tftTimeTotal    = 0;

    s_tft->setRotation(0);
    s_tft->startWrite();

    // Set destination active window on ST7735: X=0..127, Y=8..151 (128x144)
    s_tft->setAddrWindow(TFT_OFFSET_X, TFT_OFFSET_Y, TFT_TARGET_WIDTH, TFT_TARGET_HEIGHT);

    int bufferedLines = 0;

    for (int y = 0; y < TFT_TARGET_HEIGHT; y++) {
        uint32_t r0 = micros();

        // 1:1 Direct Scanline Mapping (no vertical decimation, all 144 lines rendered)
        const uint8_t* srcScanline = gbFrame160x144 + (y * GB_NATIVE_WIDTH);
        uint16_t* lineDest = &s_burstBuffer[bufferedLines * TFT_TARGET_WIDTH];

        // Stroke-preserving horizontal decimation (160 px -> 128 px):
        // 32 groups of 5 source pixels map to 4 destination pixels.
        // For the shared middle pixel D2, select the darker pixel between S2 and S3
        // to preserve 1-pixel font stems and punctuation.
        for (int g = 0; g < 32; g++) {
            int s = g * 5;
            int d = g * 4;

            uint8_t s0 = srcScanline[s + 0] & 0x3F;
            uint8_t s1 = srcScanline[s + 1] & 0x3F;
            uint8_t s2 = srcScanline[s + 2] & 0x3F;
            uint8_t s3 = srcScanline[s + 3] & 0x3F;
            uint8_t s4 = srcScanline[s + 4] & 0x3F;

            lineDest[d + 0] = pal[s0];
            lineDest[d + 1] = pal[s1];
            lineDest[d + 2] = pal[(palLum[s2] < palLum[s3]) ? s2 : s3];
            lineDest[d + 3] = pal[s4];
        }

        bufferedLines++;
        uint32_t r1 = micros();
        renderTimeTotal += (r1 - r0);

        // When the burst buffer reaches BURST_LINES (8 lines), flush over 40 MHz SPI
        if (bufferedLines >= BURST_LINES) {
            uint32_t s0 = micros();
            s_tft->writePixels(s_burstBuffer, BURST_PIXELS);
            uint32_t s1 = micros();
            tftTimeTotal += (s1 - s0);
            bufferedLines = 0;
        }
    }

    // Flush any remaining buffered scanlines (144 % 8 == 0, guarded for robustness)
    if (bufferedLines > 0) {
        uint32_t s0 = micros();
        s_tft->writePixels(s_burstBuffer, bufferedLines * TFT_TARGET_WIDTH);
        uint32_t s1 = micros();
        tftTimeTotal += (s1 - s0);
        bufferedLines = 0;
    }

    s_tft->endWrite();

    s_lastRenderTimeUs = renderTimeTotal;
    s_lastTftTimeUs    = tftTimeTotal;
    s_totalFramesRendered++;
}

uint32_t GbDisplayAdapter::getLastRenderTimeUs() {
    return s_lastRenderTimeUs;
}

uint32_t GbDisplayAdapter::getLastTftTimeUs() {
    return s_lastTftTimeUs;
}

uint32_t GbDisplayAdapter::getTotalFramesRendered() {
    return s_totalFramesRendered;
}

void GbDisplayAdapter::resetMetrics() {
    s_lastRenderTimeUs    = 0;
    s_lastTftTimeUs       = 0;
    s_totalFramesRendered = 0;
}
