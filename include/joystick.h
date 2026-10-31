#ifndef JOYSTICK_H
#define JOYSTICK_H

#include <Arduino.h>

// Physical hardware pins (ESP32 ADC1)
#define HW_JOY_X  34
#define HW_JOY_Y  35
#define HW_JOY_SW 32

enum class JoyDir : uint8_t {
    NONE = 0,
    UP,
    DOWN,
    LEFT,
    RIGHT
};

class GlobalJoystick {
public:
    GlobalJoystick();

    // Call once in setup() to configure pins and calibrate resting center
    void begin();

    // High-frequency polling update: reads ADC, evaluates thresholds, handles hysteresis & repeats
    void update();

    // Continuous digital state (ideal for games: NES, Pixel Wars, Lost Crown)
    bool isUp() const;
    bool isDown() const;
    bool isLeft() const;
    bool isRight() const;
    bool isPressed() const; // Joystick click (GPIO 32, active LOW)
    JoyDir getDirection() const;

    // Edge-triggered & auto-repeat state (ideal for menus: Home, Retro Launcher, AI App)
    bool wasUpPressed();
    bool wasDownPressed();
    bool wasLeftPressed();
    bool wasRightPressed();
    bool wasClickPressed();

    // Analog readings (rotated logical coordinates 0..4095)
    int getLogicalX() const;
    int getLogicalY() const;
    int getRawX() const;
    int getRawY() const;

    // Calibration & threshold inspection/tuning
    int getCenterX() const;
    int getCenterY() const;
    int getActiveThreshold() const;
    int getReleaseThreshold() const;
    void setThresholds(int activeThreshold, int releaseThreshold);

private:
    int m_rawX;
    int m_rawY;
    int m_logicalX;
    int m_logicalY;
    int m_centerX;
    int m_centerY;

    int m_threshActive;  // default 450 counts from center
    int m_threshRelease; // default 250 counts from center

    bool m_up;
    bool m_down;
    bool m_left;
    bool m_right;
    bool m_click;

    // Edge & Repeat management for menu navigation
    JoyDir m_lastDir;
    unsigned long m_dirHoldStart;
    unsigned long m_dirLastRepeat;
    bool m_dirConsumed;

    bool m_lastClickRaw;
    unsigned long m_lastClickDebounce;
    bool m_clickTriggered;
};

// Global singleton instance accessible everywhere in the project
extern GlobalJoystick Joystick;

// Legacy compatibility helpers
inline int readJoystickX() {
    return Joystick.getLogicalX();
}

inline int readJoystickY() {
    return Joystick.getLogicalY();
}

#endif // JOYSTICK_H
