#include "SpotifyApp.h"
#include "SpotifyConnection.h"
#include "SpotifyTypes.h"
#include "SpotifyUI.h"
#include "button.h"
#include "config.h"
#include <Arduino.h>

static SpotifyTrackState currentTrack;
static bool wantsExit = false;
static unsigned long lastPollMillis = 0;
static const unsigned long POLL_INTERVAL_MS = 3000; // Synchronize every 3 seconds

void SpotifyApp::init(Adafruit_ST7735 &tft) {
    wantsExit = false;

    Serial.println("[Spotify] Spotify UI opened");
    Serial.println("[Spotify] Requesting Spotify state...");

    // Initialize display & UI
    SpotifyUI::init(tft);

    // Initial placeholder state while fetching
    currentTrack = SpotifyTrackState();
    currentTrack.connected = SpotifyConnection::ensureWiFiConnected();
    currentTrack.title = "Connecting...";
    currentTrack.artist = "Spotify Bridge";
    SpotifyUI::drawFullUI(tft, currentTrack);

    // Fetch initial live state
    if (SpotifyConnection::getState(currentTrack)) {
        Serial.println("[Spotify] Spotify state received");
        SpotifyUI::drawFullUI(tft, currentTrack);
    } else {
        Serial.println("[Spotify] Initial bridge fetch failed / offline");
        SpotifyUI::drawFullUI(tft, currentTrack);
    }

    lastPollMillis = millis();
}

void SpotifyApp::update(Adafruit_ST7735 &tft) {
    unsigned long now = millis();

    // 1. Process BACK button -> return to Home
    if (isBackPressed()) {
        wantsExit = true;
        return;
    }

    // 2. Periodic state synchronization with Python bridge (every 3 seconds)
    bool stateFetched = false;
    if (now - lastPollMillis >= POLL_INTERVAL_MS) {
        lastPollMillis = now;
        stateFetched = SpotifyConnection::getState(currentTrack);
    }

    // 3. Dynamic UI update (local progress interpolation & animated equalizer waves)
    SpotifyUI::updateDynamicUI(tft, currentTrack, stateFetched);
}

bool SpotifyApp::shouldExit() {
    if (wantsExit) {
        wantsExit = false;
        return true;
    }
    return false;
}
