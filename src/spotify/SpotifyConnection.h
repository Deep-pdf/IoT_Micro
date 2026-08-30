#ifndef SPOTIFY_CONNECTION_H
#define SPOTIFY_CONNECTION_H

#include <Arduino.h>
#include "SpotifyTypes.h"

class SpotifyConnection {
public:
    static bool ensureWiFiConnected();
    static bool getState(SpotifyTrackState &state);
};

#endif // SPOTIFY_CONNECTION_H
