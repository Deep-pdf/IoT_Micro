#include "cartridge.h"
#include "bus.h"
#include "ppu2C02.h"
#include <cstring>

struct CartridgeHeader
{
    char name[4];
    uint8_t PRG_ROM_chunks;
    uint8_t CHR_ROM_chunks;
    uint8_t mapper1;
    uint8_t mapper2;
    uint8_t PRG_RAM_size;
    uint8_t tv_system;
    uint8_t tv_system2;
    char unused[5];
};

Cartridge::Cartridge(const uint8_t* romData, size_t romSize)
{
    if (!romData || romSize < sizeof(CartridgeHeader))
    {
        is_valid = false;
        return;
    }

    raw_rom = romData;
    raw_size = romSize;

    const CartridgeHeader* header = (const CartridgeHeader*)romData;

    // Check iNES magic: "NES\x1a"
    if (header->name[0] != 'N' || header->name[1] != 'E' ||
        header->name[2] != 'S' || header->name[3] != 0x1A)
    {
        is_valid = false;
        return;
    }

    mapper_ID = (header->mapper2 & 0xF0) | (header->mapper1 >> 4);
    hardware_mirror = (header->mapper1 & 0x01) ? MIRROR::VERTICAL : MIRROR::HORIZONTAL;

    uint8_t number_PRG_banks = header->PRG_ROM_chunks;
    uint8_t number_CHR_banks = header->CHR_ROM_chunks;

    prg_base = sizeof(CartridgeHeader) + ((header->mapper1 & 0x04) ? 512 : 0);
    chr_base = prg_base + (number_PRG_banks * 16384);

    mROM.prg_base = romData + prg_base;
    mROM.prg_size = number_PRG_banks * 16384;
    mROM.chr_base = (number_CHR_banks > 0) ? (romData + chr_base) : nullptr;
    mROM.chr_size = number_CHR_banks * 8192;

    // Compute CRC32
    if (mROM.prg_size > 0 && (prg_base + mROM.prg_size <= romSize))
    {
        CRC32 = crc32(mROM.prg_base, mROM.prg_size);
        CRC32 ^= ~0U;
    }

    createMapper(number_PRG_banks, number_CHR_banks, ROMBackend::FLASH);
    is_valid = true;
}

Cartridge::~Cartridge()
{
    if (mapper.state)
    {
        if (mapper_ID == 4)
        {
            Mapper004_state* s = (Mapper004_state*)mapper.state;
            if (s->RAM) free(s->RAM);
            delete s;
        }
        else if (mapper_ID == 2)
        {
            Mapper002_state* s = (Mapper002_state*)mapper.state;
            if (s->CHR_bank && s->number_CHR_banks == 0) free(s->CHR_bank);
            delete s;
        }
        else if (mapper_ID == 0)
        {
            Mapper000_state* s = (Mapper000_state*)mapper.state;
            delete s;
        }
        mapper.state = nullptr;
    }
}

void Cartridge::createMapper(uint8_t number_PRG_banks, uint8_t number_CHR_banks, ROMBackend backend)
{
    switch (mapper_ID)
    {
    case 0:
        mapper = createMapper000(number_PRG_banks, number_CHR_banks, backend, this);
        break;
    case 2:
        mapper = createMapper002(number_PRG_banks, number_CHR_banks, backend, this);
        break;
    case 4:
        mapper = createMapper004(number_PRG_banks, number_CHR_banks, backend, this);
        break;
    default:
        is_valid = false;
        break;
    }
}

void Cartridge::reset()
{
    switch (mapper_ID)
    {
    case 0: mapper000_reset(&mapper); break;
    case 2: mapper002_reset(&mapper); break;
    case 4: mapper004_reset(&mapper); break;
    default: break;
    }
}

void Cartridge::mapPages(Bus* bus)
{
    switch (mapper_ID)
    {
    case 0: mapper000_mapPages(&mapper, bus); break;
    case 2: mapper002_mapPages(&mapper, bus); break;
    case 4: mapper004_mapPages(&mapper, bus); break;
    default: break;
    }
}

void Cartridge::mapPPUPages(Ppu2C02* ppu)
{
    switch (mapper_ID)
    {
    case 0: mapper000_mapPPUPages(&mapper, ppu); break;
    case 2: mapper002_mapPPUPages(&mapper, ppu); break;
    case 4: mapper004_mapPPUPages(&mapper, ppu); break;
    default: break;
    }
}

void Cartridge::ppuScanline()
{
    switch (mapper_ID)
    {
    case 4: mapper004_scanline(&mapper); break;
    default: break;
    }
}

void Cartridge::cpuCycle(int cycles)
{
}

void Cartridge::loadPRGBank(uint8_t* bank, uint16_t size, uint32_t offset)
{
    if (mROM.prg_base && bank)
    {
        memcpy(bank, mROM.prg_base + offset, size);
    }
}

void Cartridge::loadCHRBank(uint8_t* bank, uint16_t size, uint32_t offset)
{
    if (mROM.chr_base && bank)
    {
        memcpy(bank, mROM.chr_base + offset, size);
    }
}

void Cartridge::setMirrorMode(MIRROR m)
{
    mirror = m;
    bus->setPPUMirrorMode(m);
}

MIRROR Cartridge::getMirrorMode()
{
    return mirror;
}

void Cartridge::IRQ()
{
    bus->IRQ();
}

bool Cartridge::isValid()
{
    return is_valid;
}

uint32_t Cartridge::crc32(const void* buf, size_t size, uint32_t seed)
{
    const uint8_t* p = (const uint8_t*)buf;
    uint32_t crc = seed;
    while (size--)
    {
        crc ^= *p++;
        for (int i = 0; i < 8; i++)
        {
            crc = (crc >> 1) ^ (-(int)(crc & 1) & 0xEDB88320);
        }
    }
    return crc;
}

