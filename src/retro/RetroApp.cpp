#include "RetroApp.h"
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
