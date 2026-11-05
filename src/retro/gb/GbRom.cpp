#include "GbRom.h"
#include "GbLz4.h"
#include <Arduino.h>
#include <pgmspace.h>
#include <string.h>
#include "esp_heap_caps.h"

// Static storage allocation
GbRomCacheSlot GbRom::s_slots[GB_ROM_CACHE_SLOTS] = {};
uint8_t* GbRom::s_contiguousBuffer = nullptr;
bool     GbRom::s_isContiguous = false;
uint8_t* GbRom::s_activeSwitchableBank = nullptr;
uint8_t  GbRom::s_currentLoadedBank = 0xFF;
bool     GbRom::s_initialized = false;
uint32_t GbRom::s_accessCounter = 0;

uint32_t GbRom::s_totalRequests = 0;
uint32_t GbRom::s_cacheHitCount = 0;
uint32_t GbRom::s_decompressCount = 0;
uint32_t GbRom::s_evictionCount = 0;
uint32_t GbRom::s_totalDecompUs = 0;
uint32_t GbRom::s_maxDecompUs = 0;
uint16_t GbRom::s_bankDecompCounts[GB_ROM_TOTAL_BANKS] = {0};

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
    if (s_initialized) {
        return true;
    }

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

    // 2. Report memory safety diagnostics before allocation
    uint32_t free8BitBefore     = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    uint32_t largest8BitBefore  = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    uint32_t freeInternalBefore = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    Serial.println("[GbRom] Pre-Allocation Heap Metrics:");
    Serial.printf("        Free 8-bit DRAM:   %6u bytes\n", free8BitBefore);
    Serial.printf("        Largest 8-bit:     %6u bytes\n", largest8BitBefore);
    Serial.printf("        Free Internal:     %6u bytes\n", freeInternalBefore);

    // 3. Allocate 4 cache slots (Total payload: 65,536 bytes)
    // Strategy: Attempt single contiguous 64 KB block first
    s_isContiguous = false;
    s_contiguousBuffer = (uint8_t*)heap_caps_malloc(GB_ROM_CACHE_SLOTS * GB_ROM_BANK_SIZE, MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);

    if (s_contiguousBuffer) {
        s_isContiguous = true;
        for (int i = 0; i < GB_ROM_CACHE_SLOTS; i++) {
            s_slots[i].buffer     = s_contiguousBuffer + (i * GB_ROM_BANK_SIZE);
            s_slots[i].bankNum    = 0xFF;
            s_slots[i].valid      = false;
            s_slots[i].lastAccess = 0;
        }
        Serial.printf("[GbRom] Cache Allocation: Contiguous 64 KB block at %p\n", s_contiguousBuffer);
    } else {
        Serial.println("[GbRom] Contiguous 64 KB block unavailable; allocating four independent 16 KB slots.");
        s_isContiguous = false;
        bool allocOk = true;
        for (int i = 0; i < GB_ROM_CACHE_SLOTS; i++) {
            s_slots[i].buffer = (uint8_t*)heap_caps_malloc(GB_ROM_BANK_SIZE, MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
            s_slots[i].bankNum    = 0xFF;
            s_slots[i].valid      = false;
            s_slots[i].lastAccess = 0;
            if (!s_slots[i].buffer) {
                Serial.printf("[GbRom] ERROR: Failed to allocate 16 KB cache slot %d!\n", i);
                allocOk = false;
                break;
            }
        }
        if (!allocOk) {
            // Unwind partial allocations cleanly
            for (int i = 0; i < GB_ROM_CACHE_SLOTS; i++) {
                if (s_slots[i].buffer) {
                    free(s_slots[i].buffer);
                    s_slots[i].buffer = nullptr;
                }
            }
            Serial.println("[GbRom] ERROR: Partial allocation cleaned up. Cache init failed.");
            return false;
        }
    }

    // 4. Validate all 4 slot addresses: distinct, non-overlapping, and 4-byte aligned
    for (int i = 0; i < GB_ROM_CACHE_SLOTS; i++) {
        bool isAligned = (((uintptr_t)s_slots[i].buffer & 0x03) == 0);
        Serial.printf("[GbRom] Cache Slot %d: Buffer %p | Aligned: %s\n",
                      i, s_slots[i].buffer, isAligned ? "YES" : "NO");
        if (!isAligned) {
            Serial.printf("[GbRom] ERROR: Cache slot %d buffer is not 4-byte aligned!\n", i);
            deinit();
            return false;
        }
        for (int j = i + 1; j < GB_ROM_CACHE_SLOTS; j++) {
            if (s_slots[i].buffer == s_slots[j].buffer) {
                Serial.printf("[GbRom] ERROR: Duplicate buffer pointer in slots %d and %d!\n", i, j);
                deinit();
                return false;
            }
        }
    }

    s_activeSwitchableBank = nullptr;
    s_currentLoadedBank    = 0xFF;
    s_accessCounter        = 0;
    resetMetrics();

    // 5. Pre-load Bank 1 into Slot 0 (default startup state for Game Boy DMG)
    if (!selectRomBank(1)) {
        Serial.println("[GbRom] ERROR: Failed to load initial Bank 1!");
        deinit();
        return false;
    }

    s_initialized = true;
    Serial.printf("[GbRom] Initialized 4-Slot LRU Cache (65536 B) | Bank 0 (%u B Flash) + Banks 1..63 (%u B LZ4 HC-12)\n",
                  (unsigned)GB_ROM_BANK0_SIZE, (unsigned)gb_rom_compressed_size);
    return true;
}

void GbRom::deinit() {
    if (s_isContiguous) {
        if (s_contiguousBuffer) {
            free(s_contiguousBuffer);
            s_contiguousBuffer = nullptr;
        }
        for (int i = 0; i < GB_ROM_CACHE_SLOTS; i++) {
            s_slots[i].buffer = nullptr;
        }
    } else {
        for (int i = 0; i < GB_ROM_CACHE_SLOTS; i++) {
            if (s_slots[i].buffer) {
                free(s_slots[i].buffer);
                s_slots[i].buffer = nullptr;
            }
        }
    }
    s_isContiguous = false;
    for (int i = 0; i < GB_ROM_CACHE_SLOTS; i++) {
        s_slots[i].bankNum    = 0xFF;
        s_slots[i].valid      = false;
        s_slots[i].lastAccess = 0;
    }
    s_activeSwitchableBank = nullptr;
    s_currentLoadedBank    = 0xFF;
    s_initialized          = false;
    s_accessCounter        = 0;
    resetMetrics();
    Serial.println("[GbRom] Deinitialized: 4 cache slots released & state reset.");
}

bool GbRom::selectRomBank(uint8_t bank) {
    if (!s_initialized && !s_slots[0].buffer) {
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

    if (bank >= GB_ROM_TOTAL_BANKS) {
        return false;
    }

    s_totalRequests++;

    // Fast path: already active switchable bank
    if (bank == s_currentLoadedBank && s_activeSwitchableBank != nullptr) {
        s_cacheHitCount++;
        // Refresh LRU timestamp for active slot
        for (int i = 0; i < GB_ROM_CACHE_SLOTS; i++) {
            if (s_slots[i].valid && s_slots[i].bankNum == bank) {
                s_slots[i].lastAccess = ++s_accessCounter;
                break;
            }
        }
        return true;
    }

    // Cache lookup: inspect all 4 slots
    for (int i = 0; i < GB_ROM_CACHE_SLOTS; i++) {
        if (s_slots[i].valid && s_slots[i].bankNum == bank) {
            // CACHE HIT!
            s_slots[i].lastAccess = ++s_accessCounter;
            s_activeSwitchableBank = s_slots[i].buffer;
            s_currentLoadedBank = bank;
            s_cacheHitCount++;
            return true;
        }
    }

    // CACHE MISS: Select slot for eviction using LRU policy
    int targetSlot = -1;
    uint32_t oldestAccess = 0xFFFFFFFF;

    // Prefer an unused/invalid slot first
    for (int i = 0; i < GB_ROM_CACHE_SLOTS; i++) {
        if (!s_slots[i].valid) {
            targetSlot = i;
            break;
        }
        if (s_slots[i].lastAccess < oldestAccess) {
            oldestAccess = s_slots[i].lastAccess;
            targetSlot = i;
        }
    }

    if (targetSlot < 0) {
        targetSlot = 0; // Fallback safety
    }

    // If evicting an existing valid bank, record eviction
    if (s_slots[targetSlot].valid) {
        s_evictionCount++;
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

    // Decompress safely directly into selected slot buffer
    const uint8_t* compSrc = &gb_rom_compressed_data[flashOffset];
    uint32_t t0 = micros();
    int decompBytes = gb_lz4_decompress_safe(compSrc, s_slots[targetSlot].buffer, compSize, GB_ROM_BANK_SIZE);
    uint32_t decompUs = micros() - t0;

    if (decompBytes != (int)uncompSize) {
        Serial.printf("[GbRom] ERROR: Decompression error for Bank %d! Exp: %d, Got: %d\n",
                      bank, uncompSize, decompBytes);
        s_slots[targetSlot].valid = false;
        return false;
    }

    // Update telemetry
    s_totalDecompUs += decompUs;
    if (decompUs > s_maxDecompUs) {
        s_maxDecompUs = decompUs;
    }
    s_decompressCount++;
    s_bankDecompCounts[bank]++;

    // Update slot metadata only after successful load
    s_slots[targetSlot].bankNum    = bank;
    s_slots[targetSlot].valid      = true;
    s_slots[targetSlot].lastAccess = ++s_accessCounter;

    // Update active switchable bank pointer and active bank number
    s_activeSwitchableBank = s_slots[targetSlot].buffer;
    s_currentLoadedBank    = bank;

    return true;
}

uint8_t GbRom::getCurrentBank() {
    return s_currentLoadedBank;
}

uint32_t GbRom::getTotalRequests() {
    return s_totalRequests;
}

uint32_t GbRom::getCacheHitCount() {
    return s_cacheHitCount;
}

uint32_t GbRom::getCacheMissCount() {
    return (s_totalRequests >= s_cacheHitCount) ? (s_totalRequests - s_cacheHitCount) : 0;
}

uint32_t GbRom::getDecompressionCount() {
    return s_decompressCount;
}

uint32_t GbRom::getEvictionCount() {
    return s_evictionCount;
}

uint32_t GbRom::getTotalDecompUs() {
    return s_totalDecompUs;
}

uint32_t GbRom::getMaxDecompUs() {
    return s_maxDecompUs;
}

uint16_t GbRom::getBankDecompCount(uint8_t bank) {
    return (bank < GB_ROM_TOTAL_BANKS) ? s_bankDecompCounts[bank] : 0;
}

void GbRom::getSlotInfo(uint8_t slotIdx, uint8_t& outBank, bool& outValid, uint32_t& outLastAccess) {
    if (slotIdx < GB_ROM_CACHE_SLOTS) {
        outBank       = s_slots[slotIdx].bankNum;
        outValid      = s_slots[slotIdx].valid;
        outLastAccess = s_slots[slotIdx].lastAccess;
    } else {
        outBank       = 0xFF;
        outValid      = false;
        outLastAccess = 0;
    }
}

void GbRom::resetMetrics() {
    s_totalRequests   = 0;
    s_cacheHitCount   = 0;
    s_decompressCount = 0;
    s_evictionCount   = 0;
    s_totalDecompUs   = 0;
    s_maxDecompUs     = 0;
    memset(s_bankDecompCounts, 0, sizeof(s_bankDecompCounts));
}

const uint8_t* GbRom::getFixedBankBuffer() {
    return gb_rom_bank0_data;
}

const uint8_t* GbRom::getSwitchableBankBuffer() {
    return s_activeSwitchableBank;
}

size_t GbRom::getTotalFlashSize() {
    return gb_rom_bank0_size + gb_rom_compressed_size + sizeof(GB_ROM_BANKS);
}

size_t GbRom::getRamFootprint() {
    return (GB_ROM_CACHE_SLOTS * GB_ROM_BANK_SIZE) + sizeof(s_slots) + sizeof(s_contiguousBuffer) +
           sizeof(s_isContiguous) + sizeof(s_activeSwitchableBank) + sizeof(s_currentLoadedBank) +
           sizeof(s_initialized) + sizeof(s_bankDecompCounts);
}

uint32_t GbRom::benchmarkBankDecompress(uint8_t bank) {
    if (!s_initialized || bank == 0 || bank >= GB_ROM_TOTAL_BANKS) return 0;

    uint32_t flashOffset = pgm_read_dword(&GB_ROM_BANKS[bank].flashOffset);
    uint16_t compSize    = pgm_read_word(&GB_ROM_BANKS[bank].compSize);

    const uint8_t* compSrc = &gb_rom_compressed_data[flashOffset];

    uint32_t t0 = micros();
    gb_lz4_decompress_safe(compSrc, s_slots[0].buffer, compSize, GB_ROM_BANK_SIZE);
    uint32_t t1 = micros();

    return (t1 - t0);
}

bool GbRom::validate() {
    if (!s_initialized) {
        if (!init()) {
            return false;
        }
    }

    Serial.println("==================================================");
    Serial.println("[GbRom] Running Full 64-Bank & 4-Slot Cache Test");
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
                                                 s_slots[0].buffer,
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

        uint32_t actualCrc = calculateCrc32(s_slots[0].buffer, GB_ROM_BANK_SIZE);
        if (actualCrc != expectedCrc) {
            Serial.printf("[GbRom] FAIL: Bank %02d CRC mismatch! Exp: 0x%08X, Got: 0x%08X\n",
                          b, expectedCrc, actualCrc);
            return false;
        }

        if (b <= 3 || b >= 61 || b == 19 || b == 45) {
            Serial.printf("[GbRom] Bank %02d: Comp=%5d B, Decomp=%5u us, CRC 0x%08X -> PASS\n",
                          b, compSize, decompUs, actualCrc);
        }
    }
    Serial.printf("[GbRom] All 63 Compressed Banks Validated (Avg: %.1f us, Worst: %u us at B%02d)\n",
                  (float)totalDecompUs / 63.0f, maxDecompUs, worstBank);

    // Invalidate cache metadata corrupted by direct scratch use of s_slots[0].buffer in Step 3
    for (int i = 0; i < GB_ROM_CACHE_SLOTS; i++) {
        s_slots[i].bankNum    = 0xFF;
        s_slots[i].valid      = false;
        s_slots[i].lastAccess = 0;
    }
    s_activeSwitchableBank = nullptr;
    s_currentLoadedBank    = 0xFF;
    s_accessCounter        = 0;
    resetMetrics();

    // 4. Test MBC3 Register Remapping & Masking
    // Bank 0 should map to Bank 1
    if (!selectRomBank(0) || getCurrentBank() != 1) {
        Serial.println("[GbRom] FAIL: Bank 0 -> Bank 1 remap failed!");
        return false;
    }
    uint32_t expectedCrc1 = pgm_read_dword(&GB_ROM_BANKS[1].crc32);
    if (calculateCrc32(s_activeSwitchableBank, GB_ROM_BANK_SIZE) != expectedCrc1) {
        Serial.println("[GbRom] FAIL: Bank 0 remapped contents CRC mismatch!");
        return false;
    }
    Serial.println("[GbRom] MBC3 Bank 0 -> Bank 1 Remap -> PASS");

    // Bank 64 (0x40 & 0x3F == 0 -> maps to Bank 1)
    if (!selectRomBank(64) || getCurrentBank() != 1) {
        Serial.println("[GbRom] FAIL: Bank 64 mask test failed!");
        return false;
    }
    Serial.println("[GbRom] MBC3 Bank Masking (Bank 64 -> Bank 1) -> PASS");

    // 5. Test Four-Slot LRU Cache Behavior (Section 6C: Request A, B, C, D, A, E)
    Serial.println("[GbRom] Testing Four-Slot LRU Cache Semantics (A, B, C, D, A, E)...");

    // Reset cache slots and telemetry for clean deterministic test
    for (int i = 0; i < GB_ROM_CACHE_SLOTS; i++) {
        s_slots[i].bankNum    = 0xFF;
        s_slots[i].valid      = false;
        s_slots[i].lastAccess = 0;
    }
    s_activeSwitchableBank = nullptr;
    s_currentLoadedBank    = 0xFF;
    s_accessCounter        = 0;
    resetMetrics();

    // Banks for test: A=10, B=20, C=30, D=40, E=50
    const uint8_t bankA = 10, bankB = 20, bankC = 30, bankD = 40, bankE = 50;

    // Step 5.1: Request A (Bank 10) -> Miss, Slot 0
    if (!selectRomBank(bankA) || getCurrentBank() != bankA || s_decompressCount != 1 || s_cacheHitCount != 0) {
        Serial.printf("[GbRom] FAIL: Step 1 (Bank %d) failed! Decomp: %u, Hits: %u\n", bankA, s_decompressCount, s_cacheHitCount);
        return false;
    }
    if (calculateCrc32(s_activeSwitchableBank, GB_ROM_BANK_SIZE) != pgm_read_dword(&GB_ROM_BANKS[bankA].crc32)) {
        Serial.println("[GbRom] FAIL: Bank A CRC mismatch!");
        return false;
    }

    // Step 5.2: Request B (Bank 20) -> Miss, Slot 1
    if (!selectRomBank(bankB) || getCurrentBank() != bankB || s_decompressCount != 2 || s_cacheHitCount != 0) {
        Serial.printf("[GbRom] FAIL: Step 2 (Bank %d) failed! Decomp: %u, Hits: %u\n", bankB, s_decompressCount, s_cacheHitCount);
        return false;
    }
    if (calculateCrc32(s_activeSwitchableBank, GB_ROM_BANK_SIZE) != pgm_read_dword(&GB_ROM_BANKS[bankB].crc32)) {
        Serial.println("[GbRom] FAIL: Bank B CRC mismatch!");
        return false;
    }

    // Step 5.3: Request C (Bank 30) -> Miss, Slot 2
    if (!selectRomBank(bankC) || getCurrentBank() != bankC || s_decompressCount != 3 || s_cacheHitCount != 0) {
        Serial.printf("[GbRom] FAIL: Step 3 (Bank %d) failed! Decomp: %u, Hits: %u\n", bankC, s_decompressCount, s_cacheHitCount);
        return false;
    }

    // Step 5.4: Request D (Bank 40) -> Miss, Slot 3 (All 4 slots now populated)
    if (!selectRomBank(bankD) || getCurrentBank() != bankD || s_decompressCount != 4 || s_cacheHitCount != 0 || s_evictionCount != 0) {
        Serial.printf("[GbRom] FAIL: Step 4 (Bank %d) failed! Decomp: %u, Evict: %u\n", bankD, s_decompressCount, s_evictionCount);
        return false;
    }
    Serial.println("[GbRom]   Slots Populated: [10, 20, 30, 40] | Evictions: 0 -> PASS");

    // Step 5.5: Request A (Bank 10) again -> CACHE HIT!
    uint32_t decompsBeforeA2 = s_decompressCount;
    if (!selectRomBank(bankA) || getCurrentBank() != bankA) {
        Serial.println("[GbRom] FAIL: Step 5 (Bank A again) selection failed!");
        return false;
    }
    if (s_decompressCount != decompsBeforeA2 || s_cacheHitCount != 1) {
        Serial.printf("[GbRom] FAIL: Cache hit failed! Decomp: %u (exp %u), Hits: %u (exp 1)\n",
                      s_decompressCount, decompsBeforeA2, s_cacheHitCount);
        return false;
    }
    if (calculateCrc32(s_activeSwitchableBank, GB_ROM_BANK_SIZE) != pgm_read_dword(&GB_ROM_BANKS[bankA].crc32)) {
        Serial.println("[GbRom] FAIL: Bank A cached CRC corrupted!");
        return false;
    }
    Serial.println("[GbRom]   Request A (Bank 10) Cache Hit -> PASS (0 decompressions, CRC32 verified)");

    // Step 5.6: Request E (Bank 50) -> CACHE MISS! All slots full -> Evict LRU slot (Bank B / 20)
    // Note: Access order was A(1), B(2), C(3), D(4), A(5). Bank B has access 2 (lowest), so B must be evicted!
    if (!selectRomBank(bankE) || getCurrentBank() != bankE) {
        Serial.println("[GbRom] FAIL: Step 6 (Bank E) selection failed!");
        return false;
    }
    if (s_decompressCount != 5 || s_evictionCount != 1) {
        Serial.printf("[GbRom] FAIL: Eviction accounting failed! Decomp: %u (exp 5), Evict: %u (exp 1)\n",
                      s_decompressCount, s_evictionCount);
        return false;
    }
    if (calculateCrc32(s_activeSwitchableBank, GB_ROM_BANK_SIZE) != pgm_read_dword(&GB_ROM_BANKS[bankE].crc32)) {
        Serial.println("[GbRom] FAIL: Bank E decompressed CRC mismatch!");
        return false;
    }

    // Verify Bank B (20) was evicted and Banks A(10), C(30), D(40), E(50) remain in slots
    bool foundA = false, foundB = false, foundC = false, foundD = false, foundE = false;
    for (int i = 0; i < GB_ROM_CACHE_SLOTS; i++) {
        if (s_slots[i].valid) {
            if (s_slots[i].bankNum == bankA) foundA = true;
            if (s_slots[i].bankNum == bankB) foundB = true;
            if (s_slots[i].bankNum == bankC) foundC = true;
            if (s_slots[i].bankNum == bankD) foundD = true;
            if (s_slots[i].bankNum == bankE) foundE = true;
        }
    }
    if (!foundA || foundB || !foundC || !foundD || !foundE) {
        Serial.printf("[GbRom] FAIL: Incorrect slot eviction state! A:%d B(evicted):%d C:%d D:%d E:%d\n",
                      foundA, foundB, foundC, foundD, foundE);
        return false;
    }
    Serial.println("[GbRom]   LRU Eviction (Evicted Bank 20, Loaded Bank 50) -> PASS");

    // Step 5.7: Request C (Bank 30) -> CACHE HIT!
    if (!selectRomBank(bankC) || s_decompressCount != 5 || s_cacheHitCount != 2) {
        Serial.println("[GbRom] FAIL: Retained Bank C cache hit failed!");
        return false;
    }

    // Step 5.8: Request A (Bank 10) -> CACHE HIT!
    if (!selectRomBank(bankA) || s_decompressCount != 5 || s_cacheHitCount != 3) {
        Serial.println("[GbRom] FAIL: Retained Bank A cache hit failed!");
        return false;
    }
    Serial.println("[GbRom]   Repeated Cache Hits on Un-evicted Slots -> PASS");

    // Step 5.9: Request evicted B (Bank 20) -> Miss, triggers second eviction (Bank D / 40)
    if (!selectRomBank(bankB) || s_decompressCount != 6 || s_evictionCount != 2) {
        Serial.printf("[GbRom] FAIL: Re-requesting evicted bank failed! Decomp: %u (exp 6), Evict: %u (exp 2)\n",
                      s_decompressCount, s_evictionCount);
        return false;
    }
    Serial.println("[GbRom]   Evicted Bank Re-request -> PASS (Decomp=6, Evict=2)");

    Serial.println("==================================================");
    Serial.println("[GbRom] ALL 64 BANKS & 4-SLOT LRU CACHE VALIDATED!");
    Serial.println("==================================================");

    // Restore Bank 1 & reset metrics for game execution
    selectRomBank(1);
    resetMetrics();
    return true;
}
