#include "SpotifyConnection.h"
#include "config.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

#ifndef SPOTIFY_BRIDGE_HOST
#define SPOTIFY_BRIDGE_HOST "192.168.1.100"
#endif

#ifndef SPOTIFY_BRIDGE_PORT
#define SPOTIFY_BRIDGE_PORT 8888
#endif

bool SpotifyConnection::ensureWiFiConnected() {
    if (WiFi.status() == WL_CONNECTED) {
        return true;
    }
    Serial.println("WiFi not connected");
    return false;
}

bool SpotifyConnection::getState(SpotifyTrackState &state) {
    // Reset state to safe defaults
    state.ok = false;
    state.playing = false;
    state.trackId = "";
    state.title = "";
    state.artist = "";
    state.album = "";
    state.progressMs = 0;
    state.durationMs = 0;
    state.artworkUrl = "";

    Serial.println("================================");
    Serial.println("SPOTIFY BRIDGE TEST");
    Serial.println("================================");
    Serial.println();
    Serial.println("Bridge:");
    Serial.print("http://");
    Serial.print(SPOTIFY_BRIDGE_HOST);
    Serial.print(":");
    Serial.println(SPOTIFY_BRIDGE_PORT);
    Serial.println();
    Serial.println("Requesting Spotify state...");
    Serial.println();

    if (!ensureWiFiConnected()) {
        Serial.println("Spotify bridge connection FAILED");
        Serial.println("WiFi not connected");
        Serial.println("================================");
        return false;
    }

    char url[128];
    snprintf(url, sizeof(url), "http://%s:%d/spotify/state", SPOTIFY_BRIDGE_HOST, SPOTIFY_BRIDGE_PORT);

    HTTPClient http;
    http.begin(url);
    http.setTimeout(10000); // 10s timeout

    int httpCode = http.GET();

    if (httpCode <= 0) {
        Serial.println("Spotify bridge connection FAILED");
        Serial.print("HTTP request failed: ");
        Serial.println(http.errorToString(httpCode).c_str());
        Serial.println("================================");
        http.end();
        return false;
    }

    if (httpCode != 200) {
        Serial.println("Spotify bridge connection FAILED");
        Serial.print("HTTP status: ");
        Serial.println(httpCode);
        Serial.println("================================");
        http.end();
        return false;
    }

    String payload = http.getString();
    http.end();

    if (payload.isEmpty()) {
        Serial.println("Spotify bridge connection FAILED");
        Serial.println("HTTP request failed: Empty response payload");
        Serial.println("================================");
        return false;
    }

#if ARDUINOJSON_VERSION_MAJOR >= 7
    JsonDocument doc;
#else
    StaticJsonDocument<1024> doc;
#endif

    DeserializationError error = deserializeJson(doc, payload);
    if (error) {
        Serial.println("Spotify bridge connection FAILED");
        Serial.print("HTTP request failed: JSON parse error (");
        Serial.print(error.c_str());
        Serial.println(")");
        Serial.println("================================");
        return false;
    }

    state.ok = doc["ok"] | false;
    state.playing = doc["playing"] | false;
    state.trackId = doc["track_id"].is<const char*>() ? doc["track_id"].as<String>() : "";
    state.title = doc["title"].is<const char*>() ? doc["title"].as<String>() : "";
    state.artist = doc["artist"].is<const char*>() ? doc["artist"].as<String>() : "";
    state.album = doc["album"].is<const char*>() ? doc["album"].as<String>() : "";
    state.progressMs = doc["progress_ms"] | 0;
    state.durationMs = doc["duration_ms"] | 0;
    state.artworkUrl = doc["artwork_url"].is<const char*>() ? doc["artwork_url"].as<String>() : "";

    Serial.println("Spotify bridge connected!");
    Serial.println();
    Serial.print("Playing: ");
    Serial.println(state.playing ? "YES" : "NO");
    Serial.println();
    Serial.println("Title:");
    Serial.println(state.title);
    Serial.println();
    Serial.println("Artist:");
    Serial.println(state.artist);
    Serial.println();
    Serial.println("Album:");
    Serial.println(state.album);
    Serial.println();
    Serial.println("Progress:");
    Serial.print(state.progressMs);
    Serial.println(" ms");
    Serial.println();
    Serial.println("Duration:");
    Serial.print(state.durationMs);
    Serial.println(" ms");
    Serial.println();
    Serial.println("Artwork:");
    Serial.println(state.artworkUrl);
    Serial.println("================================");

    return true;
}
