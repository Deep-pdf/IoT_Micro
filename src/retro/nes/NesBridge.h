#ifndef NES_BRIDGE_H
#define NES_BRIDGE_H

#include <Arduino.h>
#include <Adafruit_ST7735.h>
#include "NesRom.h"

class NesBridge {
public:
    // Initialize the NES engine and bind to the ST7735 display adapter
    static bool init(Adafruit_ST7735* display);

    // Load an NES ROM from memory / flash
    static bool loadRom(const uint8_t* romData, size_t romSize);
    static bool loadRom(const NesRom& rom);

    // Set and get the 8-bit NES controller state (Bit 0: A, 1: B, 2: Select, 3: Start, 4: Up, 5: Down, 6: Left, 7: Right)
    static void setController(uint8_t state);
    static uint8_t getController();

    // Emulate and render a single frame
    static void runFrame();

    // Reset emulation
    static void reset();

    // Stop emulation and free cartridge resources
    static void stop();

    // Returns whether an NES game is currently active
    static bool isRunning();
};

#endif // NES_BRIDGE_H

