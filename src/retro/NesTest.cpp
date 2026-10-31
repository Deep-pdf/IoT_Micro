#include "NesTest.h"
#include "nes/NesBridge.h"
#include "nes/NesDisplayAdapter.h"
#include "nes/NesRom.h"
#include "nes/NesPalette.h"
#include "joystick.h"

#include <Arduino.h>
#include <Adafruit_ST7735.h>

// ---- Pin aliases (match config.h / button.h) ----
#define NESTEST_BTN_A     13   // GPIO 13 push button → NES A
#define NESTEST_BTN_START 25   // GPIO 25 push button → NES Start (long hold = exit test)

// NES controller bit positions (standard 8-bit shift register order)
#define NES_A      0x01
#define NES_B      0x02
#define NES_SELECT 0x04
#define NES_START  0x08
#define NES_UP     0x10
#define NES_DOWN   0x20
#define NES_LEFT   0x40
#define NES_RIGHT  0x80

// ---- Module state ----
static bool          s_initialized      = false;
static bool          s_exitRequested    = false;
static bool          s_startHeld        = false;
static unsigned long s_startHoldMs      = 0;
static unsigned long s_frameCount       = 0;
static unsigned long s_lastFpsPrint     = 0;
static unsigned long s_totalFrameTimeUs = 0;
static unsigned long s_totalEmuTimeUs   = 0;
static unsigned long s_totalRendTimeUs  = 0;
static unsigned long s_totalTftTimeUs   = 0;

// Build the NES controller byte from global fast joystick state
static uint8_t readNesController() {
    Joystick.update();
    uint8_t ctrl = 0x00;

    // Fast, responsive directions from global driver
    if (Joystick.isUp())    ctrl |= NES_UP;
    if (Joystick.isDown())  ctrl |= NES_DOWN;
    if (Joystick.isLeft())  ctrl |= NES_LEFT;
    if (Joystick.isRight()) ctrl |= NES_RIGHT;

    // Buttons: INPUT_PULLUP → active LOW
    if (digitalRead(NESTEST_BTN_A)     == LOW) ctrl |= NES_A;
    if (Joystick.isPressed())                 ctrl |= NES_B; // GPIO 32 click
    if (digitalRead(NESTEST_BTN_START) == LOW) ctrl |= NES_START;

    return ctrl;
}

// Display diagnostic mode shown before the emulator starts
// Tests: RGB565 byte order, primary colors, NES palette, 2x2 sharpness checkerboard, and active bounds
static void drawSplash(Adafruit_ST7735& tft) {
    tft.setRotation(0);
    tft.fillScreen(0x0000);
    tft.setFont(nullptr);
    tft.setTextWrap(false);

    // Active area boundary rectangle: (0, 20, 128, 120)
    tft.drawRect(0, 20, 128, 120, 0x7BEF);

    NesPalette::init();
    const uint16_t* origPal = NesPalette::getOriginalPalette(0);
    const uint16_t* enhPal  = NesPalette::getEnhancedPalette(0);

    // Representative 9 colors: Red, Green, Blue, Yellow, Sky Blue, Brown, Dark Gray, Light Gray, White
    static const uint8_t repIndices[9] = {
        0x16, // Red (Mario)
        0x2A, // Green (Bush / Pipe)
        0x12, // Blue (Deep)
        0x27, // Yellow (Coin / Block)
        0x21, // Sky Blue
        0x17, // Brown (Terrain)
        0x00, // Dark Gray
        0x10, // Light Gray
        0x20  // White
    };

    // Header
    tft.setTextColor(0x07E0); // Green
    tft.setTextSize(1);
    tft.setCursor(4, 23);
    tft.print("NES PALETTE ENHANCED");

    // Row 1: Original NES swatches
    tft.setTextColor(0xAD55); // Gray label
    tft.setCursor(4, 35);
    tft.print("ORG");
    int startX = 26;
    for (int i = 0; i < 9; i++) {
        tft.fillRect(startX + i * 11, 33, 10, 9, origPal[repIndices[i]]);
    }

    // Row 2: Enhanced NES swatches (direct vertical comparison)
    tft.setTextColor(0x07FF); // Cyan label
    tft.setCursor(4, 47);
    tft.print("ENH");
    for (int i = 0; i < 9; i++) {
        tft.fillRect(startX + i * 11, 45, 10, 9, enhPal[repIndices[i]]);
    }

    // Swatch indicator initials
    tft.setTextColor(0x7BEF); // Mid gray
    tft.setCursor(startX + 2, 57);
    tft.print("R  G  B  Y  S  Br Dk Lt W");

    // 2-pixel fine checkerboard (pixel alignment & sharpness check)
    for (int y = 67; y < 75; y += 2) {
        for (int x = 4; x < 124; x += 4) {
            tft.drawPixel(x, y, 0xFFFF);
            tft.drawPixel(x + 1, y, 0xFFFF);
            tft.drawPixel(x + 2, y + 1, 0xFFFF);
            tft.drawPixel(x + 3, y + 1, 0xFFFF);
        }
    }

    // Diagnostic information
    tft.setTextColor(0x07E0); // Green
    tft.setCursor(4, 78);
    tft.printf("Sat:+%d%% Con:+%d%% Exp:-%d%%", NesPalette::getSaturationBoost(), NesPalette::getContrastBoost(), NesPalette::getExposureReduce());

    tft.setTextColor(0xFFFF);
    tft.setCursor(4, 90);
    tft.print("ROM: Mario Bros 3");
    tft.setCursor(4, 102);
    tft.printf("Joy Cal: %d,%d", Joystick.getCenterX(), Joystick.getCenterY());
    tft.setCursor(4, 114);
    tft.print("SPI: 40MHz  LUT: 64xRGB");

    tft.setTextColor(0xFD20); // Orange
    tft.setCursor(4, 126);
    tft.print("STARTING...");

    delay(2000);
}

// ============================================================
// Public API
// ============================================================

void NesTest::init(Adafruit_ST7735& tft) {
    s_initialized      = false;
    s_exitRequested    = false;
    s_startHeld        = false;
    s_frameCount       = 0;
    s_lastFpsPrint     = millis();
    s_totalFrameTimeUs = 0;
    s_totalEmuTimeUs   = 0;
    s_totalRendTimeUs  = 0;
    s_totalTftTimeUs   = 0;

    // Initialize global joystick system
    Joystick.begin();

    // Re-initialize buttons
    pinMode(NESTEST_BTN_A,     INPUT_PULLUP);
    pinMode(NESTEST_BTN_START, INPUT_PULLUP);

    Serial.println();
    Serial.println("=== NES OPTIMIZED TEST START ===");
    Serial.println("ROM: Super Mario Bros. 3");
    Serial.println("ROM SIZE: 393232");
    Serial.println("MAPPER: 4 (MMC3)");
    Serial.println("DISPLAY: 128x120 active on 128x160 ST7735");
    Serial.printf("JOYSTICK: Center=(%d,%d) Thresh=+/-d\n",
                  Joystick.getCenterX(), Joystick.getCenterY(), Joystick.getActiveThreshold());
    Serial.println("Controls: JOY=DPAD, GPIO13=A, GPIO32=B, GPIO25=Start (hold 2s=exit)");

    drawSplash(tft);

    // Bind display adapter and init emulator
    NesBridge::init(&tft);

    const uint16_t* orig = NesPalette::getOriginalPalette(0);
    const uint16_t* enh  = NesPalette::getEnhancedPalette(0);
    Serial.printf("[NesPalette] Swatches: Red 0x%04X->0x%04X | Sky 0x%04X->0x%04X | Grn 0x%04X->0x%04X | Yel 0x%04X->0x%04X\n",
                  orig[0x16], enh[0x16],
                  orig[0x21], enh[0x21],
                  orig[0x2A], enh[0x2A],
                  orig[0x27], enh[0x27]);

    bool loaded = NesBridge::loadGame(NES_GAME_MARIO);
    if (!loaded) {
        Serial.println("[NesTest] ERROR: loadGame(NES_GAME_MARIO) FAILED");
        tft.setRotation(0);
        tft.fillScreen(0x0000);
        tft.setTextColor(0xF800);
        tft.setCursor(4, 40);
        tft.print("ROM LOAD FAILED");
        tft.setCursor(4, 54);
        tft.print("See serial log");
        s_exitRequested = true;
        return;
    }

    Serial.println("[NesTest] ROM loaded OK — starting emulation with fast global input");
    s_initialized = true;
}

void NesTest::update(Adafruit_ST7735& tft) {
    if (!s_initialized || s_exitRequested) return;

    // --- Controller input ---
    uint8_t ctrl = readNesController();
    NesBridge::setController(ctrl);

    // --- Exit: hold Start button for 2+ seconds ---
    bool startPressed = (ctrl & NES_START) != 0;
    if (startPressed && !s_startHeld) {
        s_startHoldMs = millis();
        s_startHeld   = true;
    } else if (!startPressed) {
        s_startHeld = false;
    } else if (startPressed && s_startHeld) {
        if ((millis() - s_startHoldMs) > 2000UL) {
            Serial.println("[NesTest] Long-hold exit → returning to launcher");
            s_exitRequested = true;
            NesBridge::stop();
            return;
        }
    }

    // --- Run one NES frame with microsecond stage profiling ---
    NesDisplayAdapter::resetFrameMetrics();

    unsigned long t0 = micros();
    NesBridge::runFrame();
    unsigned long t1 = micros();

    unsigned long frameTimeUs = t1 - t0;
    unsigned long rendTimeUs  = NesDisplayAdapter::getRenderTimeUs();
    unsigned long tftTimeUs   = NesDisplayAdapter::getTftTimeUs();
    unsigned long emuTimeUs   = (frameTimeUs > (rendTimeUs + tftTimeUs)) ? (frameTimeUs - (rendTimeUs + tftTimeUs)) : 0;

    s_totalFrameTimeUs += frameTimeUs;
    s_totalRendTimeUs  += rendTimeUs;
    s_totalTftTimeUs   += tftTimeUs;
    s_totalEmuTimeUs   += emuTimeUs;
    s_frameCount++;

    // --- FPS and timing breakdown report every 3 seconds ---
    unsigned long now = millis();
    if (now - s_lastFpsPrint >= 3000UL) {
        unsigned long elapsed = now - s_lastFpsPrint;
        float fps = (elapsed > 0) ? ((float)s_frameCount * 1000.0f / (float)elapsed) : 0.0f;
        float avgFrameMs = (s_frameCount > 0) ? ((float)s_totalFrameTimeUs / (float)s_frameCount / 1000.0f) : 0.0f;
        float avgEmuMs   = (s_frameCount > 0) ? ((float)s_totalEmuTimeUs   / (float)s_frameCount / 1000.0f) : 0.0f;
        float avgRendMs  = (s_frameCount > 0) ? ((float)s_totalRendTimeUs  / (float)s_frameCount / 1000.0f) : 0.0f;
        float avgTftMs   = (s_frameCount > 0) ? ((float)s_totalTftTimeUs   / (float)s_frameCount / 1000.0f) : 0.0f;

        Serial.print("[NesTest] FPS: ");
        Serial.print(fps, 1);
        Serial.print(" | Frame: ");
        Serial.print(avgFrameMs, 1);
        Serial.print("ms | Emu: ");
        Serial.print(avgEmuMs, 1);
        Serial.print("ms | Render: ");
        Serial.print(avgRendMs, 1);
        Serial.print("ms | TFT: ");
        Serial.print(avgTftMs, 1);
        Serial.printf("ms | Joy: (%d,%d) [L:%d R:%d U:%d D:%d]\n",
                      Joystick.getLogicalX(), Joystick.getLogicalY(),
                      Joystick.isLeft(), Joystick.isRight(), Joystick.isUp(), Joystick.isDown());

        s_frameCount       = 0;
        s_totalFrameTimeUs = 0;
        s_totalEmuTimeUs   = 0;
        s_totalRendTimeUs  = 0;
        s_totalTftTimeUs   = 0;
        s_lastFpsPrint     = now;
    }
}

bool NesTest::shouldExit() {
    return s_exitRequested;
}
