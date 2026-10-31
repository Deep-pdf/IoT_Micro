#include "joystick.h"

GlobalJoystick Joystick;

GlobalJoystick::GlobalJoystick()
    : m_rawX(2048), m_rawY(2048),
      m_logicalX(2048), m_logicalY(2048),
      m_centerX(2048), m_centerY(2048),
      m_threshActive(450), m_threshRelease(250),
      m_up(false), m_down(false), m_left(false), m_right(false), m_click(false),
      m_lastDir(JoyDir::NONE), m_dirHoldStart(0), m_dirLastRepeat(0), m_dirConsumed(false),
      m_lastClickRaw(HIGH), m_lastClickDebounce(0), m_clickTriggered(false),
      m_lastUpdateMicros(0)
{
}

void GlobalJoystick::begin() {
    pinMode(HW_JOY_X, INPUT);
    pinMode(HW_JOY_Y, INPUT);
    pinMode(HW_JOY_SW, INPUT_PULLUP);

    // Fast resting center calibration (16 samples over ~1.5 ms)
    long sumX = 0;
    long sumY = 0;
    for (int i = 0; i < 16; i++) {
        int ry = analogRead(HW_JOY_Y);
        int rx = analogRead(HW_JOY_X);
        sumX += (4095 - ry);
        sumY += rx;
        delayMicroseconds(50);
    }

    int calX = (int)(sumX / 16);
    int calY = (int)(sumY / 16);

    // Sanity check: if stick was physically held during boot, fallback to 2048
    if (calX >= 1200 && calX <= 2800) m_centerX = calX;
    else m_centerX = 2048;

    if (calY >= 1200 && calY <= 2800) m_centerY = calY;
    else m_centerY = 2048;

    Serial.printf("[Joystick] Calibrated Center: X=%d, Y=%d | Active=+/-d, Release=+/-d\n",
                  m_centerX, m_centerY, m_threshActive, m_threshRelease);

    update();
}

void GlobalJoystick::update() {
    unsigned long nowMicros = micros();
    if (m_lastUpdateMicros != 0 && (nowMicros - m_lastUpdateMicros < 1000)) {
        return; // Rate limit ADC sampling to at most once per millisecond
    }
    m_lastUpdateMicros = nowMicros;

    // 1. Read hardware ADC and apply 90-degree CW rotation
    m_rawX = analogRead(HW_JOY_X);
    m_rawY = analogRead(HW_JOY_Y);

    int lx = 4095 - m_rawY;
    if (lx < 0) lx = 0;
    else if (lx > 4095) lx = 4095;
    m_logicalX = lx;

    int ly = m_rawX;
    if (ly < 0) ly = 0;
    else if (ly > 4095) ly = 4095;
    m_logicalY = ly;

    // 2. Evaluate X-axis (LEFT / RIGHT) with hysteresis and immediate direction switching
    int dx = m_logicalX - m_centerX;
    if (dx < -m_threshActive) {
        // Physical LEFT
        m_left = true;
        m_right = false;
    } else if (dx > m_threshActive) {
        // Physical RIGHT
        m_right = true;
        m_left = false;
    } else if (dx >= -m_threshRelease && m_left) {
        // Immediate release when moving back towards center
        m_left = false;
    } else if (dx <= m_threshRelease && m_right) {
        // Immediate release when moving back towards center
        m_right = false;
    }

    // 3. Evaluate Y-axis (UP / DOWN) with hysteresis and immediate direction switching
    int dy = m_logicalY - m_centerY;
    if (dy < -m_threshActive) {
        // Physical UP (logical Y goes low)
        m_up = true;
        m_down = false;
    } else if (dy > m_threshActive) {
        // Physical DOWN (logical Y goes high)
        m_down = true;
        m_up = false;
    } else if (dy >= -m_threshRelease && m_up) {
        // Immediate release when moving back towards center
        m_up = false;
    } else if (dy <= m_threshRelease && m_down) {
        // Immediate release when moving back towards center
        m_down = false;
    }

    // 4. Click reading (GPIO 32 with 30ms debounce)
    int sw = digitalRead(HW_JOY_SW);
    unsigned long now = millis();
    m_click = (sw == LOW);

    m_clickTriggered = false;
    if (sw != m_lastClickRaw) {
        if (now - m_lastClickDebounce > 30) {
            m_lastClickDebounce = now;
            m_lastClickRaw = sw;
            if (sw == LOW) {
                m_clickTriggered = true;
            }
        }
    }

    // 5. Menu auto-repeat tracking
    JoyDir curDir = getDirection();
    if (curDir != m_lastDir) {
        m_lastDir = curDir;
        m_dirHoldStart = now;
        m_dirLastRepeat = now;
        m_dirConsumed = false;
    }
}

JoyDir GlobalJoystick::getDirection() const {
    int dx = abs(m_logicalX - m_centerX);
    int dy = abs(m_logicalY - m_centerY);
    if (dx >= dy) {
        if (m_left) return JoyDir::LEFT;
        if (m_right) return JoyDir::RIGHT;
    }
    if (m_up) return JoyDir::UP;
    if (m_down) return JoyDir::DOWN;
    return JoyDir::NONE;
}

bool GlobalJoystick::isUp() const { return m_up; }
bool GlobalJoystick::isDown() const { return m_down; }
bool GlobalJoystick::isLeft() const { return m_left; }
bool GlobalJoystick::isRight() const { return m_right; }
bool GlobalJoystick::isPressed() const { return m_click; }

bool GlobalJoystick::wasUpPressed() {
    if (getDirection() != JoyDir::UP) return false;
    unsigned long now = millis();
    if (!m_dirConsumed) {
        m_dirConsumed = true;
        return true;
    }
    // Auto-repeat: 350ms initial delay, then every 150ms
    if ((now - m_dirHoldStart >= 350) && (now - m_dirLastRepeat >= 150)) {
        m_dirLastRepeat = now;
        return true;
    }
    return false;
}

bool GlobalJoystick::wasDownPressed() {
    if (getDirection() != JoyDir::DOWN) return false;
    unsigned long now = millis();
    if (!m_dirConsumed) {
        m_dirConsumed = true;
        return true;
    }
    if ((now - m_dirHoldStart >= 350) && (now - m_dirLastRepeat >= 150)) {
        m_dirLastRepeat = now;
        return true;
    }
    return false;
}

bool GlobalJoystick::wasLeftPressed() {
    if (getDirection() != JoyDir::LEFT) return false;
    unsigned long now = millis();
    if (!m_dirConsumed) {
        m_dirConsumed = true;
        return true;
    }
    if ((now - m_dirHoldStart >= 350) && (now - m_dirLastRepeat >= 150)) {
        m_dirLastRepeat = now;
        return true;
    }
    return false;
}

bool GlobalJoystick::wasRightPressed() {
    if (getDirection() != JoyDir::RIGHT) return false;
    unsigned long now = millis();
    if (!m_dirConsumed) {
        m_dirConsumed = true;
        return true;
    }
    if ((now - m_dirHoldStart >= 350) && (now - m_dirLastRepeat >= 150)) {
        m_dirLastRepeat = now;
        return true;
    }
    return false;
}

bool GlobalJoystick::wasClickPressed() {
    if (m_clickTriggered) {
        m_clickTriggered = false;
        return true;
    }
    return false;
}

int GlobalJoystick::getLogicalX() const { return m_logicalX; }
int GlobalJoystick::getLogicalY() const { return m_logicalY; }
int GlobalJoystick::getRawX() const { return m_rawX; }
int GlobalJoystick::getRawY() const { return m_rawY; }
int GlobalJoystick::getCenterX() const { return m_centerX; }
int GlobalJoystick::getCenterY() const { return m_centerY; }
int GlobalJoystick::getActiveThreshold() const { return m_threshActive; }
int GlobalJoystick::getReleaseThreshold() const { return m_threshRelease; }
void GlobalJoystick::setThresholds(int activeThreshold, int releaseThreshold) {
    m_threshActive = activeThreshold;
    m_threshRelease = releaseThreshold;
}

