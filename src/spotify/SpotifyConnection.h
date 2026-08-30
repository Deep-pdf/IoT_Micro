#ifndef SPOTIFY_CONNECTION_H
#define SPOTIFY_CONNECTION_H

#include <Arduino.h>
#include "SpotifyTypes.h"

class SpotifyConnection {
public:
    static bool ensureWiFiConnected();
    static bool getState(SpotifyTrackState &state);
    static bool fetchArtwork(uint16_t *buffer, size_t maxPixels, int targetSize = 40);
    static bool sendPlayPause();
    static bool sendNext();
    static bool sendPrevious();

private:
    static bool postEndpoint(const char* path);
};

#endif // SPOTIFY_CONNECTION_H
