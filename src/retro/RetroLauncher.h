#ifndef RETRO_LAUNCHER_H
#define RETRO_LAUNCHER_H

#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>

enum RetroGameSelection {
    GAME_CONTRA = 0,    // Row 1: Contra
    GAME_POKEMON = 1,   // Row 2: Pokémon Red
    GAME_RAD_RACER = 2, // Row 3: Rad Racer 2
    GAME_COUNT = 3,
    GAME_MARIO = 99     // Preserved for backwards compatibility
};

enum RetroLauncherFocus {
    FOCUS_GAME_CONTRA = 0,
    FOCUS_GAME_POKEMON = 1,
    FOCUS_GAME_RAD_RACER = 2,
    FOCUS_SETTINGS = 3,
    FOCUS_GAME_MARIO = 99
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
