#include "RetroLauncher.h"
#include "RetroAssets.h"
#include "RetroDisplay.h"
#include "RetroInput.h"
#include "nes/NesDisplayAdapter.h"

static RetroLauncherFocus s_focus = FOCUS_GAME_MARIO;
static RetroGameSelection selectedGame = GAME_MARIO;
static RetroLaunchIntent s_launchIntent = RETRO_LAUNCH_NONE;
static bool exitRequested = false;
static bool needsRedraw = true;
static bool unavailableActive = false;
static unsigned long unavailableExpiry = 0;

static bool s_inSettingsMenu = false;
static int s_settingsMenuSelection = 0; // 0: HYBRID, 1: FULL GAME

void RetroLauncher::init(Adafruit_ST7735 &tft, bool resetSelection) {
    RetroInput::init();
    if (resetSelection) {
        selectedGame = GAME_MARIO;
        s_focus = FOCUS_GAME_MARIO;
    }
    s_inSettingsMenu = false;
    s_settingsMenuSelection = (int)NesDisplayAdapter::loadSavedDisplayMode();
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

    // Draw the compact Settings badge replacing the old Select/Play area
    RetroDisplay::drawSettingsSection(tft, (s_focus == FOCUS_SETTINGS), (uint8_t)NesDisplayAdapter::getDisplayMode());
}

void RetroLauncher::update(Adafruit_ST7735 &tft) {
    if (exitRequested) return;

    // =========================================================================
    // A. IN SETTINGS MENU OVERLAY
    // =========================================================================
    if (s_inSettingsMenu) {
        // Back button closes Settings Menu and returns to launcher
        if (RetroInput::isExitPressed()) {
            s_inSettingsMenu = false;
            needsRedraw = true;
            return;
        }

        // Navigate between DISPLAY MODE options: 0 = HYBRID, 1 = FULL GAME
        RetroNavEvent nav = RetroInput::readNavigation();
        if (nav == RETRO_NAV_DOWN) {
            if (s_settingsMenuSelection < 1) {
                s_settingsMenuSelection = 1;
                needsRedraw = true;
            }
        } else if (nav == RETRO_NAV_UP) {
            if (s_settingsMenuSelection > 0) {
                s_settingsMenuSelection = 0;
                needsRedraw = true;
            }
        }

        // Click / Enter selects and saves the active display mode to NVS
        if (RetroInput::isLaunchPressed()) {
            NesDisplayAdapter::saveDisplayMode((NesDisplayMode)s_settingsMenuSelection);
            needsRedraw = true;
        }

        if (needsRedraw) {
            size_t assetSize = 0;
            const uint8_t* assetData = getRetroScreenAsset(selectedGame, assetSize);
            RetroDisplay::drawFrame(tft, assetData, assetSize);
            RetroDisplay::drawSettingsSection(tft, true, (uint8_t)NesDisplayAdapter::getDisplayMode());
            RetroDisplay::drawSettingsMenu(tft, s_settingsMenuSelection, (uint8_t)NesDisplayAdapter::getDisplayMode());
            needsRedraw = false;
        }
        return;
    }

    // =========================================================================
    // B. IN MAIN RETRO ARCADE LAUNCHER
    // =========================================================================

    // 1. Back button returns to Micro_IOT OS (Home screen)
    if (RetroInput::isExitPressed()) {
        exitRequested = true;
        return;
    }

    // 2. Joystick UP/DOWN navigation across games and the Settings section
    RetroNavEvent nav = RetroInput::readNavigation();
    if (nav == RETRO_NAV_DOWN) {
        if (s_focus == FOCUS_GAME_MARIO) {
            s_focus = FOCUS_GAME_CONTRA;
            selectedGame = GAME_CONTRA;
        } else if (s_focus == FOCUS_GAME_CONTRA) {
            s_focus = FOCUS_GAME_POKEMON;
            selectedGame = GAME_POKEMON;
        } else if (s_focus == FOCUS_GAME_POKEMON) {
            s_focus = FOCUS_SETTINGS;
        } else if (s_focus == FOCUS_SETTINGS) {
            s_focus = FOCUS_GAME_MARIO;
            selectedGame = GAME_MARIO;
        }
        needsRedraw = true;
        unavailableActive = false;
    } else if (nav == RETRO_NAV_UP) {
        if (s_focus == FOCUS_GAME_MARIO) {
            s_focus = FOCUS_SETTINGS;
        } else if (s_focus == FOCUS_SETTINGS) {
            s_focus = FOCUS_GAME_POKEMON;
            selectedGame = GAME_POKEMON;
        } else if (s_focus == FOCUS_GAME_POKEMON) {
            s_focus = FOCUS_GAME_CONTRA;
            selectedGame = GAME_CONTRA;
        } else if (s_focus == FOCUS_GAME_CONTRA) {
            s_focus = FOCUS_GAME_MARIO;
            selectedGame = GAME_MARIO;
        }
        needsRedraw = true;
        unavailableActive = false;
    }

    // 3. Process Launch or Enter Settings
    if (RetroInput::isLaunchPressed()) {
        if (s_focus == FOCUS_SETTINGS) {
            s_inSettingsMenu = true;
            s_settingsMenuSelection = (int)NesDisplayAdapter::getDisplayMode();
            needsRedraw = true;
        } else if (s_focus == FOCUS_GAME_MARIO) {
            s_launchIntent = RETRO_LAUNCH_MARIO;
            return;
        } else if (s_focus == FOCUS_GAME_CONTRA) {
            s_launchIntent = RETRO_LAUNCH_CONTRA;
            return;
        } else if (s_focus == FOCUS_GAME_POKEMON) {
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

    // 5. Redraw screen asset and UI elements if selection or overlay changed
    if (needsRedraw) {
        size_t assetSize = 0;
        const uint8_t* assetData = getRetroScreenAsset(selectedGame, assetSize);
        RetroDisplay::drawFrame(tft, assetData, assetSize);

        // Draw the compact Settings section
        RetroDisplay::drawSettingsSection(tft, (s_focus == FOCUS_SETTINGS), (uint8_t)NesDisplayAdapter::getDisplayMode());

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

bool RetroLauncher::isInSettingsMenu() {
    return s_inSettingsMenu;
}
