#ifndef HOME_SCREEN_H
#define HOME_SCREEN_H

#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>

// Draws the static home screen elements in landscape orientation (Rotation 1)
void drawHomeScreen(Adafruit_ST7735 &tft);

// Updates the Wi-Fi icon color in the status bar (green if connected, white if disconnected)
void updateWiFiIcon(Adafruit_ST7735 &tft, bool connected);

// Updates the clock and day/date elements dynamically if they change
void updateTimeAndDate(Adafruit_ST7735 &tft);

#endif // HOME_SCREEN_H
