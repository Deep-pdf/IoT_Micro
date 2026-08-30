#include "SpotifyApp.h"
#include "SpotifyConnection.h"
#include "SpotifyTypes.h"
#include "button.h"
#include "config.h"
#include <Arduino.h>

static SpotifyTrackState currentTrack;
static bool wantsExit = false;

static void drawPlaceholderScreen(Adafruit_ST7735 &tft, const char* statusMsg) {
    tft.setRotation(2);
    tft.fillScreen(ST77XX_BLACK);

    tft.setFont(NULL);
    tft.setTextSize(1);

    // Header
    tft.setTextColor(tft.color565(29, 185, 84), ST77XX_BLACK);
    tft.setCursor(14, 18);
    tft.print("SPOTIFY BRIDGE TEST");

    tft.drawFastHLine(10, 32, 108, tft.color565(80, 80, 80));

    // Status Message
    tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
    tft.setCursor(12, 46);
    tft.print(statusMsg);

    // Host info
    tft.setTextColor(tft.color565(170, 170, 170), ST77XX_BLACK);
    tft.setCursor(12, 68);
    tft.print("Host:");
    tft.setCursor(12, 80);
    tft.print(SPOTIFY_BRIDGE_HOST);

    // Instructions
    tft.setTextColor(tft.color565(120, 120, 120), ST77XX_BLACK);
    tft.setCursor(12, 104);
    tft.print("Serial: 115200 baud");

    tft.setCursor(12, 126);
    tft.print("[ENTER] Re-test");

    tft.setCursor(12, 140);
    tft.print("[BACK]  Return Home");
}

void SpotifyApp::init(Adafruit_ST7735 &tft) {
    wantsExit = false;

    // Show initial test screen
    drawPlaceholderScreen(tft, "Testing Bridge...");

    // Execute the Spotify Bridge test
    bool success = SpotifyConnection::getState(currentTrack);

    // Update screen with test outcome
    if (success) {
        drawPlaceholderScreen(tft, "Connected! See Serial");
    } else {
        drawPlaceholderScreen(tft, "Failed! See Serial");
    }
}

void SpotifyApp::update(Adafruit_ST7735 &tft) {
    // 1. Process BACK button -> return to Home
    if (isBackPressed()) {
        wantsExit = true;
        return;
    }

    // 2. Process ENTER button -> re-trigger bridge test
    if (isEnterPressed()) {
        drawPlaceholderScreen(tft, "Testing Bridge...");
        bool success = SpotifyConnection::getState(currentTrack);
        if (success) {
            drawPlaceholderScreen(tft, "Connected! See Serial");
        } else {
            drawPlaceholderScreen(tft, "Failed! See Serial");
        }
    }
}

bool SpotifyApp::shouldExit() {
    if (wantsExit) {
        wantsExit = false;
        return true;
    }
    return false;
}
