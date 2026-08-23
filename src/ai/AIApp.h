#ifndef AI_APP_H
#define AI_APP_H

#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>

class AIApp {
public:
    static void init(Adafruit_ST7735 &tft);
    static void update(Adafruit_ST7735 &tft);
};

#endif // AI_APP_H
