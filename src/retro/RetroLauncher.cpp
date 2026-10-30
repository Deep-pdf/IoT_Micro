#include "RetroLauncher.h"
#include "RetroAssets.h"
#include "RetroDisplay.h"
#include "RetroInput.h"

static RetroGameSelection selectedGame = GAME_MARIO;
static bool exitRequested = false;
static bool needsRedraw = true;
static bool launchMessageActive = false;
static unsigned long launchMessageExpiry = 0;

static const char* const GAME_TITLES[] = {
    "SUPER MARIO BROS.",
    "CONTRA (NES)",
    "POKEMON RED (GB)"
};

void RetroLauncher::init(Adafruit_ST7735 &tft) {
    RetroInput::init();
    selectedGame = GAME_MARIO;
    exitRequested = false;
    needsRedraw = true;
    launchMessageActive = false;
    launchMessageExpiry = 0;

    // Blank screen to black before drawing initial frame
    tft.setRotation(0);
    tft.fillScreen(ST77XX_BLACK);

    // Render initial Mario state (Arcade#1)
    size_t assetSize = 0;
    const uint8_t* assetData = getRetroScreenAsset(selectedGame, assetSize);
    RetroDisplay::drawFrame(tft, assetData, assetSize);
    needsRedraw = false;
}

void RetroLauncher::update(Adafruit_ST7735 &tft) {
    if (exitRequested) return;

    // Check Back button to return to Micro_IOT OS
    if (RetroInput::isExitPressed()) {
        exitRequested = true;
        return;
    }

    // Process Joystick Navigation
    RetroNavEvent nav = RetroInput::readNavigation();
    if (nav == RETRO_NAV_DOWN) {
        if (selectedGame < GAME_POKEMON) {
            selectedGame = (RetroGameSelection)(selectedGame + 1);
            needsRedraw = true;
            launchMessageActive = false;
        }
    } else if (nav == RETRO_NAV_UP) {
        if (selectedGame > GAME_MARIO) {
            selectedGame = (RetroGameSelection)(selectedGame - 1);
            needsRedraw = true;
            launchMessageActive = false;
        }
    }

    // Process Launch (Joystick click or Enter button)
    if (RetroInput::isLaunchPressed()) {
        launchMessageActive = true;
        launchMessageExpiry = millis() + 1500;
        needsRedraw = true;
    }

    // Auto-dismiss launch notification
    if (launchMessageActive && millis() >= launchMessageExpiry) {
        launchMessageActive = false;
        needsRedraw = true;
    }

    // Perform screen update if selection or notification state changed
    if (needsRedraw) {
        size_t assetSize = 0;
        const uint8_t* assetData = getRetroScreenAsset(selectedGame, assetSize);
        RetroDisplay::drawFrame(tft, assetData, assetSize);

        if (launchMessageActive) {
            RetroDisplay::drawLaunchNotification(tft, GAME_TITLES[selectedGame]);
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

