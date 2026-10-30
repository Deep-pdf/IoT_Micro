#ifndef MAPPER_H
#define MAPPER_H

#include <cstring>
#include <stdint.h>
#include <stdlib.h>
#include "Arduino.h"
#include "nes_debug.h"
#include "flash_mmap.h"
#include "rom_backends.h"
#include "rom_types.h"

class Cartridge;
class Bus;
class Ppu2C02;

class Mapper
{
public:
    void* state = nullptr;
};

inline void mapperNoScanline(Mapper*)
{
}
inline void mapperNoCycle(Mapper*, int)
{
}

struct Bank
{
    uint8_t bank_id;
    uint8_t* bank_ptr;
    uint32_t last_used;
    uint32_t size;
};

struct BankCache
{
    Bank* banks;
    uint8_t num_banks;
    uint32_t tick;
    Cartridge* cart;
};

void bankInit(BankCache* cache, Bank* banks, uint8_t num_banks, uint32_t bank_size, Cartridge* cart);
uint8_t* getBank(BankCache* cache, uint8_t bank_id, RomType rom);
uint8_t getBankIndex(BankCache* cache, uint8_t* bank_ptr);

#endif // MAPPER_H

