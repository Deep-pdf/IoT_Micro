#include "GbInput.h"
#include "core/gnuboy.h"
#include "config.h"
#include "joystick.h"
#include "button.h"
#include "retro/RetroSelectCombo.h"

#define GB_EXIT_HOLD_MS  2000UL // Continuous hold of BTN_BACK to trigger safe exit
#define GB_DEBOUNCE_MS     20UL // Release debounce filter to prevent mechanical chatter

struct DebouncedBtn {
    uint8_t  pin;
    bool     stable;
    bool     raw;
    uint32_t lastEdgeMs;
};

static DebouncedBtn s_btnA     = { (uint8_t)BTN_ENTER, false, false, 0 }; // GPIO 13 -> Game Boy A
static DebouncedBtn s_btnB     = { (uint8_t)HW_JOY_SW, false, false, 0 }; // GPIO 32 -> Game Boy B
static DebouncedBtn s_btnStart = { (uint8_t)BTN_BACK,  false, false, 0 }; // GPIO 25 -> Game Boy START

static uint8_t  s_padState        = 0;
static bool     s_exitRequested   = false;
static uint32_t s_backHoldStartMs = 0;
static bool     s_backHeld        = false;
static RetroSelectCombo s_gbCombo;

static void updateButtonState(DebouncedBtn& btn, uint32_t now) {
    bool rawPressed = (digitalRead(btn.pin) == LOW);
    if (rawPressed != btn.raw) {
        btn.raw = rawPressed;
        btn.lastEdgeMs = now;
    }

    if (rawPressed && !btn.stable) {
        // Immediate response on press: 0 ms latency
        btn.stable = true;
    } else if (!rawPressed && btn.stable) {
        // Debounced release filter: require stable HIGH for GB_DEBOUNCE_MS
        if ((now - btn.lastEdgeMs) >= GB_DEBOUNCE_MS) {
            btn.stable = false;
        }
    }
}

void GbInput::init() {
    pinMode(BTN_ENTER, INPUT_PULLUP);
    pinMode(BTN_BACK,  INPUT_PULLUP);
    pinMode(HW_JOY_SW, INPUT_PULLUP);

    // Ensure global joystick hardware calibration is performed
    Joystick.begin();

    reset();

    Serial.println("[GbInput] Initialized Game Boy Controls:");
    Serial.println("          D-Pad: Analog Joystick (GPIO 34/35)");
    Serial.println("          A:     ENTER Button (GPIO 13, Active LOW)");
    Serial.println("          B:     Joystick Click (GPIO 32, Active LOW)");
    Serial.println("          START: BACK Button (GPIO 25, Active LOW, Short Press)");
    Serial.println("          EXIT:  BACK Button (GPIO 25, Hold >= 2.0s)");
    Serial.println("          SELECT: GPIO 13 + GPIO 32 (Simultaneous Combo)");
    Serial.printf("[GbInput] Joystick Calibrated Center: (%d, %d) | Active: +/- %d, Release: +/- %d\n",
                  Joystick.getCenterX(), Joystick.getCenterY(),
                  Joystick.getActiveThreshold(), Joystick.getReleaseThreshold());
}

void GbInput::update() {
    uint32_t now = millis();

    // 1. Debounce physical pushbuttons
    updateButtonState(s_btnA, now);
    updateButtonState(s_btnB, now);
    updateButtonState(s_btnStart, now);

    // 2. Track BTN_BACK hold duration for safe exit gesture vs short START
    bool backPressed = s_btnStart.stable;
    if (backPressed && !s_backHeld) {
        s_backHeld = true;
        s_backHoldStartMs = now;
    } else if (!backPressed) {
        s_backHeld = false;
        s_backHoldStartMs = 0;
    } else if (backPressed && s_backHeld) {
        uint32_t holdDuration = now - s_backHoldStartMs;
        if (holdDuration >= GB_EXIT_HOLD_MS) {
            s_exitRequested = true;
        }
    }

    // 3. Update Joystick ADC and evaluate directional states
    Joystick.update();
    bool up    = Joystick.isUp();
    bool down  = Joystick.isDown();
    bool left  = Joystick.isLeft();
    bool right = Joystick.isRight();

    // 4. Assemble 8-bit Gnuboy pad bitmask
    uint8_t pad = 0;

    // Directional keys with strict opposite-direction exclusion
    // Simultaneous diagonal inputs (e.g. UP + RIGHT) are fully supported.
    if (up && !down)    pad |= GB_PAD_UP;
    if (down && !up)    pad |= GB_PAD_DOWN;
    if (left && !right) pad |= GB_PAD_LEFT;
    if (right && !left) pad |= GB_PAD_RIGHT;

    // Action buttons & SELECT combination (GPIO 13 + GPIO 32)
    bool actA = false;
    bool actB = false;
    bool actSelect = false;
    s_gbCombo.update(s_btnA.raw, s_btnB.raw, now, actA, actB, actSelect);

    if (actA)      pad |= GB_PAD_A;      // BTN_ENTER (GPIO 13)
    if (actB)      pad |= GB_PAD_B;      // Joystick click (GPIO 32)
    if (actSelect) pad |= GB_PAD_SELECT; // GPIO 13 + GPIO 32 simultaneous combo

    // START button (BTN_BACK / GPIO 25)
    // Send START while held prior to triggering the 2.0s exit threshold
    if (s_btnStart.stable && !s_exitRequested) {
        pad |= GB_PAD_START;
    }

    s_padState = pad;
}

uint8_t GbInput::getPadState() {
    return s_padState;
}

bool GbInput::isExitRequested() {
    return s_exitRequested;
}

void GbInput::reset() {
    s_btnA.stable = false;
    s_btnA.raw = (digitalRead(BTN_ENTER) == LOW);
    s_btnA.lastEdgeMs = millis();

    s_btnB.stable = false;
    s_btnB.raw = (digitalRead(HW_JOY_SW) == LOW);
    s_btnB.lastEdgeMs = millis();

    s_btnStart.stable = false;
    s_btnStart.raw = (digitalRead(BTN_BACK) == LOW);
    s_btnStart.lastEdgeMs = millis();

    s_backHeld = false;
    s_backHoldStartMs = 0;
    s_exitRequested = false;
    s_padState = 0;

    s_gbCombo.reset();

    gnuboy_set_pad(0);
}

void GbInput::deinit() {
    reset();
}

void GbInput::waitForExitRelease() {
    while (digitalRead(BTN_BACK) == LOW) {
        delay(10);
    }
    clearButtonEvents();
}

int GbInput::getJoystickX() {
    return Joystick.getLogicalX();
}

int GbInput::getJoystickY() {
    return Joystick.getLogicalY();
}

int GbInput::getCenterX() {
    return Joystick.getCenterX();
}

int GbInput::getCenterY() {
    return Joystick.getCenterY();
}

int GbInput::getActiveThreshold() {
    return Joystick.getActiveThreshold();
}

int GbInput::getReleaseThreshold() {
    return Joystick.getReleaseThreshold();
}

uint32_t GbInput::getBackHoldMs() {
    if (s_backHeld) {
        return millis() - s_backHoldStartMs;
    }
    return 0;
}
