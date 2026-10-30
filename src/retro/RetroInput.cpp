#include "RetroInput.h"
#include "joystick.h"
#include "button.h"

static bool joyCentered = true;
static unsigned long lastNavTime = 0;
static const unsigned long REPEAT_DELAY_MS = 350;

static int lastJoySwState = HIGH;
static unsigned long lastJoySwDebounce = 0;

void RetroInput::init() {
    pinMode(HW_JOY_SW, INPUT_PULLUP);
    joyCentered = true;
    lastNavTime = 0;
    lastJoySwState = HIGH;
    lastJoySwDebounce = 0;
}

RetroNavEvent RetroInput::readNavigation() {
    int vry = readJoystickY();
    unsigned long now = millis();

    // Deadzone centering check
    bool isCentered = (vry >= 1500 && vry <= 2700);

    if (isCentered) {
        joyCentered = true;
        return RETRO_NAV_NONE;
    }

    // Require return-to-center or minimum repeat delay so one physical movement
    // produces exactly one selection change without runaway skipping
    if (joyCentered || (now - lastNavTime >= REPEAT_DELAY_MS)) {
        if (vry > 3000) { // Physical DOWN
            joyCentered = false;
            lastNavTime = now;
            return RETRO_NAV_DOWN;
        } else if (vry < 1000) { // Physical UP
            joyCentered = false;
            lastNavTime = now;
            return RETRO_NAV_UP;
        }
    }

    return RETRO_NAV_NONE;
}

bool RetroInput::isLaunchPressed() {
    // 1. Enter Pushbutton on GPIO 13
    if (isEnterPressed()) {
        return true;
    }

    // 2. Joystick Switch Click on GPIO 32
    int swReading = digitalRead(HW_JOY_SW);
    if (swReading != lastJoySwState) {
        if (millis() - lastJoySwDebounce > 50) {
            lastJoySwDebounce = millis();
            lastJoySwState = swReading;
            if (lastJoySwState == LOW) {
                return true;
            }
        }
    }

    return false;
}

bool RetroInput::isExitPressed() {
    return isBackPressed();
}

