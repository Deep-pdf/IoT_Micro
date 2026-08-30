#ifndef SPOTIFY_CONNECTION_H
#define SPOTIFY_CONNECTION_H

#include <Arduino.h>
#include "SpotifyTypes.h"

class SpotifyConnection {
public:
    static bool ensureWiFiConnected();
    static bool fetchState(SpotifyTrackState &outState);
    static bool sendPlayPause();
    static bool sendPlay();
    static bool sendPause();
    static bool sendNext();
    static bool sendPrevious();

private:
    static String getEndpointUrl(const char* path);
    static bool postEndpoint(const char* path, String &outResponse);
    static bool getEndpoint(const char* path, String &outResponse);
};

#endif // SPOTIFY_CONNECTION_H
