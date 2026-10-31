#include "RetroApp.h"

#ifdef NES_HARDWARE_TEST
// ---- Step 3B standalone NES hardware test ----
// Remove NES_HARDWARE_TEST define in platformio.ini (build_flags) to restore the launcher.
#include "NesTest.h"

void RetroApp::init(Adafruit_ST7735 &tft) {
    NesTest::init(tft);
}

void RetroApp::update(Adafruit_ST7735 &tft) {
    NesTest::update(tft);
}

bool RetroApp::shouldExit() {
    return NesTest::shouldExit();
}

#else
// ---- Normal launcher mode ----
#include "RetroLauncher.h"

void RetroApp::init(Adafruit_ST7735 &tft) {
    RetroLauncher::init(tft);
}

void RetroApp::update(Adafruit_ST7735 &tft) {
    RetroLauncher::update(tft);
}

bool RetroApp::shouldExit() {
    return RetroLauncher::shouldExit();
}
#endif
