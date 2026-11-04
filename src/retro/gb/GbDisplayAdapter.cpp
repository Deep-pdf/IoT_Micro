#include "GbDisplayAdapter.h"
#include <Arduino.h>
#include <stdlib.h>
#include <string.h>

static Adafruit_ST7735* s_tft = nullptr;
static uint8_t*         s_framebuffer = nullptr;
static bool             s_initialized = false;

// Precomputed 2D nearest-neighbor scaling look-up tables in Flash (.rodata)
static const uint8_t s_xLut[GbDisplayAdapter::TFT_TARGET_WIDTH] PROGMEM = {
      0,   1,   3,   4,   5,   6,   8,   9,  10,  11,  13,  14,  15,  16,  18,  19,
     20,  21,  23,  24,  25,  26,  28,  29,  30,  31,  33,  34,  35,  36,  38,  39,
     40,  41,  43,  44,  45,  46,  48,  49,  50,  51,  53,  54,  55,  56,  58,  59,
     60,  61,  63,  64,  65,  66,  68,  69,  70,  71,  73,  74,  75,  76,  78,  79,
     80,  81,  83,  84,  85,  86,  88,  89,  90,  91,  93,  94,  95,  96,  98,  99,
    100, 101, 103, 104, 105, 106, 108, 109, 110, 111, 113, 114, 115, 116, 118, 119,
    120, 121, 123, 124, 125, 126, 128, 129, 130, 131, 133, 134, 135, 136, 138, 139,
    140, 141, 143, 144, 145, 146, 148, 149, 150, 151, 153, 154, 155, 156, 158, 159
};

static const uint8_t s_yLut[GbDisplayAdapter::TFT_TARGET_HEIGHT] PROGMEM = {
      0,   1,   3,   4,   5,   6,   8,   9,  10,  11,  13,  14,  15,  16,  18,  19,
     20,  21,  23,  24,  25,  26,  28,  29,  30,  31,  33,  34,  35,  36,  38,  39,
     40,  41,  43,  44,  45,  46,  48,  49,  50,  51,  53,  54,  55,  56,  58,  59,
     60,  61,  63,  64,  65,  66,  68,  69,  70,  71,  73,  74,  75,  76,  78,  79,
     80,  81,  83,  84,  85,  86,  88,  89,  90,  91,  93,  94,  95,  96,  98,  99,
    100, 101, 103, 104, 105, 106, 108, 109, 110, 111, 113, 114, 115, 116, 118, 119,
    120, 121, 123, 124, 125, 126, 128, 129, 130, 131, 133, 134, 135, 136, 138, 139,
    140, 141, 143
};

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

    // 2. Nearest-neighbor scaling LUTs are precomputed and stored in Flash (.rodata / PROGMEM)

    // 3. Allocate multi-line burst buffer dynamically (8 lines * 128 px * 2 bytes = 2048 bytes)
    if (!s_burstBuffer) {
        s_burstBuffer = (uint16_t*)malloc(BURST_PIXELS * sizeof(uint16_t));
        if (!s_burstBuffer) {
            Serial.printf("[GbDisplayAdapter] ERROR: Failed to allocate %u-byte burst buffer!\n",
                          (unsigned)(BURST_PIXELS * sizeof(uint16_t)));
            return false;
        }
    }

    // 4. Allocate the single 160x144 8-bit framebuffer dynamically (23,040 bytes)
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

    Serial.printf("[GbDisplayAdapter] Initialized | Viewport: %dx%d at (%d, %d) | Framebuffer: %d bytes | Burst: %d bytes\n",
                  TFT_TARGET_WIDTH, TFT_TARGET_HEIGHT, TFT_OFFSET_X, TFT_OFFSET_Y,
                  GB_NATIVE_WIDTH * GB_NATIVE_HEIGHT,
                  (int)(BURST_PIXELS * sizeof(uint16_t)));
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

    // Top letterbox band: 22 pixels tall (Y=0..21)
    s_tft->fillRect(0, 0, TFT_TOTAL_WIDTH, TFT_OFFSET_Y, 0x0000);

    // Bottom letterbox band: 23 pixels tall (Y=137..159)
    constexpr int bottomY = TFT_OFFSET_Y + TFT_TARGET_HEIGHT; // 22 + 115 = 137
    constexpr int bottomH = TFT_TOTAL_HEIGHT - bottomY;       // 160 - 137 = 23
    s_tft->fillRect(0, bottomY, TFT_TOTAL_WIDTH, bottomH, 0x0000);
}

void GbDisplayAdapter::renderFrame(const uint8_t* gbFrame160x144, const uint16_t* palette64) {
    if (!s_tft || !gbFrame160x144 || !s_burstBuffer) return;

    const uint16_t* pal = palette64 ? palette64 : s_fallbackPalette;

    uint32_t renderTimeTotal = 0;
    uint32_t tftTimeTotal    = 0;

    s_tft->setRotation(0);
    s_tft->startWrite();

    // Set destination active window on ST7735: X=0..127, Y=22..136 (128x115)
    s_tft->setAddrWindow(TFT_OFFSET_X, TFT_OFFSET_Y, TFT_TARGET_WIDTH, TFT_TARGET_HEIGHT);

    int bufferedLines = 0;

    for (int y = 0; y < TFT_TARGET_HEIGHT; y++) {
        uint32_t r0 = micros();
        const uint8_t* srcScanline = gbFrame160x144 + ((uint32_t)s_yLut[y] * GB_NATIVE_WIDTH);
        uint16_t* lineDest = &s_burstBuffer[bufferedLines * TFT_TARGET_WIDTH];

        // Horizontal scaling & color translation
        for (int x = 0; x < TFT_TARGET_WIDTH; x++) {
            lineDest[x] = pal[srcScanline[s_xLut[x]] & 0x3F];
        }
        bufferedLines++;
        uint32_t r1 = micros();
        renderTimeTotal += (r1 - r0);

        // When the burst buffer reaches BURST_LINES (8 lines), flush over SPI
        if (bufferedLines >= BURST_LINES) {
            uint32_t s0 = micros();
            s_tft->writePixels(s_burstBuffer, BURST_PIXELS);
            uint32_t s1 = micros();
            tftTimeTotal += (s1 - s0);
            bufferedLines = 0;
        }
    }

    // Flush any remaining buffered scanlines (115 % 8 = 3 lines)
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

