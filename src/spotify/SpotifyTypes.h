#ifndef SPOTIFY_TYPES_H
#define SPOTIFY_TYPES_H

#include <Arduino.h>

enum SpotifyControlSelection {
    CTRL_PREV = 0,
    CTRL_PLAYPAUSE = 1,
    CTRL_NEXT = 2
};

struct SpotifyTrackState {
    bool ok = false;
    bool connected = false;
    bool playing = false;

    String trackId = "";
    String title = "";
    String artist = "";
    String album = "";

    uint32_t progressMs = 0;
    uint32_t durationMs = 0;

    String artworkUrl = "";
    unsigned long lastSyncMillis = 0;

    uint32_t estimatedProgressMs() const {
        if (durationMs == 0) return 0;
        if (!playing) return progressMs;
        unsigned long elapsed = millis() - lastSyncMillis;
        uint32_t current = progressMs + (uint32_t)elapsed;
        if (current > durationMs) current = durationMs;
        return current;
    }
};

#endif // SPOTIFY_TYPES_H
