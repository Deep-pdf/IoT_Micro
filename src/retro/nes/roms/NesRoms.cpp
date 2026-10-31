#include "NesRoms.h"

const NesRom NES_ROM_MARIO = {
    .title = "Super Mario Bros. 3",
    .data = rom_mario_data,
    .size = rom_mario_size,
    .mapper = 4
};

const NesRom NES_ROM_CONTRA = {
    .title = "Contra",
    .data = rom_contra_data,
    .size = rom_contra_size,
    .mapper = 2
};

const NesRom* NesGetRom(NesGameId game) {
    switch (game) {
        case NES_GAME_MARIO:
            return &NES_ROM_MARIO;
        case NES_GAME_CONTRA:
            return &NES_ROM_CONTRA;
        default:
            return nullptr;
    }
}

