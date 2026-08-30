#include "SpotifyApp.h"
#include "SpotifyConnection.h"
#include "SpotifyTypes.h"
#include "SpotifyUI.h"
#include "button.h"
#include "config.h"
#include <Arduino.h>

// Joystick Pins
#define PIN_JOY_X   34
#define PIN_JOY_Y   35
#define PIN_JOY_SW  32

static SpotifyTrackState currentTrack;
static SpotifyControlSelection currentSelection = CTRL_PLAYPAUSE;
static bool wantsExit = false;
static unsigned long lastPollMillis = 0;
static const unsigned long POLL_INTERVAL_MS = 2500; // Poll bridge every 2.5s

// Artwork Buffer (40x40 RGB565 = 1600 words = 3.2 KB)
static uint16_t artworkBuffer[40 * 40];
static bool hasArtwork = false;
static String lastArtworkTrackId = "";

// Joystick Debounce & Deadzone State
static bool joyCentered = true;
static int joySwLastState = HIGH;
static unsigned long lastSwDebounce = 0;

static void updateTrackArtwork() {
    if (currentTrack.trackId.isEmpty()) {
        hasArtwork = false;
        lastArtworkTrackId = "";
        return;
    }

    if (currentTrack.trackId != lastArtworkTrackId) {
        lastArtworkTrackId = currentTrack.trackId;
        hasArtwork = SpotifyConnection::fetchArtwork(artworkBuffer, 40 * 40, 40);
        if (hasArtwork) {
            Serial.println("[Spotify] Album artwork fetched (40x40 RGB565)");
        }
    }
}

void SpotifyApp::init(Adafruit_ST7735 &tft) {
    wantsExit = false;
    currentSelection = CTRL_PLAYPAUSE;
    joyCentered = true;
    hasArtwork = false;
    lastArtworkTrackId = "";
    pinMode(PIN_JOY_SW, INPUT_PULLUP);

    Serial.println("[Spotify] Spotify UI opened");

    // Initialize display
    SpotifyUI::init(tft);

    // Initial placeholder state
    currentTrack = SpotifyTrackState();
    currentTrack.connected = SpotifyConnection::ensureWiFiConnected();
    currentTrack.title = "Connecting...";
    currentTrack.artist = "Spotify Bridge";
    SpotifyUI::drawFullUI(tft, currentTrack, currentSelection, artworkBuffer, hasArtwork);

    // Fetch initial live state
    if (SpotifyConnection::getState(currentTrack)) {
        updateTrackArtwork();
        SpotifyUI::drawFullUI(tft, currentTrack, currentSelection, artworkBuffer, hasArtwork);
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

    // 2. Process Joystick Switch Pin 32 (Debounced)
    int swRead = digitalRead(PIN_JOY_SW);
    bool joySwClicked = false;
    if (swRead != joySwLastState) {
        lastSwDebounce = now;
        joySwLastState = swRead;
    }
    if ((now - lastSwDebounce) > 50) {
        static int swStable = HIGH;
        if (swRead != swStable) {
            swStable = swRead;
            if (swStable == LOW) {
                joySwClicked = true;
            }
        }
    }

    // 3. Process Joystick Navigation (Move focus between PREV, PLAY/PAUSE, NEXT)
    int vrx = analogRead(PIN_JOY_X);
    int vry = analogRead(PIN_JOY_Y);

    bool isCentered = (vrx > 1500 && vrx < 2700 && vry > 1500 && vry < 2700);

    if (isCentered) {
        joyCentered = true;
    } else if (joyCentered) {
        if (vrx < 1000) {
            // Tilting LEFT -> move focus left
            joyCentered = false;
            if (currentSelection == CTRL_NEXT) {
                currentSelection = CTRL_PLAYPAUSE;
            } else {
                currentSelection = CTRL_PREV;
            }
            SpotifyUI::drawControls(tft, currentTrack.playing, currentSelection);
        } else if (vrx > 3000) {
            // Tilting RIGHT -> move focus right
            joyCentered = false;
            if (currentSelection == CTRL_PREV) {
                currentSelection = CTRL_PLAYPAUSE;
            } else {
                currentSelection = CTRL_NEXT;
            }
            SpotifyUI::drawControls(tft, currentTrack.playing, currentSelection);
        } else if (vry < 1000 || vry > 3000) {
            // Tilting UP / DOWN -> reset focus to PLAY/PAUSE
            joyCentered = false;
            currentSelection = CTRL_PLAYPAUSE;
            SpotifyUI::drawControls(tft, currentTrack.playing, currentSelection);
        }
    }

    // 4. Process Action Trigger (ENTER button Pin 13 OR Joystick Switch Pin 32)
    bool actionTriggered = isEnterPressed() || joySwClicked;

    if (actionTriggered) {
        if (currentSelection == CTRL_PLAYPAUSE) {
            SpotifyConnection::sendPlayPause();
            currentTrack.playing = !currentTrack.playing;
            currentTrack.lastSyncMillis = now;
            SpotifyUI::drawControls(tft, currentTrack.playing, currentSelection);
            lastPollMillis = now - (POLL_INTERVAL_MS - 400); // Trigger fast refresh
        } else if (currentSelection == CTRL_PREV) {
            SpotifyConnection::sendPrevious();
            delay(150);
            SpotifyConnection::getState(currentTrack);
            updateTrackArtwork();
            SpotifyUI::drawFullUI(tft, currentTrack, currentSelection, artworkBuffer, hasArtwork);
            lastPollMillis = now;
        } else if (currentSelection == CTRL_NEXT) {
            SpotifyConnection::sendNext();
            delay(150);
            SpotifyConnection::getState(currentTrack);
            updateTrackArtwork();
            SpotifyUI::drawFullUI(tft, currentTrack, currentSelection, artworkBuffer, hasArtwork);
            lastPollMillis = now;
        }
    }

    // 5. Periodic Background State Synchronization
    bool stateFetched = false;
    if (now - lastPollMillis >= POLL_INTERVAL_MS) {
        lastPollMillis = now;
        String prevId = currentTrack.trackId;
        stateFetched = SpotifyConnection::getState(currentTrack);
        if (stateFetched && currentTrack.trackId != prevId) {
            updateTrackArtwork();
        }
    }

    // 6. Dynamic UI Animation (Local progress estimation & retro equalizer waves)
    SpotifyUI::updateDynamicUI(tft, currentTrack, currentSelection, stateFetched, artworkBuffer, hasArtwork);
}

bool SpotifyApp::shouldExit() {
    if (wantsExit) {
        wantsExit = false;
        return true;
    }
    return false;
}
