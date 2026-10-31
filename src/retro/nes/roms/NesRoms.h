#ifndef NES_ROMS_H
#define NES_ROMS_H

#include "../NesRom.h"

#ifdef __cplusplus
extern "C" {
#endif

extern const uint8_t rom_mario_data[];
extern const uint32_t rom_mario_size;

extern const uint8_t rom_contra_data[];
extern const uint32_t rom_contra_size;

#ifdef __cplusplus
}
#endif

extern const NesRom NES_ROM_MARIO;
extern const NesRom NES_ROM_CONTRA;

#endif // NES_ROMS_H
