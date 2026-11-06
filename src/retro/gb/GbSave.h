#ifndef GB_SAVE_H
#define GB_SAVE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

class GbSave {
public:
    static constexpr size_t SRAM_BANK_SIZE   = 8192;
    static constexpr size_t SRAM_TOTAL_SIZE  = 32768; // 4 banks * 8192 bytes
    static constexpr const char* SAVE_FILE_PATH   = "/pokemon_red.sav";
    static constexpr const char* TEMP_FILE_PATH   = "/pokemon_red.sav.tmp";
    static constexpr const char* BACKUP_FILE_PATH = "/pokemon_red.sav.bak";

    // Initializes LittleFS on the 896 KB 'spiffs' flash partition.
    // Mounts LittleFS safely. If virgin (100% 0xFF) flash is detected, performs a one-time format.
    // If existing non-blank data exists and mount fails, preserves data without formatting.
    // Verifies filesystem read/write/delete functionality and recovers interrupted commits.
    static bool init();

    // Loads persistent save data into cart.rambanks[0..3].
    // Must be called AFTER gnuboy_reset(true).
    // If save does not exist, leaves clean initialized RAM.
    // If size != 32768, safely rejects and restores clean RAM.
    // Clears dirty state on success.
    static bool load();

    // Saves all four 8 KB Cartridge RAM banks atomically to flash.
    // Writes to .tmp, verifies size and byte-level contents, rotates backup, and commits.
    // Clears dirty state on success. Retains dirty state on failure.
    static bool save();

    // Periodic check for auto-saving dirty SRAM during gameplay.
    // Debounces writes (waits 2.0s after last write before committing).
    // Call once per emulated frame.
    static void update();

    // Returns true if Cartridge RAM has unsaved modifications.
    static bool isDirty();

    // Cleans up save manager state across emulator sessions.
    static void deinit();

    // Diagnostic accessors & helpers
    static bool isFilesystemReady();
    static size_t getSaveFileSize();
    static uint32_t getLastSaveMs();
    static uint32_t getSaveCount();
    static uint32_t getBankCrc32(int bank);
    static uint32_t calcCrc32(const uint8_t* data, size_t length);
};

#endif // GB_SAVE_H
