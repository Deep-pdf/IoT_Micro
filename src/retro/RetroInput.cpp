#include "RetroInput.h"
#include "joystick.h"
#include "button.h"

void RetroInput::init() {
    Joystick.begin();
}

RetroNavEvent RetroInput::readNavigation() {
    Joystick.update();
    if (Joystick.wasDownPressed()) return RETRO_NAV_DOWN;
    if (Joystick.wasUpPressed())   return RETRO_NAV_UP;
    return RETRO_NAV_NONE;
}

bool RetroInput::isLaunchPressed() {
    // 1. Enter Pushbutton on GPIO 13
    if (isEnterPressed()) {
        return true;
    }

    // 2. Joystick Switch Click on GPIO 32
    return Joystick.wasClickPressed();
}

bool RetroInput::isExitPressed() {
    return isBackPressed();
}
