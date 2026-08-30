#include "SpotifyApp.h"
#include "SpotifyTypes.h"
#include "SpotifyConnection.h"
#include "SpotifyUI.h"
#include "button.h"
#include "config.h"
#include <Arduino.h>

#ifndef SPOTIFY_MOCK_MODE
#define SPOTIFY_MOCK_MODE false
#endif

// Joystick Pins
#define PIN_JOY_X   34
#define PIN_JOY_Y   35
#define PIN_JOY_SW  32

// Mock Track Structure
struct MockTrack {
    const char* title;
    const char* artist;
    const char* album;
    uint32_t durationMs;
    uint32_t progressMs;
};

static const MockTrack MOCK_TRACKS[] = {
    { "Blinding Lights", "The Weeknd", "After Hours", 200040, 102000 },
    { "Starboy", "The Weeknd ft. Daft Punk", "Starboy", 230450, 45000 },
    { "As It Was", "Harry Styles", "Harry's House", 167380, 12000 },
    { "Levitating", "Dua Lipa", "Future Nostalgia", 203064, 85000 }
};
static const size_t NUM_MOCK_TRACKS = sizeof(MOCK_TRACKS) / sizeof(MOCK_TRACKS[0]);
static size_t currentMockIndex = 0;

// Application State
static SpotifyTrackState currentTrack;
static SpotifyControlSelection currentSelection = CTRL_PLAYPAUSE;
static bool wantsExit = false;

// Polling & Timing
static unsigned long lastPollMillis = 0;
static const unsigned long POLL_INTERVAL_MS = 2500;

// Joystick deadzone state
static bool joyCentered = true;
static int joySwLastState = HIGH;
static unsigned long lastSwDebounce = 0;

static void loadMockTrack(size_t index) {
    if (index >= NUM_MOCK_TRACKS) index = 0;
    const MockTrack &m = MOCK_TRACKS[index];

    currentTrack.ok = true;
    currentTrack.connected = true;
    currentTrack.playing = true;
    currentTrack.trackId = "mock_" + String(index);
    currentTrack.title = m.title;
    currentTrack.artist = m.artist;
    currentTrack.album = m.album;
    currentTrack.durationMs = m.durationMs;
    currentTrack.progressMs = m.progressMs;
    currentTrack.lastSyncMillis = millis();
}

void SpotifyApp::init(Adafruit_ST7735 &tft) {
    wantsExit = false;
    currentSelection = CTRL_PLAYPAUSE;
    joyCentered = true;
    pinMode(PIN_JOY_SW, INPUT_PULLUP);

    SpotifyUI::init(tft);

    if (SPOTIFY_MOCK_MODE) {
        currentMockIndex = 0;
        loadMockTrack(currentMockIndex);
        SpotifyUI::drawFullUI(tft, currentTrack, currentSelection);
    } else {
        currentTrack = SpotifyTrackState();
        currentTrack.title = "Connecting...";
        currentTrack.artist = "Bridge Host: " SPOTIFY_BRIDGE_HOST;
        currentTrack.connected = SpotifyConnection::ensureWiFiConnected();
        
        SpotifyUI::drawFullUI(tft, currentTrack, currentSelection);

        // Immediate first attempt to fetch state
        if (SpotifyConnection::fetchState(currentTrack)) {
            SpotifyUI::drawFullUI(tft, currentTrack, currentSelection);
        }
        lastPollMillis = millis();
    }
}

void SpotifyApp::update(Adafruit_ST7735 &tft) {
    unsigned long now = millis();

    // 1. Process BACK Button -> Request Exit
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

    // 3. Process Play/Pause Click (From Joystick Click Pin 32 OR Enter Button Pin 13)
    bool actionTriggered = joySwClicked || isEnterPressed();

    if (actionTriggered) {
        if (SPOTIFY_MOCK_MODE) {
            if (currentSelection == CTRL_PREV) {
                currentMockIndex = (currentMockIndex == 0) ? (NUM_MOCK_TRACKS - 1) : (currentMockIndex - 1);
                loadMockTrack(currentMockIndex);
            } else if (currentSelection == CTRL_NEXT) {
                currentMockIndex = (currentMockIndex + 1) % NUM_MOCK_TRACKS;
                loadMockTrack(currentMockIndex);
            } else {
                // Play / Pause toggle
                currentTrack.playing = !currentTrack.playing;
                currentTrack.lastSyncMillis = now;
            }
            SpotifyUI::drawFullUI(tft, currentTrack, currentSelection);
        } else {
            if (currentSelection == CTRL_PREV) {
                SpotifyConnection::sendPrevious();
                delay(100);
                SpotifyConnection::fetchState(currentTrack);
            } else if (currentSelection == CTRL_NEXT) {
                SpotifyConnection::sendNext();
                delay(100);
                SpotifyConnection::fetchState(currentTrack);
            } else {
                SpotifyConnection::sendPlayPause();
                currentTrack.playing = !currentTrack.playing;
                currentTrack.lastSyncMillis = now;
            }
            SpotifyUI::drawFullUI(tft, currentTrack, currentSelection);
        }
    }

    // 4. Process Joystick Navigation (Left / Right / Up / Down)
    int vrx = analogRead(PIN_JOY_X);
    int vry = analogRead(PIN_JOY_Y);

    bool isCentered = (vrx > 1500 && vrx < 2700 && vry > 1500 && vry < 2700);

    if (isCentered) {
        joyCentered = true;
    } else if (joyCentered) {
        if (vrx < 1000) {
            // JOYSTICK LEFT -> Previous Track Action / Focus
            joyCentered = false;
            currentSelection = CTRL_PREV;

            if (SPOTIFY_MOCK_MODE) {
                currentMockIndex = (currentMockIndex == 0) ? (NUM_MOCK_TRACKS - 1) : (currentMockIndex - 1);
                loadMockTrack(currentMockIndex);
                SpotifyUI::drawFullUI(tft, currentTrack, currentSelection);
            } else {
                SpotifyConnection::sendPrevious();
                delay(100);
                SpotifyConnection::fetchState(currentTrack);
                SpotifyUI::drawFullUI(tft, currentTrack, currentSelection);
            }
        } else if (vrx > 3000) {
            // JOYSTICK RIGHT -> Next Track Action / Focus
            joyCentered = false;
            currentSelection = CTRL_NEXT;

            if (SPOTIFY_MOCK_MODE) {
                currentMockIndex = (currentMockIndex + 1) % NUM_MOCK_TRACKS;
                loadMockTrack(currentMockIndex);
                SpotifyUI::drawFullUI(tft, currentTrack, currentSelection);
            } else {
                SpotifyConnection::sendNext();
                delay(100);
                SpotifyConnection::fetchState(currentTrack);
                SpotifyUI::drawFullUI(tft, currentTrack, currentSelection);
            }
        } else if (vry < 1000 || vry > 3000) {
            // JOYSTICK UP/DOWN -> Reset focus back to Play/Pause
            joyCentered = false;
            currentSelection = CTRL_PLAYPAUSE;
            SpotifyUI::drawFullUI(tft, currentTrack, currentSelection);
        }
    }

    // 5. Periodic Background Polling & Dynamic UI Refresh
    if (SPOTIFY_MOCK_MODE) {
        // In mock mode, if playing and progress reaches end of duration, loop mock track
        if (currentTrack.playing && currentTrack.estimatedProgressMs() >= currentTrack.durationMs) {
            currentMockIndex = (currentMockIndex + 1) % NUM_MOCK_TRACKS;
            loadMockTrack(currentMockIndex);
            SpotifyUI::drawFullUI(tft, currentTrack, currentSelection);
        } else {
            SpotifyUI::updateDynamicUI(tft, currentTrack, currentSelection, false);
        }
    } else {
        bool stateFetched = false;
        if (now - lastPollMillis >= POLL_INTERVAL_MS) {
            lastPollMillis = now;
            stateFetched = SpotifyConnection::fetchState(currentTrack);
        }
        SpotifyUI::updateDynamicUI(tft, currentTrack, currentSelection, stateFetched);
    }
}

bool SpotifyApp::shouldExit() {
    if (wantsExit) {
        wantsExit = false;
        return true;
    }
    return false;
}
