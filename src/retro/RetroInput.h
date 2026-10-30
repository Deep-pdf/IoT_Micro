#ifndef RETRO_INPUT_H
#define RETRO_INPUT_H

#include <Arduino.h>

enum RetroNavEvent {
    RETRO_NAV_NONE = 0,
    RETRO_NAV_UP,
    RETRO_NAV_DOWN
};

class RetroInput {
public:
    static void init();
    static RetroNavEvent readNavigation();
    static bool isLaunchPressed();
    static bool isExitPressed();
};

#endif // RETRO_INPUT_H

