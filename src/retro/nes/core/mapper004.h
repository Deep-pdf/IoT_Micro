#ifndef MAPPER004_H
#define MAPPER004_H

#include "mapper.h"
#include "mirror_mode.h"

#define MAPPER004_NUM_PRG_BANKS_8K 17
#define MAPPER004_NUM_CHR_BANKS_1K 16

struct Mapper004_state
{
    Cartridge* cart = nullptr;
    MappedROM* mROM = nullptr;
    ROMBackend backend;
    uint8_t number_PRG_banks;
    uint8_t number_CHR_banks;
    uint8_t* RAM = nullptr;

    uint8_t bank_register[8];
    uint8_t* ptr_PRG_bank_8K[4];
    uint8_t* ptr_CHR_bank_1K[8];

    Bank PRG_banks_8K[MAPPER004_NUM_PRG_BANKS_8K];
    Bank CHR_banks_1K[MAPPER004_NUM_CHR_BANKS_1K];
    BankCache PRG_cache_8K;
    BankCache CHR_cache_1K;

    uint8_t bank_select = 0x00;
    uint8_t IRQ_latch = 0x00;
    uint8_t IRQ_counter = 0x00;
    bool IRQ_enable = false;

    uint8_t PRG_ROM_bank_mode = 0;
    uint8_t CHR_ROM_bank_mode = 0;
    uint16_t PRG_mask = 0;
    uint16_t CHR_mask = 0;
};

Mapper createMapper004(uint8_t PRG_banks, uint8_t CHR_banks, ROMBackend backend, Cartridge* cart);

void mapper004_mapPages(Mapper* mapper, Bus* bus);
void mapper004_mapPPUPages(Mapper* mapper, Ppu2C02* ppu);
void mapper004_scanline(Mapper* mapper);
void mapper004_reset(Mapper* mapper);

#endif // MAPPER004_H

