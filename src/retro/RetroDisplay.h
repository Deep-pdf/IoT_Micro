#ifndef RETRO_DISPLAY_H
#define RETRO_DISPLAY_H

#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>

class RetroDisplay {
public:
    // Decodes PackBits16 image stream directly into a single 128-pixel line buffer
    // and writes each line over hardware SPI to ST7735.
    // Memory overhead: exactly 256 bytes of static RAM, 0 bytes dynamic heap allocation.
    static void drawFrame(Adafruit_ST7735 &tft, const uint8_t *compressedData, size_t dataSize);

    // Displays a sleek launch notification card overlay
    static void drawLaunchNotification(Adafruit_ST7735 &tft, const char *gameTitle);

    // Displays an unavailable notification card overlay (e.g. for Pokémon)
    static void drawUnavailableNotification(Adafruit_ST7735 &tft, const char *gameTitle);
};

#endif // RETRO_DISPLAY_H
