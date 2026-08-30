#ifndef SPOTIFY_APP_H
#define SPOTIFY_APP_H

#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>

class SpotifyApp {
public:
    // Called once when the user enters the Spotify app from Apps Page 2
    static void init(Adafruit_ST7735 &tft);

    // Called every loop() iteration while currentScreen == STATE_SPOTIFY
    static void update(Adafruit_ST7735 &tft);

    // Returns true (and clears flag) when BACK is pressed to return to Home
    static bool shouldExit();
};

#endif // SPOTIFY_APP_H
