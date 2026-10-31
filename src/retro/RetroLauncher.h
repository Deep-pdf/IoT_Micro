#ifndef RETRO_LAUNCHER_H
#define RETRO_LAUNCHER_H

#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>

enum RetroGameSelection {
    GAME_MARIO = 0,    // Arcade#1 (Super Mario Bros. 3)
    GAME_CONTRA = 1,   // Arcade#2 (Contra)
    GAME_POKEMON = 2,  // Arcade#3 (Pokémon Red)
    GAME_COUNT = 3
};

enum RetroLaunchIntent {
    RETRO_LAUNCH_NONE = 0,
    RETRO_LAUNCH_MARIO,
    RETRO_LAUNCH_CONTRA
};

class RetroLauncher {
public:
    static void init(Adafruit_ST7735 &tft, bool resetSelection = false);
    static void update(Adafruit_ST7735 &tft);
    static bool shouldExit();
    static RetroGameSelection getSelectedGame();
    static RetroLaunchIntent getLaunchIntent();
    static void clearLaunchIntent();
};

#endif // RETRO_LAUNCHER_H
