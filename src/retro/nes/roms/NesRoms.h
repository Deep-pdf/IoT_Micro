#ifndef NES_ROMS_H
#define NES_ROMS_H

#include "../NesRom.h"

extern const uint8_t rom_mario_data[] PROGMEM;
extern const size_t rom_mario_size;

extern const uint8_t rom_contra_data[] PROGMEM;
extern const size_t rom_contra_size;

extern const NesRom NES_ROM_MARIO;
extern const NesRom NES_ROM_CONTRA;

#endif // NES_ROMS_H

