#include "mapper.h"
#include "cartridge.h"

void bankInit(BankCache* cache, Bank* banks, uint8_t num_banks, uint32_t bank_size, Cartridge* cart)
{
    cache->banks = banks;
    cache->num_banks = num_banks;
    cache->tick = 0;
    cache->cart = cart;

    for (int i = 0; i < num_banks; i++)
    {
        cache->banks[i].bank_id = 0xFF;
        cache->banks[i].last_used = 0;
        cache->banks[i].size = bank_size;

        uint8_t* ptr = (uint8_t*)malloc(bank_size);
        cache->banks[i].bank_ptr = ptr;
    }
}

IRAM_ATTR uint8_t* getBank(BankCache* cache, uint8_t bank_id, RomType rom)
{
    cache->tick++;

    for (int i = 0, n = cache->num_banks; i < n; i++)
    {
        if (cache->banks[i].bank_id == bank_id)
        {
            cache->banks[i].last_used = cache->tick;
            return cache->banks[i].bank_ptr;
        }
    }

    int bank_index = 0;
    uint32_t min_use = cache->banks[0].last_used;
    for (int i = 1, n = cache->num_banks; i < n; i++)
    {
        if (cache->banks[i].last_used < min_use)
        {
            min_use = cache->banks[i].last_used;
            bank_index = i;
        }
    }

    uint8_t* bank = cache->banks[bank_index].bank_ptr;
    uint32_t size = cache->banks[bank_index].size;
    if (rom == RomType::PRG) cache->cart->loadPRGBank(bank, size, bank_id * size);
    else if (rom == RomType::CHR) cache->cart->loadCHRBank(bank, size, bank_id * size);

    cache->banks[bank_index].bank_id = bank_id;
    cache->banks[bank_index].last_used = cache->tick;

    return bank;
}

uint8_t getBankIndex(BankCache* cache, uint8_t* ptr)
{
    for (int i = 0, banks = cache->num_banks; i < banks; i++)
    {
        if (cache->banks[i].bank_ptr == ptr) return cache->banks[i].bank_id;
    }

    return 0;
}

