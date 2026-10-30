#ifndef RETRO_LAUNCHER_H
#define RETRO_LAUNCHER_H

#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>

enum RetroGameSelection {
    GAME_MARIO = 0,    // Arcade#1
    GAME_CONTRA = 1,   // Arcade#2
    GAME_POKEMON = 2,  // Arcade#3
    GAME_COUNT = 3
};

class RetroLauncher {
public:
    static void init(Adafruit_ST7735 &tft);
    static void update(Adafruit_ST7735 &tft);
    static bool shouldExit();
    static RetroGameSelection getSelectedGame();
};

#endif // RETRO_LAUNCHER_H

