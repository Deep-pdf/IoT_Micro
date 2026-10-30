#ifndef NES_ROM_H
#define NES_ROM_H

#include <Arduino.h>

struct NesRom {
    const char* title;
    const uint8_t* data;
    size_t size;
    uint8_t mapper;
};

#endif // NES_ROM_H

