#include "mapper004.h"
#include "bus.h"
#include "cartridge.h"
#include "ppu2C02.h"

static constexpr MIRROR mapper004_mirror[2] = { MIRROR::VERTICAL, MIRROR::HORIZONTAL };
static inline uint8_t* getPRGBank(Mapper004_state* state, uint8_t index);
static inline uint8_t* getCHRBank(Mapper004_state* state, uint8_t index);

static void mapper004_remapWindows(Mapper004_state* state, Bus* bus)
{
    uint8_t* windows[4] = { state->ptr_PRG_bank_8K[0], state->ptr_PRG_bank_8K[1],
                            state->ptr_PRG_bank_8K[2], state->ptr_PRG_bank_8K[3] };
    for (int i = 0; i < 4; i++)
    {
        int base = 0x80 + (i * 0x20);
        for (int p = 0; p <= 0x1F; p++) bus->read_pages[base + p] = windows[i] + (p * 256);
    }
}

static void mapper004_remapCHRPages(Mapper004_state* state, Ppu2C02* ppu)
{
    uint8_t* windows[8] = { state->ptr_CHR_bank_1K[0], state->ptr_CHR_bank_1K[1],
                            state->ptr_CHR_bank_1K[2], state->ptr_CHR_bank_1K[3],
                            state->ptr_CHR_bank_1K[4], state->ptr_CHR_bank_1K[5],
                            state->ptr_CHR_bank_1K[6], state->ptr_CHR_bank_1K[7] };
    for (int i = 0; i < 8; i++)
    {
        int base = 0x00 + (i * 0x04);
        for (int p = 0; p <= 0x03; p++) ppu->ppu_read_pages[base + p] = windows[i] + (p * 256);
    }
}

static void mapper004_bankWrite(Bus* bus, uint16_t addr, uint8_t data)
{
    Mapper004_state* state = (Mapper004_state*)bus->cart->mapper.state;
    if (!(addr & 0x01))
    {
        state->bank_select = data & 0x07;
        state->PRG_ROM_bank_mode = (data >> 6) & 0x01;
        state->CHR_ROM_bank_mode = (data >> 7) & 0x01;
        return;
    }

    state->bank_register[state->bank_select] = data;

    uint8_t bank_register[10];
    bank_register[0] = (state->bank_register[0] & 0xFE) & state->CHR_mask;
    bank_register[1] = (state->bank_register[1] & 0xFE) & state->CHR_mask;
    bank_register[2] = state->bank_register[2] & state->CHR_mask;
    bank_register[3] = state->bank_register[3] & state->CHR_mask;
    bank_register[4] = state->bank_register[4] & state->CHR_mask;
    bank_register[5] = state->bank_register[5] & state->CHR_mask;
    bank_register[6] = state->bank_register[6] & state->PRG_mask;
    bank_register[7] = state->bank_register[7] & state->PRG_mask;
    bank_register[8] = ((state->bank_register[0] & 0xFE) + 1) & state->CHR_mask;
    bank_register[9] = ((state->bank_register[1] & 0xFE) + 1) & state->CHR_mask;
    if (state->CHR_ROM_bank_mode)
    {
        state->ptr_CHR_bank_1K[0] = getCHRBank(state, bank_register[2]);
        state->ptr_CHR_bank_1K[1] = getCHRBank(state, bank_register[3]);
        state->ptr_CHR_bank_1K[2] = getCHRBank(state, bank_register[4]);
        state->ptr_CHR_bank_1K[3] = getCHRBank(state, bank_register[5]);
        state->ptr_CHR_bank_1K[4] = getCHRBank(state, bank_register[0]);
        state->ptr_CHR_bank_1K[5] = getCHRBank(state, bank_register[8]);
        state->ptr_CHR_bank_1K[6] = getCHRBank(state, bank_register[1]);
        state->ptr_CHR_bank_1K[7] = getCHRBank(state, bank_register[9]);
    }
    else
    {
        state->ptr_CHR_bank_1K[0] = getCHRBank(state, bank_register[0]);
        state->ptr_CHR_bank_1K[1] = getCHRBank(state, bank_register[8]);
        state->ptr_CHR_bank_1K[2] = getCHRBank(state, bank_register[1]);
        state->ptr_CHR_bank_1K[3] = getCHRBank(state, bank_register[9]);
        state->ptr_CHR_bank_1K[4] = getCHRBank(state, bank_register[2]);
        state->ptr_CHR_bank_1K[5] = getCHRBank(state, bank_register[3]);
        state->ptr_CHR_bank_1K[6] = getCHRBank(state, bank_register[4]);
        state->ptr_CHR_bank_1K[7] = getCHRBank(state, bank_register[5]);
    }

    if (state->PRG_ROM_bank_mode)
    {
        state->ptr_PRG_bank_8K[0] = getPRGBank(state, (state->number_PRG_banks * 2) - 2);
        state->ptr_PRG_bank_8K[2] = getPRGBank(state, bank_register[6]);
    }
    else
    {
        state->ptr_PRG_bank_8K[0] = getPRGBank(state, bank_register[6]);
        state->ptr_PRG_bank_8K[2] = getPRGBank(state, (state->number_PRG_banks * 2) - 2);
    }
    state->ptr_PRG_bank_8K[1] = getPRGBank(state, bank_register[7]);
    state->ptr_PRG_bank_8K[3] = getPRGBank(state, (state->number_PRG_banks * 2) - 1);

    mapper004_remapWindows(state, bus);
    mapper004_remapCHRPages(state, &bus->ppu);
}

static void mapper004_mirrorWrite(Bus* bus, uint16_t addr, uint8_t data)
{
    Mapper004_state* state = (Mapper004_state*)bus->cart->mapper.state;
    if (!(addr & 0x01)) state->cart->setMirrorMode(mapper004_mirror[data & 0x01]);
}

static void mapper004_irqLatchWrite(Bus* bus, uint16_t addr, uint8_t data)
{
    Mapper004_state* state = (Mapper004_state*)bus->cart->mapper.state;
    if (!(addr & 0x01)) state->IRQ_latch = data;
    else state->IRQ_counter = 0;
}

static void mapper004_irqEnableWrite(Bus* bus, uint16_t addr, uint8_t data)
{
    Mapper004_state* state = (Mapper004_state*)bus->cart->mapper.state;
    state->IRQ_enable = (addr & 0x01);
}

void mapper004_scanline(Mapper* mapper)
{
    Mapper004_state* state = (Mapper004_state*)mapper->state;
    if (state->IRQ_counter == 0) state->IRQ_counter = state->IRQ_latch;
    else
    {
        state->IRQ_counter--;
        if ((state->IRQ_counter == 0) && state->IRQ_enable) state->cart->IRQ();
    }
}

void mapper004_reset(Mapper* mapper)
{
    Mapper004_state* state = (Mapper004_state*)mapper->state;
    if (state->RAM) memset(state->RAM, 0, 8U * 1024U);

    switch (state->backend)
    {
    case ROMBackend::LRU:
        state->ptr_PRG_bank_8K[0] = getBank(&state->PRG_cache_8K, 0, RomType::PRG);
        state->ptr_PRG_bank_8K[1] = getBank(&state->PRG_cache_8K, 0, RomType::PRG);
        state->ptr_PRG_bank_8K[2] =
            getBank(&state->PRG_cache_8K, (state->number_PRG_banks * 2) - 2, RomType::PRG);
        state->ptr_PRG_bank_8K[3] =
            getBank(&state->PRG_cache_8K, (state->number_PRG_banks * 2) - 1, RomType::PRG);

        state->ptr_CHR_bank_1K[0] = getBank(&state->CHR_cache_1K, 0, RomType::CHR);
        state->ptr_CHR_bank_1K[1] = getBank(&state->CHR_cache_1K, 0, RomType::CHR);
        state->ptr_CHR_bank_1K[2] = getBank(&state->CHR_cache_1K, 0, RomType::CHR);
        state->ptr_CHR_bank_1K[3] = getBank(&state->CHR_cache_1K, 0, RomType::CHR);
        state->ptr_CHR_bank_1K[4] = getBank(&state->CHR_cache_1K, 0, RomType::CHR);
        state->ptr_CHR_bank_1K[5] = getBank(&state->CHR_cache_1K, 0, RomType::CHR);
        state->ptr_CHR_bank_1K[6] = getBank(&state->CHR_cache_1K, 0, RomType::CHR);
        state->ptr_CHR_bank_1K[7] = getBank(&state->CHR_cache_1K, 0, RomType::CHR);
        break;

    case ROMBackend::FLASH:
        state->ptr_PRG_bank_8K[0] = (uint8_t*)state->mROM->prg_base;
        state->ptr_PRG_bank_8K[1] = (uint8_t*)state->mROM->prg_base;
        state->ptr_PRG_bank_8K[2] =
            (uint8_t*)(state->mROM->prg_base + (state->mROM->prg_size - ((8U * 1024U) * 2)));
        state->ptr_PRG_bank_8K[3] =
            (uint8_t*)(state->mROM->prg_base + (state->mROM->prg_size - ((8U * 1024U) * 1)));

        state->ptr_CHR_bank_1K[0] = (uint8_t*)state->mROM->chr_base;
        state->ptr_CHR_bank_1K[1] = (uint8_t*)state->mROM->chr_base;
        state->ptr_CHR_bank_1K[2] = (uint8_t*)state->mROM->chr_base;
        state->ptr_CHR_bank_1K[3] = (uint8_t*)state->mROM->chr_base;
        state->ptr_CHR_bank_1K[4] = (uint8_t*)state->mROM->chr_base;
        state->ptr_CHR_bank_1K[5] = (uint8_t*)state->mROM->chr_base;
        state->ptr_CHR_bank_1K[6] = (uint8_t*)state->mROM->chr_base;
        state->ptr_CHR_bank_1K[7] = (uint8_t*)state->mROM->chr_base;
        break;
    }

    state->bank_select = 0x00;
    state->IRQ_latch = 0x00;
    state->IRQ_counter = 0x00;
    state->IRQ_enable = false;
    state->PRG_ROM_bank_mode = 0;
    state->CHR_ROM_bank_mode = 0;
    state->PRG_mask = (state->number_PRG_banks * 2) - 1;
    state->CHR_mask = (state->number_CHR_banks * 8) - 1;
    state->cart->setMirrorMode(MIRROR::HORIZONTAL);
}

void mapper004_mapPages(Mapper* mapper, Bus* bus)
{
    Mapper004_state* state = (Mapper004_state*)mapper->state;

    // $6000-$7FFF: 8KB PRG-RAM
    if (state->RAM)
    {
        for (int p = 0x60; p <= 0x7F; p++)
        {
            bus->read_pages[p] = state->RAM + ((p - 0x60) * 256);
            bus->write_pages[p] = state->RAM + ((p - 0x60) * 256);
        }
    }

    // Map bank writes
    // $8000-$9FFF: Bank select (even address) | Bank data (odd address)
    for (int p = 0x80; p <= 0x9F; p++) bus->write_handlers[p] = mapper004_bankWrite;

    // $A000-$BFFF: Mirroring (even address)
    for (int p = 0xA0; p <= 0xBF; p++) bus->write_handlers[p] = mapper004_mirrorWrite;

    // $C000-$DFFF: IRQ latch (even address) | IRQ reload (odd address)
    for (int p = 0xC0; p <= 0xDF; p++) bus->write_handlers[p] = mapper004_irqLatchWrite;

    // $E000-$FFFF: IRQ disable (even address) | IRQ enable (odd address)
    for (int p = 0xE0; p <= 0xFF; p++) bus->write_handlers[p] = mapper004_irqEnableWrite;

    // Map bank reads
    mapper004_remapWindows(state, bus);
}

void mapper004_mapPPUPages(Mapper* mapper, Ppu2C02* ppu)
{
    Mapper004_state* state = (Mapper004_state*)mapper->state;
    mapper004_remapCHRPages(state, ppu);
}

Mapper createMapper004(uint8_t PRG_banks, uint8_t CHR_banks, ROMBackend backend, Cartridge* cart)
{
    Mapper mapper;
    Mapper004_state* state = new Mapper004_state;

    state->RAM = (uint8_t*)malloc(8U * 1024U);
    switch (backend)
    {
    case ROMBackend::LRU:
        bankInit(&state->PRG_cache_8K, state->PRG_banks_8K, MAPPER004_NUM_PRG_BANKS_8K, 8U * 1024U,
                 cart);
        bankInit(&state->CHR_cache_1K, state->CHR_banks_1K, MAPPER004_NUM_CHR_BANKS_1K, 1U * 1024,
                 cart);
        break;

    case ROMBackend::FLASH: state->mROM = &cart->mROM; break;
    }

    state->backend = backend;
    state->number_PRG_banks = PRG_banks;
    state->number_CHR_banks = CHR_banks;
    state->cart = cart;
    mapper.state = state;
    return mapper;
}

static inline uint8_t* getPRGBank(Mapper004_state* state, uint8_t index)
{
    if (state->backend == ROMBackend::LRU)
        return getBank(&state->PRG_cache_8K, index, RomType::PRG);
    return (uint8_t*)(state->mROM->prg_base + (uint32_t)index * 8U * 1024U);
}

static inline uint8_t* getCHRBank(Mapper004_state* state, uint8_t index)
{
    if (state->backend == ROMBackend::LRU)
        return getBank(&state->CHR_cache_1K, index, RomType::CHR);
    return (uint8_t*)(state->mROM->chr_base + (uint32_t)index * 1U * 1024U);
}

