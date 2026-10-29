#ifndef JOYSTICK_H
#define JOYSTICK_H

#include <Arduino.h>

// Physical hardware pins (ESP32 ADC1)
#define HW_JOY_X  34
#define HW_JOY_Y  35
#define HW_JOY_SW 32

/**
 * Universal Joystick Reader for 90-degree clockwise rotated hardware.
 *
 * Physical Rotation Mapping:
 * - Physical UP    -> y goes LOW  (< 1000)
 * - Physical DOWN  -> y goes HIGH (> 3000)
 * - Physical LEFT  -> x goes LOW  (< 1000)
 * - Physical RIGHT -> x goes HIGH (> 3000)
 *
 * Math Derivation (90 deg Clockwise Rotation):
 *   Logical Y = Raw X
 *   Logical X = 4095 - Raw Y
 */
inline int readJoystickX() {
    int rawY = analogRead(HW_JOY_Y);
    int x = 4095 - rawY;
    if (x < 0) x = 0;
    if (x > 4095) x = 4095;
    return x;
}

inline int readJoystickY() {
    int rawX = analogRead(HW_JOY_X);
    int y = rawX;
    if (y < 0) y = 0;
    if (y > 4095) y = 4095;
    return y;
}

#endif // JOYSTICK_H
