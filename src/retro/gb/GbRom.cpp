#include "GbRom.h"
#include "GbLz4.h"
#include <Arduino.h>
#include <pgmspace.h>
#include <string.h>

// Static storage allocation
uint8_t* GbRom::s_switchableBankRam = nullptr;
uint8_t  GbRom::s_currentLoadedBank = 0xFF;
bool     GbRom::s_initialized = false;
uint32_t GbRom::s_decompressCount = 0;
uint32_t GbRom::s_cacheHitCount = 0;

// Standard CRC32 tableless / byte-wise calculator for validation
uint32_t GbRom::calculateCrc32(const uint8_t* data, size_t length) {
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            crc = (crc >> 1) ^ (0xEDB88320UL & -(crc & 1));
        }
    }
    return ~crc;
}

bool GbRom::init() {
    // 1. Verify flash symbols and integrity
    if (gb_rom_bank0_size != GB_ROM_BANK0_SIZE) {
        Serial.printf("[GbRom] ERROR: Bank 0 size mismatch! Exp: %d, Got: %u\n",
                      GB_ROM_BANK0_SIZE, gb_rom_bank0_size);
        return false;
    }

    if (gb_rom_compressed_size != GB_ROM_COMPRESSED_DATA_SIZE) {
        Serial.printf("[GbRom] ERROR: Compressed payload mismatch! Exp: %d, Got: %u\n",
                      GB_ROM_COMPRESSED_DATA_SIZE, gb_rom_compressed_size);
        return false;
    }

    // 2. Allocate the single reusable 16 KB switchable bank buffer (once on init)
    if (!s_switchableBankRam) {
        s_switchableBankRam = (uint8_t*)malloc(GB_ROM_BANK_SIZE);
        if (!s_switchableBankRam) {
            Serial.println("[GbRom] ERROR: Failed to allocate 16 KB switchable bank buffer!");
            return false;
        }
    }

    s_currentLoadedBank = 0xFF; // Invalidate cache

    // 3. Pre-load Bank 1 (default startup state for Game Boy DMG)
    if (!selectRomBank(1)) {
        Serial.println("[GbRom] ERROR: Failed to load initial Bank 1!");
        deinit();
        return false;
    }

    s_initialized = true;
    Serial.printf("[GbRom] Initialized: Bank 0 (%d B raw flash) + Banks 1..63 (%u B LZ4 HC-12) | 16 KB SRAM buffer ready\n",
                  GB_ROM_BANK0_SIZE, gb_rom_compressed_size);
    return true;
}

void GbRom::deinit() {
    if (s_switchableBankRam) {
        free(s_switchableBankRam);
        s_switchableBankRam = nullptr;
    }
    s_currentLoadedBank = 0xFF;
    s_initialized       = false;
    s_decompressCount   = 0;
    s_cacheHitCount     = 0;
}

bool GbRom::selectRomBank(uint8_t bank) {
    if (!s_switchableBankRam) {
        return false;
    }

    // MBC3 Cartridge Specification:
    // Writing 0 to the ROM bank register remaps to Bank 1
    if (bank == 0) {
        bank = 1;
    }

    // Mask to 64 banks (6-bit mask: 0x3F)
    bank &= 0x3F;
    if (bank == 0) {
        bank = 1;
    }

    // Cache hit: already decompressed in RAM
    if (bank == s_currentLoadedBank) {
        s_cacheHitCount++;
        return true;
    }

    if (bank >= GB_ROM_TOTAL_BANKS) {
        return false;
    }

    // Read metadata for this bank from PROGMEM table
    uint32_t flashOffset = pgm_read_dword(&GB_ROM_BANKS[bank].flashOffset);
    uint16_t compSize    = pgm_read_word(&GB_ROM_BANKS[bank].compSize);
    uint16_t uncompSize  = pgm_read_word(&GB_ROM_BANKS[bank].uncompSize);

    // Bounds check within flash payload
    if (flashOffset + compSize > gb_rom_compressed_size) {
        Serial.printf("[GbRom] ERROR: Bank %d offset out of bounds! Offset: %u, Size: %u, Max: %u\n",
                      bank, flashOffset, compSize, gb_rom_compressed_size);
        return false;
    }

    // Decompress safely directly into switchable bank buffer
    const uint8_t* compSrc = &gb_rom_compressed_data[flashOffset];
    int decompBytes = gb_lz4_decompress_safe(compSrc, s_switchableBankRam, compSize, GB_ROM_BANK_SIZE);

    if (decompBytes != (int)uncompSize) {
        Serial.printf("[GbRom] ERROR: Decompression error for Bank %d! Exp: %d, Got: %d\n",
                      bank, uncompSize, decompBytes);
        s_currentLoadedBank = 0xFF; // Invalidate active bank
        return false;
    }

    // Decompression verified -> update active bank & counter
    s_currentLoadedBank = bank;
    s_decompressCount++;
    return true;
}

uint8_t GbRom::getCurrentBank() {
    return s_currentLoadedBank;
}

uint32_t GbRom::getDecompressionCount() {
    return s_decompressCount;
}

uint32_t GbRom::getCacheHitCount() {
    return s_cacheHitCount;
}

void GbRom::resetMetrics() {
    s_decompressCount = 0;
    s_cacheHitCount   = 0;
}

const uint8_t* GbRom::getFixedBankBuffer() {
    return gb_rom_bank0_data;
}

const uint8_t* GbRom::getSwitchableBankBuffer() {
    return s_switchableBankRam;
}

size_t GbRom::getTotalFlashSize() {
    return gb_rom_bank0_size + gb_rom_compressed_size + sizeof(GB_ROM_BANKS);
}

size_t GbRom::getRamFootprint() {
    return GB_ROM_BANK_SIZE + sizeof(s_switchableBankRam) + sizeof(s_currentLoadedBank) + sizeof(s_initialized);
}

uint32_t GbRom::benchmarkBankDecompress(uint8_t bank) {
    if (!s_switchableBankRam || bank == 0 || bank >= GB_ROM_TOTAL_BANKS) return 0;

    uint32_t flashOffset = pgm_read_dword(&GB_ROM_BANKS[bank].flashOffset);
    uint16_t compSize    = pgm_read_word(&GB_ROM_BANKS[bank].compSize);

    const uint8_t* compSrc = &gb_rom_compressed_data[flashOffset];

    uint32_t t0 = micros();
    gb_lz4_decompress_safe(compSrc, s_switchableBankRam, compSize, GB_ROM_BANK_SIZE);
    uint32_t t1 = micros();

    return (t1 - t0);
}

bool GbRom::validate() {
    if (!init()) {
        return false;
    }

    Serial.println("==================================================");
    Serial.println("[GbRom] Running Full 64-Bank Integrity Validation");
    Serial.println("==================================================");

    // 1. Verify Bank 0 in raw flash
    uint32_t b0CrcExpected = pgm_read_dword(&GB_ROM_BANKS[0].crc32);
    uint32_t b0CrcActual   = calculateCrc32(gb_rom_bank0_data, GB_ROM_BANK0_SIZE);

    if (b0CrcActual != b0CrcExpected) {
        Serial.printf("[GbRom] FAIL: Bank 0 CRC mismatch! Exp: 0x%08X, Got: 0x%08X\n",
                      b0CrcExpected, b0CrcActual);
        return false;
    }
    Serial.printf("[GbRom] Bank 00 (Flash Raw):  CRC 0x%08X -> PASS\n", b0CrcActual);

    // 2. Verify ROM Header inside Bank 0
    char title[17] = {0};
    memcpy(title, &gb_rom_bank0_data[0x0134], 16);
    uint8_t cartType = gb_rom_bank0_data[0x0147];
    uint8_t romCode  = gb_rom_bank0_data[0x0148];
    uint8_t ramCode  = gb_rom_bank0_data[0x0149];

    Serial.printf("[GbRom] Header: Title='%s', CartType=0x%02X, RomCode=0x%02X, RamCode=0x%02X\n",
                  title, cartType, romCode, ramCode);

    if (strncmp(title, "POKEMON RED", 11) != 0 || cartType != 0x13 || romCode != 0x05 || ramCode != 0x03) {
        Serial.println("[GbRom] FAIL: Header metadata validation mismatch!");
        return false;
    }
    Serial.println("[GbRom] Header Verification -> PASS");

    // 3. Decompress and verify Banks 1..63 against precomputed CRC32
    uint32_t maxDecompUs = 0;
    uint8_t worstBank = 1;
    uint32_t totalDecompUs = 0;

    for (uint8_t b = 1; b < GB_ROM_TOTAL_BANKS; b++) {
        uint32_t expectedCrc = pgm_read_dword(&GB_ROM_BANKS[b].crc32);
        uint16_t compSize    = pgm_read_word(&GB_ROM_BANKS[b].compSize);
        uint32_t flashOffset = pgm_read_dword(&GB_ROM_BANKS[b].flashOffset);

        uint32_t t0 = micros();
        int decompBytes = gb_lz4_decompress_safe(&gb_rom_compressed_data[flashOffset],
                                                s_switchableBankRam,
                                                compSize,
                                                GB_ROM_BANK_SIZE);
        uint32_t t1 = micros();
        uint32_t decompUs = t1 - t0;
        totalDecompUs += decompUs;

        if (decompUs > maxDecompUs) {
            maxDecompUs = decompUs;
            worstBank = b;
        }

        if (decompBytes != GB_ROM_BANK_SIZE) {
            Serial.printf("[GbRom] FAIL: Bank %02d decompressed length %d != 16384!\n", b, decompBytes);
            return false;
        }

        uint32_t actualCrc = calculateCrc32(s_switchableBankRam, GB_ROM_BANK_SIZE);
        if (actualCrc != expectedCrc) {
            Serial.printf("[GbRom] FAIL: Bank %02d CRC mismatch! Exp: 0x%08X, Got: 0x%08X\n",
                          b, expectedCrc, actualCrc);
            return false;
        }

        if (b <= 5 || b >= 59 || b == 19 || b == 45) {
            Serial.printf("[GbRom] Bank %02d: Comp=%5d B, Decomp=%5u us, CRC 0x%08X -> PASS\n",
                          b, compSize, decompUs, actualCrc);
        }
    }

    // 4. Test bank cache / repeated access
    // Step 1: Select bank 10 through the normal bank-selection path
    if (!selectRomBank(10)) {
        Serial.println("[GbRom] FAIL: Initial selectRomBank(10) failed!");
        return false;
    }
    // Step 2: Verify that the loaded bank is actually bank 10
    if (getCurrentBank() != 10) {
        Serial.printf("[GbRom] FAIL: Loaded bank %d != 10!\n", getCurrentBank());
        return false;
    }
    // Step 3: Verify its contents against the expected CRC32
    uint32_t expectedCrc10 = pgm_read_dword(&GB_ROM_BANKS[10].crc32);
    uint32_t actualCrc10   = calculateCrc32(s_switchableBankRam, GB_ROM_BANK_SIZE);
    if (actualCrc10 != expectedCrc10) {
        Serial.printf("[GbRom] FAIL: Bank 10 CRC mismatch! Exp: 0x%08X, Got: 0x%08X\n",
                      expectedCrc10, actualCrc10);
        return false;
    }
    // Step 4: Record the decompression counter
    uint32_t decompCountBefore = s_decompressCount;

    // Step 5: Select bank 10 again through the normal API
    if (!selectRomBank(10)) {
        Serial.println("[GbRom] FAIL: Repeated selectRomBank(10) failed!");
        return false;
    }
    // Step 6 & 7: Verify the selection succeeds and current bank remains 10
    if (getCurrentBank() != 10) {
        Serial.printf("[GbRom] FAIL: Current bank changed to %d on cache hit!\n", getCurrentBank());
        return false;
    }
    // Step 8: Verify the decompression counter did not increase
    if (s_decompressCount != decompCountBefore) {
        Serial.printf("[GbRom] FAIL: Decompression counter increased on cache hit! Before: %u, After: %u\n",
                      decompCountBefore, s_decompressCount);
        return false;
    }
    // Step 9: Verify the bank contents still match the expected CRC32
    uint32_t cachedCrc10 = calculateCrc32(s_switchableBankRam, GB_ROM_BANK_SIZE);
    if (cachedCrc10 != expectedCrc10) {
        Serial.printf("[GbRom] FAIL: Cached bank 10 CRC corrupted! Exp: 0x%08X, Got: 0x%08X\n",
                      expectedCrc10, cachedCrc10);
        return false;
    }
    Serial.println("[GbRom] Bank Cache Hit Test -> PASS (Zero redundant decompressions, CRC32 verified)");

    // 5. Test invalid bank requests
    // Bank 0 should map to Bank 1 (MBC3 rule)
    if (!selectRomBank(0) || getCurrentBank() != 1) {
        Serial.println("[GbRom] FAIL: Bank 0 -> Bank 1 remap failed!");
        return false;
    }
    Serial.println("[GbRom] MBC3 Bank 0 -> Bank 1 Remap Test -> PASS");

    // Bank 64 (0x40 & 0x3F == 0 -> maps to Bank 1)
    if (!selectRomBank(64) || getCurrentBank() != 1) {
        Serial.println("[GbRom] FAIL: Bank 64 mask test failed!");
        return false;
    }
    Serial.println("[GbRom] MBC3 Bank Masking Test -> PASS");

    Serial.println("==================================================");
    Serial.printf("[GbRom] ALL 64 BANKS VALIDATED SUCCESSFULLY!\n");
    Serial.printf("[GbRom] Average Decompress Time: %.1f us\n", (float)totalDecompUs / 63.0f);
    Serial.printf("[GbRom] Worst-Case Decompress:   %u us (Bank %02d)\n", maxDecompUs, worstBank);
    Serial.println("==================================================");

    // Restore Bank 1
    selectRomBank(1);
    return true;
}
