#pragma once
#include <Arduino.h>

class HackkButton {
public:
    HackkButton(uint8_t pin) : _pin(pin), _last(HIGH), _stable(HIGH), _t(0) {}
    void begin() { pinMode(_pin, INPUT_PULLUP); }

    // Returns true exactly once when the button is released (LOW→HIGH),
    // i.e., on press completion. Debounced with 40 ms.
    bool pressed() {
        bool cur = digitalRead(_pin);
        if (cur != _last) { _t = millis(); _last = cur; }
        if (millis() - _t > 40 && cur != _stable) {
            _stable = cur;
            if (_stable == LOW) return true;
        }
        return false;
    }
private:
    uint8_t  _pin;
    bool     _last, _stable;
    uint32_t _t;
};
