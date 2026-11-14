#ifndef FOCUS_TIMER_APP_H
#define FOCUS_TIMER_APP_H

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>

// Playback interval for 30 frames: ~100 ms per frame gives ~3.0s total per sequence
#define FOCUS_TIMER_ANIM_INTERVAL_MS 100

enum FocusTimerState {
    TIMER_STATE_SELECTION = 0,
    TIMER_STATE_RUNNING   = 1,
    TIMER_STATE_PAUSED    = 2,
    TIMER_STATE_COMPLETED = 3
};

enum TimerField {
    FIELD_HOURS   = 0,
    FIELD_MINUTES = 1,
    FIELD_SECONDS = 2
};

class FocusTimerApp {
public:
    // Called once when entering the Focus Timer application
    static void init(Adafruit_ST7735 &tft);

    // Called on each loop() iteration while Focus Timer is active
    static void update(Adafruit_ST7735 &tft);

    // Called when exiting Focus Timer to stop playback and clean up
    static void stop();

    // Returns true when the user requests an exit (e.g. Back button)
    static bool shouldExit();

    // Returns the current frame index (0..29)
    static int getCurrentFrame();

    // Renders a specific frame (0..29) directly to the TFT
    static void renderFrame(Adafruit_ST7735 &tft, int frameIndex);

    // Renders the foreground timer HUD overlay on top of animation pixels
    static void drawOverlayUI(Adafruit_ST7735 &tft);

    // Getters for current state and remaining seconds
    static FocusTimerState getState();
    static uint32_t getRemainingSeconds();
};

#endif // FOCUS_TIMER_APP_H
