#ifndef GB_ROM_H
#define GB_ROM_H

#include <stdint.h>
#include <stddef.h>
#include "GbRomIndex.h"

// Assembly symbols embedded in flash via GbRomData.S
extern "C" {
    extern const uint8_t  gb_rom_bank0_data[];
    extern const uint32_t gb_rom_bank0_size;
    extern const uint8_t  gb_rom_compressed_data[];
    extern const uint32_t gb_rom_compressed_size;
}

class GbRom {
public:
    // Initialize the ROM subsystem (allocates 16 KB buffer once and caches Bank 1)
    static bool init();

    // Deinitialize the ROM subsystem and release the 16 KB switchable bank buffer
    static void deinit();

    // Direct reads from raw Bank 0 in flash ($0000 - $3FFF)
    static inline uint8_t readFixedBank(uint16_t address) {
        return gb_rom_bank0_data[address & 0x3FFF];
    }

    // Direct reads from decompressed switchable bank buffer ($4000 - $7FFF)
    static inline uint8_t readSwitchableBank(uint16_t address) {
        return s_switchableBankRam ? s_switchableBankRam[address & 0x3FFF] : 0xFF;
    }

    // Selects switchable ROM bank (MBC3 logic: bank 0 maps to 1, masked 0x3F)
    static bool selectRomBank(uint8_t bank);

    // Returns currently active switchable bank number (1..63)
    static uint8_t getCurrentBank();

    // Returns pointer to raw Bank 0 in Flash (16 KB)
    static const uint8_t* getFixedBankBuffer();

    // Returns pointer to active switchable bank buffer in SRAM (16 KB)
    static const uint8_t* getSwitchableBankBuffer();

    // Runs comprehensive validation across all 64 banks against precomputed CRCs
    static bool validate();

    // Benchmarks decompression time for a specific bank in microseconds
    static uint32_t benchmarkBankDecompress(uint8_t bank);

    // Returns total compressed flash size (including Bank 0 and index)
    static size_t getTotalFlashSize();

    // Returns SRAM memory footprint used by the ROM subsystem
    static size_t getRamFootprint();

    // Telemetry counters
    static uint32_t getDecompressionCount();
    static uint32_t getCacheHitCount();
    static void     resetMetrics();

private:
    // Reusable 16 KB buffer allocated once on init (0 dynamic allocations during bank switches)
    static uint8_t* s_switchableBankRam;
    static uint8_t  s_currentLoadedBank;
    static bool     s_initialized;
    static uint32_t s_decompressCount;
    static uint32_t s_cacheHitCount;

    // Internal CRC32 calculator for validation
    static uint32_t calculateCrc32(const uint8_t* data, size_t length);
};

#endif // GB_ROM_H
