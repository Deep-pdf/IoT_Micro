#ifndef CARTRIDGE_H
#define CARTRIDGE_H

#include <stdint.h>
#include <stddef.h>

#include "flash_mmap.h"
#include "nes_debug.h"
#include "mapper.h"
#include "mapper000.h"
#include "mapper002.h"
#include "mapper004.h"
#include "mirror_mode.h"
#include "ppu2C02.h"
#include "rom_backends.h"

class Bus;
class Cartridge
{
public:
    Cartridge(const uint8_t* romData, size_t romSize);
    ~Cartridge();

    void ppuScanline();
    void cpuCycle(int cycles);
    void reset();

    void mapPages(Bus* bus);
    void mapPPUPages(Ppu2C02* ppu);

    void loadPRGBank(uint8_t* bank, uint16_t size, uint32_t offset);
    void loadCHRBank(uint8_t* bank, uint16_t size, uint32_t offset);
    void setMirrorMode(MIRROR mirror);
    MIRROR getMirrorMode();
    void connectBus(Bus* n)
    {
        bus = n;
    }
    void IRQ();
    bool isValid();

    MIRROR hardware_mirror;
    MIRROR mirror = MIRROR::HORIZONTAL;
    uint32_t CRC32 = ~0U;
    MappedROM mROM;
    Mapper mapper;

private:
    Bus* bus = nullptr;
    bool is_valid = false;
    uint32_t prg_base = 0;
    uint32_t chr_base = 0;
    const uint8_t* raw_rom = nullptr;
    size_t raw_size = 0;

    uint8_t mapper_ID = 0;
    void createMapper(uint8_t number_PRG_banks, uint8_t number_CHR_banks, ROMBackend backend);
    uint32_t crc32(const void* buf, size_t size, uint32_t seed = ~0U);
};

#endif // CARTRIDGE_H

