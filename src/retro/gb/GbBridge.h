#ifndef GB_BRIDGE_H
#define GB_BRIDGE_H

#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <stdbool.h>

class GbBridge {
public:
    // Initializes the Game Boy (Gnuboy) engine, ROM bridge with 4-slot LRU cache,
    // display adapter (128x144 vertical native, stroke-preserving filter),
    // and resets the Game Boy core for Pokémon Red.
    static bool init(Adafruit_ST7735* display);

    // Executes one complete emulated Game Boy frame with actual TFT rendering
    static void runFrame();

    // Stops emulation, releases Gnuboy core, display adapter, and 4-slot ROM cache
    static void stop();

    // Returns true if the Game Boy emulator is currently active
    static bool isRunning();

    // Returns true if user held BTN_BACK >= 2s to request exit
    static bool isExitRequested();

    // Waits until BTN_BACK is released and clears button event queues
    static void waitForExitRelease();
};

#endif // GB_BRIDGE_H

