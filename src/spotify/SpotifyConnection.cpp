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

// Fast non-blocking timeout: local Wi-Fi takes ~15ms; if no response within 800ms, don't stall the UI
static const unsigned long HTTP_TIMEOUT_MS = 800;
static String lastLoggedTrackId = "";
static bool lastLoggedPlaying = false;
static int consecutiveFailures = 0;

bool SpotifyConnection::ensureWiFiConnected() {
    return (WiFi.status() == WL_CONNECTED);
}

bool SpotifyConnection::postEndpoint(const char* path) {
    if (!ensureWiFiConnected()) {
        return false;
    }

    char url[128];
    snprintf(url, sizeof(url), "http://%s:%d%s", SPOTIFY_BRIDGE_HOST, SPOTIFY_BRIDGE_PORT, path);

    HTTPClient http;
    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Connection", "close");
    http.setTimeout(HTTP_TIMEOUT_MS);

    int httpCode = http.POST("{}");
    http.end();

    return (httpCode == 200 || httpCode == 204);
}

bool SpotifyConnection::sendPlayPause() {
    Serial.println("[Spotify] -> POST /spotify/playpause");
    return postEndpoint("/spotify/playpause");
}

bool SpotifyConnection::sendNext() {
    Serial.println("[Spotify] -> POST /spotify/next");
    return postEndpoint("/spotify/next");
}

bool SpotifyConnection::sendPrevious() {
    Serial.println("[Spotify] -> POST /spotify/previous");
    return postEndpoint("/spotify/previous");
}

bool SpotifyConnection::getState(SpotifyTrackState &state) {
    if (!ensureWiFiConnected()) {
        consecutiveFailures++;
        if (consecutiveFailures >= 3) {
            state.connected = false;
        }
        return false;
    }

    char url[128];
    snprintf(url, sizeof(url), "http://%s:%d/spotify/state", SPOTIFY_BRIDGE_HOST, SPOTIFY_BRIDGE_PORT);

    HTTPClient http;
    http.begin(url);
    http.addHeader("Connection", "close");
    http.setTimeout(HTTP_TIMEOUT_MS);

    int httpCode = http.GET();

    if (httpCode != 200) {
        http.end();
        consecutiveFailures++;
        if (consecutiveFailures >= 3) {
            state.connected = false;
        }
        return false;
    }

    String payload = http.getString();
    http.end();

    if (payload.isEmpty()) {
        consecutiveFailures++;
        if (consecutiveFailures >= 3) {
            state.connected = false;
        }
        return false;
    }

#if ARDUINOJSON_VERSION_MAJOR >= 7
    JsonDocument doc;
#else
    StaticJsonDocument<1024> doc;
#endif

    DeserializationError error = deserializeJson(doc, payload);
    if (error) {
        Serial.print("[Spotify] JSON error: ");
        Serial.println(error.c_str());
        consecutiveFailures++;
        if (consecutiveFailures >= 3) {
            state.connected = false;
        }
        return false;
    }

    // Successful fetch -> reset failure counter
    consecutiveFailures = 0;
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

    // Log when track or playing state changes
    if (state.trackId != lastLoggedTrackId || state.playing != lastLoggedPlaying) {
        lastLoggedTrackId = state.trackId;
        lastLoggedPlaying = state.playing;

        Serial.println("--------------------------------");
        Serial.print("[Spotify] Track:  ");
        Serial.println(state.title.isEmpty() ? "(No Track Playing)" : state.title);
        Serial.print("[Spotify] Artist: ");
        Serial.println(state.artist);
        Serial.print("[Spotify] State:  ");
        Serial.println(state.playing ? "PLAYING" : "PAUSED");
        Serial.println("--------------------------------");
    }

    return true;
}

bool SpotifyConnection::fetchArtwork(uint16_t *buffer, size_t maxPixels, int targetSize) {
    if (!ensureWiFiConnected() || !buffer) {
        return false;
    }

    if (maxPixels < (size_t)(targetSize * targetSize)) {
        return false;
    }

    char url[128];
    snprintf(url, sizeof(url), "http://%s:%d/spotify/artwork?size=%d", SPOTIFY_BRIDGE_HOST, SPOTIFY_BRIDGE_PORT, targetSize);

    HTTPClient http;
    http.begin(url);
    http.addHeader("Connection", "close");
    http.setTimeout(1500); // 1.5s max for 4.6KB

    int httpCode = http.GET();
    if (httpCode != 200) {
        http.end();
        return false;
    }

    int expectedBytes = targetSize * targetSize * 2;
    int totalBytes = 0;
    WiFiClient *stream = http.getStreamPtr();

    uint8_t *bytePtr = (uint8_t*)buffer;
    unsigned long start = millis();

    while (http.connected() && (totalBytes < expectedBytes) && (millis() - start < 1500)) {
        size_t available = stream->available();
        if (available) {
            int toRead = min((int)available, expectedBytes - totalBytes);
            int bytesRead = stream->readBytes(bytePtr + totalBytes, toRead);
            totalBytes += bytesRead;
        }
        delay(1);
    }
    http.end();

    if (totalBytes == expectedBytes) {
        // Convert big-endian network bytes to native uint16_t RGB565 format
        for (int i = 0; i < targetSize * targetSize; i++) {
            uint8_t high = bytePtr[i * 2];
            uint8_t low  = bytePtr[i * 2 + 1];
            buffer[i] = (uint16_t)((high << 8) | low);
        }
        return true;
    }

    return false;
}
