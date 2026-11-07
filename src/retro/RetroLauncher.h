#ifndef RETRO_LAUNCHER_H
#define RETRO_LAUNCHER_H

#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>

enum RetroGameSelection {
    GAME_MARIO = 0,     // Super Mario Bros. 3 (Arcade#1)
    GAME_CONTRA = 1,    // Contra (Arcade#2)
    GAME_POKEMON = 2,   // Pokémon Red (Arcade#3)
    GAME_RAD_RACER = 3, // Rad Racer 2 (Arcade#4)
    GAME_COUNT = 4
};

enum RetroLauncherFocus {
    FOCUS_GAME_MARIO = 0,
    FOCUS_GAME_CONTRA = 1,
    FOCUS_GAME_POKEMON = 2,
    FOCUS_GAME_RAD_RACER = 3,
    FOCUS_SETTINGS = 4
};

enum RetroLaunchIntent {
    RETRO_LAUNCH_NONE = 0,
    RETRO_LAUNCH_MARIO,
    RETRO_LAUNCH_CONTRA,
    RETRO_LAUNCH_POKEMON,
    RETRO_LAUNCH_RAD_RACER
};

class RetroLauncher {
public:
    static void init(Adafruit_ST7735 &tft, bool resetSelection = false);
    static void update(Adafruit_ST7735 &tft);
    static bool shouldExit();
    static RetroGameSelection getSelectedGame();
    static RetroLaunchIntent getLaunchIntent();
    static void clearLaunchIntent();
    static bool isInSettingsMenu();
};

#endif // RETRO_LAUNCHER_H
