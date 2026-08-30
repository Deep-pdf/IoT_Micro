#ifndef SPOTIFY_UI_H
#define SPOTIFY_UI_H

#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include "SpotifyTypes.h"

class SpotifyUI {
public:
    static void init(Adafruit_ST7735 &tft);
    static void drawFullUI(Adafruit_ST7735 &tft, const SpotifyTrackState &state);
    static void updateDynamicUI(Adafruit_ST7735 &tft, const SpotifyTrackState &state, bool stateChanged);
    static void drawProgressBar(Adafruit_ST7735 &tft, uint32_t progressMs, uint32_t durationMs);
    static void drawEqualizerWaves(Adafruit_ST7735 &tft, bool isPlaying);

private:
    static void drawHeader(Adafruit_ST7735 &tft, bool isConnected);
    static void drawArtworkPlaceholder(Adafruit_ST7735 &tft, bool isPlaying);
    static void drawTrackInfo(Adafruit_ST7735 &tft, const String &title, const String &artist, const String &album, bool isPlaying);
    static void drawControlsVisual(Adafruit_ST7735 &tft, bool isPlaying);
    static void drawOfflineState(Adafruit_ST7735 &tft);
    static void drawIdleState(Adafruit_ST7735 &tft);
    static void formatTime(uint32_t ms, char *outBuffer, size_t bufSize);
};

#endif // SPOTIFY_UI_H
