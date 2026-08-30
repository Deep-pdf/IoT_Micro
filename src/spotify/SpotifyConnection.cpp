#include "SpotifyConnection.h"
#include "config.h"
#include <WiFi.h>
#include <HTTPClient.h>

#ifndef SPOTIFY_BRIDGE_HOST
#define SPOTIFY_BRIDGE_HOST "192.168.1.100"
#endif

#ifndef SPOTIFY_BRIDGE_PORT
#define SPOTIFY_BRIDGE_PORT 8888
#endif

// Reasonable non-blocking timeout for responsive UI (1.5 seconds)
static const unsigned long SPOTIFY_HTTP_TIMEOUT_MS = 1500;

String SpotifyConnection::getEndpointUrl(const char* path) {
    char urlBuffer[128];
    snprintf(urlBuffer, sizeof(urlBuffer), "http://%s:%d%s", SPOTIFY_BRIDGE_HOST, SPOTIFY_BRIDGE_PORT, path);
    return String(urlBuffer);
}

bool SpotifyConnection::ensureWiFiConnected() {
    return (WiFi.status() == WL_CONNECTED);
}

// Helper to extract string values from JSON
static String extractJsonString(const String &json, const char *key) {
    String pattern = String("\"") + key + "\"";
    int index = json.indexOf(pattern);
    if (index == -1) return "";

    int colonIndex = json.indexOf(':', index + pattern.length());
    if (colonIndex == -1) return "";

    // Find first non-space character after colon
    int startIdx = colonIndex + 1;
    while (startIdx < (int)json.length() && (json[startIdx] == ' ' || json[startIdx] == '\t' || json[startIdx] == '\r' || json[startIdx] == '\n')) {
        startIdx++;
    }

    if (startIdx >= (int)json.length()) return "";

    // If it's null, return empty
    if (json.substring(startIdx, startIdx + 4) == "null") {
        return "";
    }

    if (json[startIdx] != '\"') return "";

    String result = "";
    for (int i = startIdx + 1; i < (int)json.length(); i++) {
        char c = json[i];
        if (c == '\\') {
            if (i + 1 < (int)json.length()) {
                char next = json[i + 1];
                if (next == 'n') result += '\n';
                else if (next == '\"') result += '\"';
                else if (next == '\\') result += '\\';
                else if (next == 't') result += ' ';
                else result += next;
                i++;
            }
        } else if (c == '\"') {
            break;
        } else {
            result += c;
        }
    }
    return result;
}

// Helper to extract boolean values from JSON
static bool extractJsonBool(const String &json, const char *key, bool defaultValue = false) {
    String pattern = String("\"") + key + "\"";
    int index = json.indexOf(pattern);
    if (index == -1) return defaultValue;

    int colonIndex = json.indexOf(':', index + pattern.length());
    if (colonIndex == -1) return defaultValue;

    int startIdx = colonIndex + 1;
    while (startIdx < (int)json.length() && (json[startIdx] == ' ' || json[startIdx] == '\t')) {
        startIdx++;
    }

    if (json.substring(startIdx, startIdx + 4) == "true") return true;
    if (json.substring(startIdx, startIdx + 5) == "false") return false;
    return defaultValue;
}

// Helper to extract integer values from JSON
static uint32_t extractJsonInt(const String &json, const char *key, uint32_t defaultValue = 0) {
    String pattern = String("\"") + key + "\"";
    int index = json.indexOf(pattern);
    if (index == -1) return defaultValue;

    int colonIndex = json.indexOf(':', index + pattern.length());
    if (colonIndex == -1) return defaultValue;

    int startIdx = colonIndex + 1;
    while (startIdx < (int)json.length() && (json[startIdx] == ' ' || json[startIdx] == '\t')) {
        startIdx++;
    }

    // Read digits
    String numStr = "";
    for (int i = startIdx; i < (int)json.length(); i++) {
        char c = json[i];
        if (c >= '0' && c <= '9') {
            numStr += c;
        } else {
            break;
        }
    }

    if (numStr.length() > 0) {
        return (uint32_t)strtoul(numStr.c_str(), NULL, 10);
    }
    return defaultValue;
}

bool SpotifyConnection::getEndpoint(const char* path, String &outResponse) {
    if (!ensureWiFiConnected()) {
        return false;
    }

    HTTPClient http;
    String url = getEndpointUrl(path);
    http.begin(url);
    http.setTimeout(SPOTIFY_HTTP_TIMEOUT_MS);

    int httpCode = http.GET();
    if (httpCode == HTTP_CODE_OK) {
        outResponse = http.getString();
        http.end();
        return true;
    } else {
        http.end();
        return false;
    }
}

bool SpotifyConnection::postEndpoint(const char* path, String &outResponse) {
    if (!ensureWiFiConnected()) {
        return false;
    }

    HTTPClient http;
    String url = getEndpointUrl(path);
    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    http.setTimeout(SPOTIFY_HTTP_TIMEOUT_MS);

    int httpCode = http.POST("{}");
    if (httpCode > 0) {
        outResponse = http.getString();
        http.end();
        return (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_NO_CONTENT);
    } else {
        http.end();
        return false;
    }
}

bool SpotifyConnection::fetchState(SpotifyTrackState &outState) {
    String response = "";
    bool success = getEndpoint("/spotify/state", response);
    
    if (!success || response.isEmpty()) {
        outState.connected = false;
        outState.ok = false;
        return false;
    }

    outState.connected = true;
    outState.ok = extractJsonBool(response, "ok", false);
    outState.playing = extractJsonBool(response, "playing", false);
    outState.trackId = extractJsonString(response, "track_id");
    outState.title = extractJsonString(response, "title");
    outState.artist = extractJsonString(response, "artist");
    outState.album = extractJsonString(response, "album");
    outState.artworkUrl = extractJsonString(response, "artwork_url");
    outState.durationMs = extractJsonInt(response, "duration_ms", 0);
    outState.progressMs = extractJsonInt(response, "progress_ms", 0);
    outState.lastSyncMillis = millis();

    return true;
}

bool SpotifyConnection::sendPlayPause() {
    String dummy = "";
    return postEndpoint("/spotify/playpause", dummy);
}

bool SpotifyConnection::sendPlay() {
    String dummy = "";
    return postEndpoint("/spotify/play", dummy);
}

bool SpotifyConnection::sendPause() {
    String dummy = "";
    return postEndpoint("/spotify/pause", dummy);
}

bool SpotifyConnection::sendNext() {
    String dummy = "";
    return postEndpoint("/spotify/next", dummy);
}

bool SpotifyConnection::sendPrevious() {
    String dummy = "";
    return postEndpoint("/spotify/previous", dummy);
}
