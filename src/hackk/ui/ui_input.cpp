#include "hackk/ui_input.h"
#include "hackk/joystick_cal.h"
#include "hackk/hackk_button.h"
#include <Arduino.h>

#define PIN_JOY_X   34
#define PIN_JOY_Y   35
#define PIN_JOY_SW  32
#define PIN_ENTER   13
#define PIN_BACK    25

static JoystickCal   joy;
static HackkButton   btnEnter(PIN_ENTER);
static HackkButton   btnBack(PIN_BACK);
static uint32_t      lastMove = 0;
#define MOVE_REPEAT_MS 180

void ui_input_init() {
    pinMode(PIN_JOY_SW, INPUT_PULLUP);
    btnEnter.begin();
    btnBack.begin();
    // Skip runtime calibration — hardcode after one-time serial test
    joy.setCalibration(2048, 2048, 1500, 1500);
}

ui_input_t ui_input_poll(int timeout_ms) {
    uint32_t start = millis();

    while (millis() - start < (uint32_t)timeout_ms) {
        if (btnEnter.pressed())              return INPUT_ENTER;
        if (btnBack.pressed())               return INPUT_BACK;
        if (digitalRead(PIN_JOY_SW) == LOW)  { delay(30); return INPUT_ENTER; }

        int dx, dy;
        joy.read(PIN_JOY_X, PIN_JOY_Y, dx, dy);
        if ((dx || dy) && (millis() - lastMove > MOVE_REPEAT_MS)) {
            lastMove = millis();
            if (dy < 0) return INPUT_UP;
            if (dy > 0) return INPUT_DOWN;
            if (dx < 0) return INPUT_LEFT;
            if (dx > 0) return INPUT_RIGHT;
        }
        delay(10);
    }
    return INPUT_NONE;
}
