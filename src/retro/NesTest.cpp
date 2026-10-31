#include "NesTest.h"
#include "nes/NesBridge.h"
#include "nes/NesRom.h"

#include <Arduino.h>
#include <Adafruit_ST7735.h>

// ---- Pin aliases (match config.h / joystick.h) ----
#define NESTEST_JOY_X     34   // ADC raw X axis
#define NESTEST_JOY_Y     35   // ADC raw Y axis
#define NESTEST_JOY_SW    32   // Joystick click → NES B
#define NESTEST_BTN_A     13   // GPIO 13 push button → NES A
#define NESTEST_BTN_START 25   // GPIO 25 push button → NES Start (long hold = exit test)

// Joystick thresholds (matching main.cpp joystick.h rotation mapping)
// Physical 90° CW rotation:  logical X = 4095 - rawY,  logical Y = rawX
#define JOY_THRESH_LO  1000
#define JOY_THRESH_HI  3000

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
static bool          s_initialized   = false;
static bool          s_exitRequested = false;
static bool          s_startHeld     = false;
static unsigned long s_startHoldMs   = 0;
static unsigned long s_frameCount    = 0;
static unsigned long s_lastFpsPrint  = 0;

// ---- Joystick: apply rotation mapping from joystick.h ----
static inline int joy_logicalX() {
    int v = 4095 - (int)analogRead(NESTEST_JOY_Y);
    return v < 0 ? 0 : (v > 4095 ? 4095 : v);
}
static inline int joy_logicalY() {
    int v = (int)analogRead(NESTEST_JOY_X);
    return v < 0 ? 0 : (v > 4095 ? 4095 : v);
}

// Build the NES controller byte from current hardware state
static uint8_t readNesController() {
    uint8_t ctrl = 0x00;

    int lx = joy_logicalX();
    int ly = joy_logicalY();

    // Prevent simultaneous UP+DOWN / LEFT+RIGHT by reading largest deflection only
    if (ly < JOY_THRESH_LO)       ctrl |= NES_UP;
    else if (ly > JOY_THRESH_HI)  ctrl |= NES_DOWN;

    if (lx < JOY_THRESH_LO)       ctrl |= NES_LEFT;
    else if (lx > JOY_THRESH_HI)  ctrl |= NES_RIGHT;

    // Buttons: INPUT_PULLUP → active LOW
    if (digitalRead(NESTEST_BTN_A)     == LOW) ctrl |= NES_A;
    if (digitalRead(NESTEST_JOY_SW)    == LOW) ctrl |= NES_B;
    if (digitalRead(NESTEST_BTN_START) == LOW) ctrl |= NES_START;

    return ctrl;
}

// Brief diagnostic splash shown before the emulator starts
static void drawSplash(Adafruit_ST7735& tft) {
    tft.setRotation(0);
    tft.fillScreen(0x0000);
    tft.setFont(nullptr);  // use Adafruit default 5×7 font
    tft.setTextWrap(false);

    tft.setTextColor(0x07E0); // green
    tft.setTextSize(1);
    tft.setCursor(4, 16);
    tft.print("NES HARDWARE TEST");

    tft.setTextColor(0xFFFF);
    tft.setCursor(4, 36);
    tft.print("ROM: Mario Bros 3");
    tft.setCursor(4, 48);
    tft.print("Mapper 4 (MMC3)");
    tft.setCursor(4, 60);
    tft.print("128x160 ST7735");

    tft.setTextColor(0xFD20); // orange
    tft.setCursor(4, 80);
    tft.print("LOADING...");

    delay(1200);
}

// ============================================================
// Public API
// ============================================================

void NesTest::init(Adafruit_ST7735& tft) {
    s_initialized   = false;
    s_exitRequested = false;
    s_startHeld     = false;
    s_frameCount    = 0;
    s_lastFpsPrint  = millis();

    // Re-initialize pins (idempotent)
    pinMode(NESTEST_BTN_A,     INPUT_PULLUP);
    pinMode(NESTEST_BTN_START, INPUT_PULLUP);
    pinMode(NESTEST_JOY_SW,    INPUT_PULLUP);
    pinMode(NESTEST_JOY_X,     INPUT);
    pinMode(NESTEST_JOY_Y,     INPUT);

    Serial.println();
    Serial.println("=== NES TEST START ===");
    Serial.println("ROM: Super Mario Bros. 3");
    Serial.println("ROM SIZE: 393232");
    Serial.println("MAPPER: 4");
    Serial.println("DISPLAY: 128x160");
    Serial.println("Controls: JOY=DPAD, GPIO13=A, GPIO32=B, GPIO25=Start (hold 2s=exit)");

    drawSplash(tft);

    // Bind display adapter and init emulator
    NesBridge::init(&tft);

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

    Serial.println("[NesTest] ROM loaded OK — starting emulation");
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

    // --- Run one NES frame ---
    NesBridge::runFrame();
    s_frameCount++;

    // --- FPS report every 5 seconds (non-disruptive to timing) ---
    unsigned long now = millis();
    if (now - s_lastFpsPrint >= 5000UL) {
        unsigned long elapsed = now - s_lastFpsPrint;
        float fps = (elapsed > 0) ? ((float)s_frameCount * 1000.0f / (float)elapsed) : 0.0f;
        Serial.print("[NesTest] FPS: ");
        Serial.println(fps, 1);
        s_frameCount   = 0;
        s_lastFpsPrint = now;
    }
}

bool NesTest::shouldExit() {
    return s_exitRequested;
}
