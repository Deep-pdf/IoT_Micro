#ifndef NES_DISPLAY_ADAPTER_H
#define NES_DISPLAY_ADAPTER_H

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>

class NesDisplayAdapter {
public:
    // Bind existing Adafruit_ST7735 display instance
    static void init(Adafruit_ST7735* display);

    // Clears the top and bottom 20px letterbox bands to pure black
    static void clearBorders();

    // Sets up the 128x120 active window on ST7735 (Y=20..139)
    static void startFrame();

    // Receives a 256-pixel NES scanline and performs integer 2:1 downscaling to 128 pixels
    static void renderScanline(int scanline, const uint16_t* nesScanline256);

    // Completes the frame write
    static void endFrame();
};

#endif // NES_DISPLAY_ADAPTER_H

