#ifndef SPOTIFY_TYPES_H
#define SPOTIFY_TYPES_H

#include <Arduino.h>

struct SpotifyTrackState {
    bool ok = false;
    bool playing = false;

    String trackId = "";
    String title = "";
    String artist = "";
    String album = "";

    uint32_t progressMs = 0;
    uint32_t durationMs = 0;

    String artworkUrl = "";
};

#endif // SPOTIFY_TYPES_H
