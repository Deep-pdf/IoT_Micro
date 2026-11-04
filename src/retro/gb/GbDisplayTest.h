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
    // Executes targetFrames (default: 1200 frames = ~20-40 seconds of runtime emulation)
    // with actual nearest-neighbor scaling and 40 MHz SPI transfers to the physical ST7735 TFT.
    // Logs comprehensive performance telemetry to Serial once per second (every 60 frames).
    // Monitors BTN_BACK (GPIO 25) for early exit and BTN_ENTER (GPIO 13) for Game Boy START.
    // Shuts down cleanly on completion, releasing the framebuffer and verifying zero memory leaks.
    static bool run(Adafruit_ST7735& tft, uint32_t targetFrames = 1200);

    // Modular lifecycle methods
    static bool init(Adafruit_ST7735& tft);
    static void update(Adafruit_ST7735& tft);
    static void deinit();
    static bool isRunning();
};

#endif // GB_DISPLAY_TEST_H

