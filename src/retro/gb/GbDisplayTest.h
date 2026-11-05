#ifndef GB_DISPLAY_TEST_H
#define GB_DISPLAY_TEST_H

#include <stdint.h>
#include <stdbool.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>

class GbDisplayTest {
public:
    // Executes the standalone Game Boy display integration test:
    // Initializes the TFT, GbRomBridge, Gnuboy core, framebuffer, and resets CPU.
    // Executes continuously with actual nearest-neighbor scaling and 40 MHz SPI transfers
    // to the physical ST7735 TFT until user holds BTN_BACK (GPIO 25) >= 2s to exit.
    // Logs comprehensive performance telemetry to Serial once per second (every 60 frames).
    // Monitors physical controls (Joystick D-pad, GPIO 13 A, GPIO 32 B, GPIO 25 START/EXIT).
    // Shuts down cleanly on exit, releasing the framebuffer and verifying zero memory leaks.
    static bool run(Adafruit_ST7735& tft);

    // Modular lifecycle methods
    static bool init(Adafruit_ST7735& tft);
    static void update(Adafruit_ST7735& tft);
    static void deinit();
    static bool isRunning();
};

#endif // GB_DISPLAY_TEST_H

