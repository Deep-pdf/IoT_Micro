#include "RetroLauncher.h"
#include "RetroAssets.h"
#include "RetroDisplay.h"
#include "RetroInput.h"

static RetroGameSelection selectedGame = GAME_MARIO;
static RetroLaunchIntent s_launchIntent = RETRO_LAUNCH_NONE;
static bool exitRequested = false;
static bool needsRedraw = true;
static bool unavailableActive = false;
static unsigned long unavailableExpiry = 0;

void RetroLauncher::init(Adafruit_ST7735 &tft, bool resetSelection) {
    RetroInput::init();
    if (resetSelection) {
        selectedGame = GAME_MARIO;
    }
    s_launchIntent = RETRO_LAUNCH_NONE;
    exitRequested = false;
    needsRedraw = false;
    unavailableActive = false;
    unavailableExpiry = 0;

    // Render selected game screen (Arcade#1, Arcade#2, or Arcade#3)
    tft.setRotation(0);
    size_t assetSize = 0;
    const uint8_t* assetData = getRetroScreenAsset(selectedGame, assetSize);
    RetroDisplay::drawFrame(tft, assetData, assetSize);
}

void RetroLauncher::update(Adafruit_ST7735 &tft) {
    if (exitRequested) return;

    // 1. Back button returns to Micro_IOT OS (Home screen)
    if (RetroInput::isExitPressed()) {
        exitRequested = true;
        return;
    }

    // 2. Joystick UP/DOWN navigation with wrap-around
    RetroNavEvent nav = RetroInput::readNavigation();
    if (nav == RETRO_NAV_DOWN) {
        if (selectedGame == GAME_POKEMON) {
            selectedGame = GAME_MARIO; // Wrap to Mario (Arcade#1)
        } else {
            selectedGame = (RetroGameSelection)(selectedGame + 1);
        }
        needsRedraw = true;
        unavailableActive = false;
    } else if (nav == RETRO_NAV_UP) {
        if (selectedGame == GAME_MARIO) {
            selectedGame = GAME_POKEMON; // Wrap to Pokemon (Arcade#3)
        } else {
            selectedGame = (RetroGameSelection)(selectedGame - 1);
        }
        needsRedraw = true;
        unavailableActive = false;
    }

    // 3. Process Launch (Joystick Click or Enter Pushbutton)
    if (RetroInput::isLaunchPressed()) {
        if (selectedGame == GAME_MARIO) {
            s_launchIntent = RETRO_LAUNCH_MARIO;
            return;
        } else if (selectedGame == GAME_CONTRA) {
            s_launchIntent = RETRO_LAUNCH_CONTRA;
            return;
        } else {
            // Pokémon Red: Not implemented / not available
            unavailableActive = true;
            unavailableExpiry = millis() + 1500;
            needsRedraw = true;
        }
    }

    // 4. Auto-dismiss unavailable notification overlay after 1.5s
    if (unavailableActive && millis() >= unavailableExpiry) {
        unavailableActive = false;
        needsRedraw = true;
    }

    // 5. Redraw screen asset if selection or overlay changed
    if (needsRedraw) {
        size_t assetSize = 0;
        const uint8_t* assetData = getRetroScreenAsset(selectedGame, assetSize);
        RetroDisplay::drawFrame(tft, assetData, assetSize);

        if (unavailableActive) {
            RetroDisplay::drawUnavailableNotification(tft, "GB Emulator N/A");
        }

        needsRedraw = false;
    }
}

bool RetroLauncher::shouldExit() {
    return exitRequested;
}

RetroGameSelection RetroLauncher::getSelectedGame() {
    return selectedGame;
}

RetroLaunchIntent RetroLauncher::getLaunchIntent() {
    return s_launchIntent;
}

void RetroLauncher::clearLaunchIntent() {
    s_launchIntent = RETRO_LAUNCH_NONE;
}
