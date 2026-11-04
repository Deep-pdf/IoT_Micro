#include "GbRomBridge.h"
#include "GbRom.h"
#include <string.h>
#include <Arduino.h>

static uint32_t s_decompressCount = 0;
static uint32_t s_cacheHitCount   = 0;

extern "C" {

bool gb_rom_bridge_init(void) {
    s_decompressCount = 0;
    s_cacheHitCount = 0;
    return GbRom::init();
}

bool gb_rom_bridge_is_initialized(void) {
    return (GbRom::getFixedBankBuffer() != nullptr) && (GbRom::getSwitchableBankBuffer() != nullptr);
}

const uint8_t* gb_rom_bridge_get_bank0(void) {
    return GbRom::getFixedBankBuffer();
}

uint8_t gb_rom_bridge_read_bank0(uint16_t address) {
    return GbRom::readFixedBank(address);
}

uint8_t gb_rom_bridge_read_switchable(uint16_t address) {
    return GbRom::readSwitchableBank(address);
}

bool gb_rom_bridge_select_bank(uint8_t bank) {
    if (bank == 0) {
        bank = 1;
    }
    bank &= 0x3F;
    if (bank == 0) {
        bank = 1;
    }

    if (bank == GbRom::getCurrentBank()) {
        s_cacheHitCount++;
        return true;
    }

    bool success = GbRom::selectRomBank(bank);
    if (success) {
        s_decompressCount++;
    }
    return success;
}

uint8_t gb_rom_bridge_get_current_bank(void) {
    return GbRom::getCurrentBank();
}

bool gb_rom_bridge_get_metadata(gb_rom_metadata_t *out_metadata) {
    if (!out_metadata) return false;

    const uint8_t *bank0 = GbRom::getFixedBankBuffer();
    if (!bank0) return false;

    // Title: 0x0134 - 0x0143
    memcpy(out_metadata->title, &bank0[0x0134], 16);
    out_metadata->title[16] = '\0';

    out_metadata->cgb_flag        = (bank0[0x0143] == 0x80 || bank0[0x0143] == 0xC0);
    out_metadata->cartridge_type  = bank0[0x0147];
    out_metadata->rom_size_code   = bank0[0x0148];
    out_metadata->ram_size_code   = bank0[0x0149];
    out_metadata->header_checksum = bank0[0x014D];
    out_metadata->global_checksum = (uint16_t)(bank0[0x014E] << 8) | bank0[0x014F];

    return true;
}

bool gb_rom_bridge_validate_cartridge(void) {
    const uint8_t *bank0 = GbRom::getFixedBankBuffer();
    if (!bank0) return false;

    // 1. Calculate Nintendo Game Boy header checksum: x = 0; for a=0x0134..0x014C: x = x - [a] - 1
    uint8_t chk = 0;
    for (uint16_t a = 0x0134; a <= 0x014C; a++) {
        chk = chk - bank0[a] - 1;
    }

    if (chk != bank0[0x014D]) {
        Serial.printf("[GbRomBridge] Header checksum error! Calc: 0x%02X, Rom: 0x%02X\n",
                      chk, bank0[0x014D]);
        return false;
    }

    // 2. Validate Cartridge Type: 0x13 = MBC3 + RAM + BATTERY
    if (bank0[0x0147] != 0x13) {
        Serial.printf("[GbRomBridge] Cartridge type mismatch! Exp: 0x13 (MBC3), Got: 0x%02X\n",
                      bank0[0x0147]);
        return false;
    }

    // 3. Validate ROM Size Code: 0x05 = 1 MB (64 banks)
    if (bank0[0x0148] != 0x05) {
        Serial.printf("[GbRomBridge] ROM size code mismatch! Exp: 0x05 (1MB), Got: 0x%02X\n",
                      bank0[0x0148]);
        return false;
    }

    // 4. Validate RAM Size Code: 0x03 = 32 KB (4 banks)
    if (bank0[0x0149] != 0x03) {
        Serial.printf("[GbRomBridge] RAM size code mismatch! Exp: 0x03 (32KB), Got: 0x%02X\n",
                      bank0[0x0149]);
        return false;
    }

    return true;
}

uint32_t gb_rom_bridge_get_decompression_count(void) {
    return s_decompressCount;
}

uint32_t gb_rom_bridge_get_cache_hit_count(void) {
    return s_cacheHitCount;
}

void gb_rom_bridge_reset_metrics(void) {
    s_decompressCount = 0;
    s_cacheHitCount = 0;
}

} // extern "C"

