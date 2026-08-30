#ifndef SPOTIFY_APP_H
#define SPOTIFY_APP_H

#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>

class SpotifyApp {
public:
    // Called once when entering the Spotify screen
    static void init(Adafruit_ST7735 &tft);

    // Called in loop() while in Spotify screen
    static void update(Adafruit_ST7735 &tft);

    // Returns true when user presses BACK to return to Home Screen
    static bool shouldExit();
};

#endif // SPOTIFY_APP_H
