#ifndef NES_ROM_H
#define NES_ROM_H

#include <Arduino.h>

enum NesGameId {
    NES_GAME_NONE = 0,
    NES_GAME_MARIO,
    NES_GAME_CONTRA
};

struct NesRom {
    const char* title;
    const uint8_t* data;
    size_t size;
    uint8_t mapper;
};

// Access pre-integrated flash ROMs
const NesRom* NesGetRom(NesGameId game);

#endif // NES_ROM_H

