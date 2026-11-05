#ifndef GB_ROM_H
#define GB_ROM_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "GbRomIndex.h"

// Assembly symbols embedded in flash via GbRomData.S
extern "C" {
    extern const uint8_t  gb_rom_bank0_data[];
    extern const uint32_t gb_rom_bank0_size;
    extern const uint8_t  gb_rom_compressed_data[];
    extern const uint32_t gb_rom_compressed_size;
}

#define GB_ROM_CACHE_SLOTS 4

// Fixed-size 16 KB cache slot descriptor
struct GbRomCacheSlot {
    uint8_t* buffer;      // 16 KB decompressed bank payload in SRAM
    uint8_t  bankNum;     // ROM bank number currently stored (1..63)
    bool     valid;       // True if this slot holds valid bank data
    uint32_t lastAccess;  // Monotonic sequence counter for LRU ordering
};

class GbRom {
public:
    // Initialize the ROM subsystem with a 4-slot LRU cache (65,536 bytes) and preload Bank 1
    static bool init();

    // Deinitialize the ROM subsystem and release all 4 cache slots
    static void deinit();

    // Direct reads from raw Bank 0 in flash ($0000 - $3FFF)
    static inline uint8_t readFixedBank(uint16_t address) {
        return gb_rom_bank0_data[address & 0x3FFF];
    }

    // Direct reads from active switchable bank buffer ($4000 - $7FFF)
    static inline uint8_t readSwitchableBank(uint16_t address) {
        return s_activeSwitchableBank ? s_activeSwitchableBank[address & 0x3FFF] : 0xFF;
    }

    // Selects switchable ROM bank (MBC3 logic: bank 0 maps to 1, masked 0x3F)
    static bool selectRomBank(uint8_t bank);

    // Returns currently active switchable bank number (1..63)
    static uint8_t getCurrentBank();

    // Returns pointer to raw Bank 0 in Flash (16 KB)
    static const uint8_t* getFixedBankBuffer();

    // Returns pointer to active switchable bank buffer in SRAM (16 KB)
    static const uint8_t* getSwitchableBankBuffer();

    // Runs comprehensive validation (all 64 bank CRCs, MBC3 remapping, and 4-slot LRU behavior)
    static bool validate();

    // Benchmarks decompression time for a specific bank in microseconds
    static uint32_t benchmarkBankDecompress(uint8_t bank);

    // Returns total compressed flash size (including Bank 0 and index)
    static size_t getTotalFlashSize();

    // Returns SRAM memory footprint used by the ROM subsystem
    static size_t getRamFootprint();

    // Telemetry counters
    static uint32_t getTotalRequests();
    static uint32_t getCacheHitCount();
    static uint32_t getCacheMissCount();
    static uint32_t getDecompressionCount();
    static uint32_t getEvictionCount();
    static uint32_t getTotalDecompUs();
    static uint32_t getMaxDecompUs();
    static uint16_t getBankDecompCount(uint8_t bank);
    static void     getSlotInfo(uint8_t slotIdx, uint8_t& outBank, bool& outValid, uint32_t& outLastAccess);
    static void     resetMetrics();

private:
    // 4 fixed-size cache slots
    static GbRomCacheSlot s_slots[GB_ROM_CACHE_SLOTS];

    // Contiguous allocation tracking
    static uint8_t* s_contiguousBuffer;
    static bool     s_isContiguous;

    // Pointer to active switchable bank in SRAM (references one of the 4 slots)
    static uint8_t* s_activeSwitchableBank;

    // Active bank number (1..63)
    static uint8_t  s_currentLoadedBank;
    static bool     s_initialized;

    // Monotonic access sequence counter for LRU ordering
    static uint32_t s_accessCounter;

    // Telemetry counters
    static uint32_t s_totalRequests;
    static uint32_t s_cacheHitCount;
    static uint32_t s_decompressCount;
    static uint32_t s_evictionCount;
    static uint32_t s_totalDecompUs;
    static uint32_t s_maxDecompUs;
    static uint16_t s_bankDecompCounts[GB_ROM_TOTAL_BANKS];

    // Internal CRC32 calculator for validation
    static uint32_t calculateCrc32(const uint8_t* data, size_t length);
};

#endif // GB_ROM_H
