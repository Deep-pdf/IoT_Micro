#include "RetroApp.h"
#include "RetroLauncher.h"
#include "RetroDisplay.h"
#include "RetroInput.h"
#include "nes/NesBridge.h"
#include "nes/NesDisplayAdapter.h"
#include "nes/NesRom.h"
#include "gb/GbBridge.h"
#include "joystick.h"
#include "button.h"
#include <Arduino.h>

#ifdef NES_HARDWARE_TEST
// ---- Standalone NES hardware test mode ----
#include "NesTest.h"

void RetroApp::init(Adafruit_ST7735 &tft) {
    NesTest::init(tft);
}

void RetroApp::update(Adafruit_ST7735 &tft) {
    NesTest::update(tft);
}

bool RetroApp::shouldExit() {
    return NesTest::shouldExit();
}

#else
// ---- Production Retro Arcade Launcher & NES Engine State Machine ----

enum RetroAppState {
    STATE_LAUNCHER,
    STATE_GAMEPLAY_NES,
    STATE_GAMEPLAY_GB
};

static RetroAppState s_state = STATE_LAUNCHER;
static NesGameId     s_activeGame = NES_GAME_NONE;
static bool          s_startHeld = false;
static unsigned long s_startHoldMs = 0;

// Diagnostics & profiling counters
static unsigned long s_frameCount       = 0;
static unsigned long s_lastFpsPrint     = 0;
static unsigned long s_totalFrameTimeUs = 0;
static unsigned long s_totalEmuTimeUs   = 0;
static unsigned long s_totalRendTimeUs  = 0;
static unsigned long s_totalTftTimeUs   = 0;

// Hardware pin mapping for NES controller emulation
#define NES_BTN_A     13   // GPIO 13 push button -> NES Button A
#define NES_BTN_START 25   // GPIO 25 push button -> NES Start (hold >2s to exit)

// NES 8-bit controller register bits
#define NES_A      0x01
#define NES_B      0x02
#define NES_SELECT 0x04
#define NES_START  0x08
#define NES_UP     0x10
#define NES_DOWN   0x20
#define NES_LEFT   0x40
#define NES_RIGHT  0x80

static uint8_t readNesController() {
    Joystick.update();
    uint8_t ctrl = 0x00;

    // Fast directional mapping via global joystick driver
    if (Joystick.isUp())    ctrl |= NES_UP;
    if (Joystick.isDown())  ctrl |= NES_DOWN;
    if (Joystick.isLeft())  ctrl |= NES_LEFT;
    if (Joystick.isRight()) ctrl |= NES_RIGHT;

    // Push buttons (INPUT_PULLUP -> active LOW)
    if (digitalRead(NES_BTN_A)     == LOW) ctrl |= NES_A;
    if (Joystick.isPressed())             ctrl |= NES_B; // GPIO 32 joystick click -> NES Button B
    if (digitalRead(NES_BTN_START) == LOW) ctrl |= NES_START;

    return ctrl;
}

void RetroApp::init(Adafruit_ST7735 &tft) {
    s_state = STATE_LAUNCHER;
    s_activeGame = NES_GAME_NONE;
    s_startHeld = false;
    s_frameCount = 0;
    s_lastFpsPrint = millis();
    RetroLauncher::init(tft, true); // true = reset selection to Super Mario Bros. (Arcade#1)
}

void RetroApp::update(Adafruit_ST7735 &tft) {
    if (s_state == STATE_LAUNCHER) {
        RetroLauncher::update(tft);
        RetroLaunchIntent intent = RetroLauncher::getLaunchIntent();
        if (intent == RETRO_LAUNCH_MARIO || intent == RETRO_LAUNCH_CONTRA || intent == RETRO_LAUNCH_RAD_RACER) {
            RetroLauncher::clearLaunchIntent();
            NesGameId targetGame = (intent == RETRO_LAUNCH_MARIO) ? NES_GAME_MARIO :
                                   (intent == RETRO_LAUNCH_CONTRA) ? NES_GAME_CONTRA : NES_GAME_RAD_RACER;
            NesBridge::init(&tft);
            NesDisplayAdapter::loadSavedDisplayMode();
            NesDisplayAdapter::clearBorders();
            if (NesBridge::loadGame(targetGame)) {
                s_activeGame = targetGame;
                s_state = STATE_GAMEPLAY_NES;
                s_startHeld = false;
                s_frameCount = 0;
                s_lastFpsPrint = millis();
                s_totalFrameTimeUs = 0;
                s_totalEmuTimeUs = 0;
                s_totalRendTimeUs = 0;
                s_totalTftTimeUs = 0;
                const char* gameTitle = (targetGame == NES_GAME_MARIO) ? "Super Mario Bros. 3" :
                                        (targetGame == NES_GAME_CONTRA) ? "Contra" : "Rad Racer 2";
                Serial.printf("[RetroApp] Launched NES Game: %s | Display Mode: %s\n",
                              gameTitle,
                              (NesDisplayAdapter::getDisplayMode() == NES_DISPLAY_HYBRID) ? "HYBRID" : "FULL");
            }
        } else if (intent == RETRO_LAUNCH_POKEMON) {
            RetroLauncher::clearLaunchIntent();
            if (GbBridge::init(&tft)) {
                s_activeGame = NES_GAME_NONE;
                s_state = STATE_GAMEPLAY_GB;
                s_frameCount = 0;
                s_lastFpsPrint = millis();
                s_totalFrameTimeUs = 0;
                Serial.println("[RetroApp] Launched Game Boy: Pokémon Red");
            } else {
                Serial.println("[RetroApp] ERROR: Failed to launch Pokémon Red!");
                RetroLauncher::init(tft, false);
            }
        }
    } else if (s_state == STATE_GAMEPLAY_NES) {
        uint8_t ctrl = readNesController();
        NesBridge::setController(ctrl);

        // Exit gameplay: Hold Start (GPIO 25) for 2+ seconds
        bool startPressed = (ctrl & NES_START) != 0;
        if (startPressed && !s_startHeld) {
            s_startHoldMs = millis();
            s_startHeld = true;
        } else if (!startPressed) {
            s_startHeld = false;
        } else if (startPressed && s_startHeld) {
            if ((millis() - s_startHoldMs) > 2000UL) {
                Serial.println("[RetroApp] Long-hold exit -> returning to launcher");
                NesBridge::stop();

                // Wait for Start button release to avoid immediate launcher exit
                while (digitalRead(NES_BTN_START) == LOW) {
                    delay(10);
                }
                clearButtonEvents();

                s_state = STATE_LAUNCHER;
                s_activeGame = NES_GAME_NONE;
                s_startHeld = false;

                // Return to launcher preserving the selected game screen
                RetroLauncher::init(tft, false);
                return;
            }
        }

        // Run one NES frame with microsecond stage profiling
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

        // Diagnostic FPS & stage breakdown every 3 seconds to Serial
        unsigned long now = millis();
        if (now - s_lastFpsPrint >= 3000UL) {
            unsigned long elapsed = now - s_lastFpsPrint;
            float fps = (elapsed > 0) ? ((float)s_frameCount * 1000.0f / (float)elapsed) : 0.0f;
            float avgFrameMs = (s_frameCount > 0) ? ((float)s_totalFrameTimeUs / (float)s_frameCount / 1000.0f) : 0.0f;
            float avgEmuMs   = (s_frameCount > 0) ? ((float)s_totalEmuTimeUs   / (float)s_frameCount / 1000.0f) : 0.0f;
            float avgRendMs  = (s_frameCount > 0) ? ((float)s_totalRendTimeUs  / (float)s_frameCount / 1000.0f) : 0.0f;
            float avgTftMs   = (s_frameCount > 0) ? ((float)s_totalTftTimeUs   / (float)s_frameCount / 1000.0f) : 0.0f;

            const char* gameName = (s_activeGame == NES_GAME_MARIO) ? "Mario 3" :
                                   (s_activeGame == NES_GAME_CONTRA) ? "Contra" : "Rad Racer 2";
            Serial.printf("[RetroApp] Game: %s | Mode: %s | FPS: %.1f | Frame: %.1fms | Emu: %.1fms | Render: %.1fms | TFT: %.1fms\n",
                          gameName,
                          (NesDisplayAdapter::getDisplayMode() == NES_DISPLAY_HYBRID) ? "HYBRID" : "FULL",
                          fps, avgFrameMs, avgEmuMs, avgRendMs, avgTftMs);

            s_frameCount       = 0;
            s_totalFrameTimeUs = 0;
            s_totalEmuTimeUs   = 0;
            s_totalRendTimeUs  = 0;
            s_totalTftTimeUs   = 0;
            s_lastFpsPrint     = now;
        }
    } else if (s_state == STATE_GAMEPLAY_GB) {
        // Run one Game Boy frame
        unsigned long t0 = micros();
        GbBridge::runFrame();
        unsigned long frameTimeUs = micros() - t0;

        // Check exit gesture (BTN_BACK hold >= 2s)
        if (GbBridge::isExitRequested()) {
            Serial.println("[RetroApp] Game Boy exit triggered -> returning to launcher");
            GbBridge::stop();
            GbBridge::waitForExitRelease();

            s_state = STATE_LAUNCHER;

            // Return to launcher preserving selected game (Pokémon Red)
            RetroLauncher::init(tft, false);
            return;
        }

        s_totalFrameTimeUs += frameTimeUs;
        s_frameCount++;

        // Performance telemetry every 3 seconds to Serial
        unsigned long now = millis();
        if (now - s_lastFpsPrint >= 3000UL) {
            unsigned long elapsed = now - s_lastFpsPrint;
            float fps = (elapsed > 0) ? ((float)s_frameCount * 1000.0f / (float)elapsed) : 0.0f;
            float avgFrameMs = (s_frameCount > 0) ? ((float)s_totalFrameTimeUs / (float)s_frameCount / 1000.0f) : 0.0f;
            Serial.printf("[RetroApp] Game: Pokémon Red | FPS: %.1f | Frame: %.1fms | FreeHeap: %u B\n",
                          fps, avgFrameMs, ESP.getFreeHeap());
            s_frameCount = 0;
            s_totalFrameTimeUs = 0;
            s_lastFpsPrint = now;
        }
    }
}

bool RetroApp::shouldExit() {
    return (s_state == STATE_LAUNCHER) && RetroLauncher::shouldExit();
}

#endif
