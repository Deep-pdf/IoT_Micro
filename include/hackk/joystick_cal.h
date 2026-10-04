#pragma once
#include <Arduino.h>

class JoystickCal {
public:
    // Call once in setup. Blocks until user centres stick and presses ENTER.
    void calibrate(int pinX, int pinY, int pinEnter) {
        Serial.println("Centre stick, then press ENTER...");
        while (digitalRead(pinEnter) == HIGH) delay(10);
        delay(200);

        long sumX = 0, sumY = 0;
        for (int i = 0; i < 50; i++) {
            sumX += analogRead(pinX);
            sumY += analogRead(pinY);
            delay(5);
        }
        _cx = sumX / 50;
        _cy = sumY / 50;

        Serial.printf("Centre: X=%d Y=%d\n", _cx, _cy);
        Serial.println("Push stick fully RIGHT and DOWN, then press ENTER...");
        while (digitalRead(pinEnter) == HIGH) delay(10);
        delay(200);

        int maxDx = 0, maxDy = 0;
        for (int i = 0; i < 50; i++) {
            int dx = abs(analogRead(pinX) - _cx);
            int dy = abs(analogRead(pinY) - _cy);
            if (dx > maxDx) maxDx = dx;
            if (dy > maxDy) maxDy = dy;
            delay(5);
        }
        _rangeX = maxDx;
        _rangeY = maxDy;
        _deadzone = (_rangeX + _rangeY) / 4;  // was /8, doubled (25% dead zone)

        Serial.printf("Cal: cx=%d cy=%d rx=%d ry=%d dz=%d\n",
                      _cx, _cy, _rangeX, _rangeY, _deadzone);
    }

    void setCalibration(int cx, int cy, int rx, int ry) {
        _cx = cx; _cy = cy;
        _rangeX = rx; _rangeY = ry;
        _deadzone = (rx + ry) / 4;   // 25 % deadzone
    }

    // Returns -1, 0, +1 for each axis after deadzone filtering.
    void read(int pinX, int pinY, int &outX, int &outY) const {
        int dx = analogRead(pinX) - _cx;
        int dy = analogRead(pinY) - _cy;
        outX = (abs(dx) > _deadzone) ? (dx > 0 ? 1 : -1) : 0;
        outY = (abs(dy) > _deadzone) ? (dy > 0 ? 1 : -1) : 0;
    }

private:
    int _cx = 2048, _cy = 2048;
    int _rangeX = 1500, _rangeY = 1500;
    int _deadzone = 300;
};
