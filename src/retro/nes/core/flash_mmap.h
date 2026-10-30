#ifndef FLASH_MMAP_H
#define FLASH_MMAP_H

#include <stdint.h>

struct MappedROM {
    const uint8_t* prg_base = nullptr;
    const uint8_t* chr_base = nullptr;
    uint32_t prg_size = 0;
    uint32_t chr_size = 0;
};

#endif // FLASH_MMAP_H
