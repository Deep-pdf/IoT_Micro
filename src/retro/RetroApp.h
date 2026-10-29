#ifndef RETRO_APP_H
#define RETRO_APP_H

#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>

class RetroApp {
public:
    // Called once when entering the Retro Arcade screen
    static void init(Adafruit_ST7735 &tft);

    // Called in loop() while in Retro Arcade screen
    static void update(Adafruit_ST7735 &tft);

    // Returns true when user presses BACK to return to Home Screen
    static bool shouldExit();
};

#endif // RETRO_APP_H

