#ifndef GB_INPUT_H
#define GB_INPUT_H

#include <Arduino.h>
#include <stdint.h>
#include <stdbool.h>

class GbInput {
public:
    // Initialize button GPIOs, pullups, and joystick ADC subsystem
    static void init();

    // Polls physical inputs, debounces buttons, updates joystick directions, and handles exit gesture.
    // Call once per emulated frame.
    static void update();

    // Returns the current 8-bit Gnuboy pad state bitmask (GB_PAD_*)
    static uint8_t getPadState();

    // Returns true if the safe exit gesture (BTN_BACK held >= 2000 ms) has triggered
    static bool isExitRequested();

    // Resets all debounced button states, timers, and Gnuboy pad
    static void reset();

    // Clean teardown and release of all input states
    static void deinit();

    // Waits until BTN_BACK is physically released, then clears button event queues
    static void waitForExitRelease();

    // Diagnostic accessors
    static int getJoystickX();
    static int getJoystickY();
    static int getCenterX();
    static int getCenterY();
    static int getActiveThreshold();
    static int getReleaseThreshold();
    static uint32_t getBackHoldMs();
};

#endif // GB_INPUT_H
