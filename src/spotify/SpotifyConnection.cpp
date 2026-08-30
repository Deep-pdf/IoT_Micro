#include "SpotifyConnection.h"
#include "config.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

#ifndef SPOTIFY_BRIDGE_HOST
#define SPOTIFY_BRIDGE_HOST "10.240.13.7"
#endif

#ifndef SPOTIFY_BRIDGE_PORT
#define SPOTIFY_BRIDGE_PORT 8888
#endif

static const unsigned long HTTP_TIMEOUT_MS = 4000;
static String lastLoggedTrackId = "";
static bool lastLoggedPlaying = false;

bool SpotifyConnection::ensureWiFiConnected() {
    return (WiFi.status() == WL_CONNECTED);
}

bool SpotifyConnection::getState(SpotifyTrackState &state) {
    if (!ensureWiFiConnected()) {
        state.connected = false;
        state.ok = false;
        return false;
    }

    char url[128];
    snprintf(url, sizeof(url), "http://%s:%d/spotify/state", SPOTIFY_BRIDGE_HOST, SPOTIFY_BRIDGE_PORT);

    HTTPClient http;
    http.begin(url);
    http.setTimeout(HTTP_TIMEOUT_MS);

    int httpCode = http.GET();

    if (httpCode <= 0) {
        http.end();
        state.connected = false;
        state.ok = false;
        return false;
    }

    if (httpCode != 200) {
        http.end();
        state.connected = false;
        state.ok = false;
        return false;
    }

    String payload = http.getString();
    http.end();

    if (payload.isEmpty()) {
        state.connected = false;
        state.ok = false;
        return false;
    }

#if ARDUINOJSON_VERSION_MAJOR >= 7
    JsonDocument doc;
#else
    StaticJsonDocument<1024> doc;
#endif

    DeserializationError error = deserializeJson(doc, payload);
    if (error) {
        Serial.print("[Spotify] JSON parse error: ");
        Serial.println(error.c_str());
        state.connected = true;
        state.ok = false;
        return false;
    }

    state.connected = true;
    state.ok = doc["ok"] | false;
    state.playing = doc["playing"] | false;
    state.trackId = doc["track_id"].is<const char*>() ? doc["track_id"].as<String>() : "";
    state.title = doc["title"].is<const char*>() ? doc["title"].as<String>() : "";
    state.artist = doc["artist"].is<const char*>() ? doc["artist"].as<String>() : "";
    state.album = doc["album"].is<const char*>() ? doc["album"].as<String>() : "";
    state.progressMs = doc["progress_ms"] | 0;
    state.durationMs = doc["duration_ms"] | 0;
    state.artworkUrl = doc["artwork_url"].is<const char*>() ? doc["artwork_url"].as<String>() : "";
    state.lastSyncMillis = millis();

    // Log to Serial only when track or playback state changes
    if (state.trackId != lastLoggedTrackId || state.playing != lastLoggedPlaying) {
        lastLoggedTrackId = state.trackId;
        lastLoggedPlaying = state.playing;

        Serial.println("================================");
        Serial.println("Spotify State Synchronized");
        Serial.print("Playing: ");
        Serial.println(state.playing ? "YES" : "PAUSED / IDLE");
        Serial.print("Title:   ");
        Serial.println(state.title.isEmpty() ? "(No Track)" : state.title);
        Serial.print("Artist:  ");
        Serial.println(state.artist.isEmpty() ? "(None)" : state.artist);
        Serial.print("Album:   ");
        Serial.println(state.album.isEmpty() ? "(None)" : state.album);
        Serial.print("Progress: ");
        Serial.print(state.progressMs / 1000);
        Serial.print("s / ");
        Serial.print(state.durationMs / 1000);
        Serial.println("s");
        Serial.println("================================");
    }

    return true;
}
