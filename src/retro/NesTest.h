#ifndef NES_TEST_H
#define NES_TEST_H

#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>

/**
 * NesTest — Step 3B standalone hardware verification.
 *
 * Loads Super Mario Bros. 3 via NesBridge::loadGame(NES_GAME_MARIO)
 * and runs the emulation loop directly using the existing 128x160 TFT.
 *
 * Controls mapped from existing hardware:
 *   Joystick Up/Down/Left/Right  -> NES Up/Down/Left/Right
 *   GPIO 13 (BTN_ENTER)          -> NES A
 *   GPIO 32 (JOY_SW)             -> NES B
 *   GPIO 25 (BTN_BACK)           -> NES Start
 *
 * Exit: hold BTN_BACK (GPIO 25) for ~2 seconds.
 */
class NesTest {
public:
    static void init(Adafruit_ST7735& tft);
    static void update(Adafruit_ST7735& tft);
    static bool shouldExit();
};

#endif // NES_TEST_H

