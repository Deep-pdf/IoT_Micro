#include "GbSave.h"
#include "config.h"
#include <LittleFS.h>
#include <Arduino.h>
#include <esp_partition.h>

#ifdef F
#undef F
#endif

#include "core/hw.h"
#include "core/gnuboy.h"

static bool     s_fsReady      = false;
static uint32_t s_lastDirtyMs  = 0;
static unsigned s_lastDirtyBit = 0;
static uint32_t s_lastSaveMs   = 0;
static uint32_t s_saveCount    = 0;

// Debounce interval: wait 2.0 seconds of write inactivity before committing auto-save
static constexpr uint32_t SAVE_DEBOUNCE_MS     = 2000;
// Minimum interval between successive automatic flash writes to protect flash endurance
static constexpr uint32_t MIN_SAVE_INTERVAL_MS = 10000;

// Standard IEEE 802.3 32-bit CRC calculation (matches zlib / standard tools)
uint32_t GbSave::calcCrc32(const uint8_t* data, size_t length) {
    if (!data || length == 0) return 0;
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            crc = (crc >> 1) ^ (0xEDB88320UL & (-(int32_t)(crc & 1)));
        }
    }
    return ~crc;
}

uint32_t GbSave::getBankCrc32(int bank) {
    if (bank < 0 || bank >= 4 || !cart.rambanks[bank]) return 0;
    return calcCrc32(cart.rambanks[bank], SRAM_BANK_SIZE);
}

// Safely inspects the physical flash partition to confirm if it is genuinely blank / unformatted.
// Returns true ONLY if all inspected blocks are 100% erased flash (0xFF).
static bool isPartitionVirgin(const char* label = nullptr) {
    const esp_partition_t* part = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA,
        ESP_PARTITION_SUBTYPE_DATA_SPIFFS,
        label
    );
    if (!part) {
        Serial.println("[GbSave] ERROR: SPIFFS/LittleFS partition not found in partition table!");
        return false;
    }

    uint8_t buf[256];

    // Check first 4 KB block (where LittleFS superblock is written)
    for (size_t offset = 0; offset < 4096; offset += sizeof(buf)) {
        if (esp_partition_read(part, offset, buf, sizeof(buf)) != ESP_OK) {
            return false;
        }
        for (size_t i = 0; i < sizeof(buf); i++) {
            if (buf[i] != 0xFF) return false; // Contains non-0xFF data -> not virgin!
        }
    }

    // Check final 4 KB block of the partition
    size_t lastBlockOffset = part->size - 4096;
    for (size_t offset = lastBlockOffset; offset < part->size; offset += sizeof(buf)) {
        if (esp_partition_read(part, offset, buf, sizeof(buf)) != ESP_OK) {
            return false;
        }
        for (size_t i = 0; i < sizeof(buf); i++) {
            if (buf[i] != 0xFF) return false;
        }
    }

    return true; // Genuinely blank/unformatted flash confirmed!
}

// Verifies write, read, and delete functionality on the mounted filesystem
static bool verifyFilesystemIO() {
    const char* testPath = "/.fs_test";
    const char* testData = "POKEMON_SRAM_OK";
    size_t testLen = strlen(testData);

    // 1. Write test file
    File f = LittleFS.open(testPath, "w");
    if (!f) {
        Serial.println("[GbSave] ERROR: Filesystem I/O test failed to create test file!");
        return false;
    }
    size_t written = f.write((const uint8_t*)testData, testLen);
    f.flush();
    f.close();
    if (written != testLen) {
        Serial.println("[GbSave] ERROR: Filesystem I/O test wrote incomplete data!");
        LittleFS.remove(testPath);
        return false;
    }

    // 2. Read test file back
    f = LittleFS.open(testPath, "r");
    if (!f) {
        Serial.println("[GbSave] ERROR: Filesystem I/O test failed to re-open test file!");
        LittleFS.remove(testPath);
        return false;
    }
    char readBuf[32] = {0};
    size_t readLen = f.read((uint8_t*)readBuf, sizeof(readBuf) - 1);
    f.close();
    LittleFS.remove(testPath);

    if (readLen != testLen || strcmp(readBuf, testData) != 0) {
        Serial.println("[GbSave] ERROR: Filesystem I/O test data verification failed!");
        return false;
    }

    return true;
}

bool GbSave::init() {
    if (s_fsReady) return true;

    Serial.println("[GbSave] Initializing LittleFS storage subsystem...");

    // 1. Initial attempt: mount existing filesystem without automatic formatting
    bool mounted = LittleFS.begin(false);

    // 2. If initial mount fails, perform safe first-time initialization check
    if (!mounted) {
        Serial.println("[GbSave] Initial LittleFS mount failed. Inspecting physical partition...");
        if (isPartitionVirgin()) {
            Serial.println("[GbSave] Virgin unformatted flash partition confirmed (100% 0xFF).");
            Serial.println("[GbSave] Performing safe one-time LittleFS format...");
            uint32_t t0 = millis();
            if (LittleFS.format()) {
                uint32_t fmtDuration = millis() - t0;
                Serial.printf("[GbSave] One-time LittleFS format completed in %u ms.\n", fmtDuration);
                mounted = LittleFS.begin(false);
                if (mounted) {
                    Serial.println("[GbSave] LittleFS mounted successfully after initial format.");
                } else {
                    Serial.println("[GbSave] ERROR: LittleFS failed to mount after successful format!");
                }
            } else {
                Serial.println("[GbSave] ERROR: LittleFS.format() failed!");
            }
        } else {
            Serial.println("[GbSave] ERROR: Partition is NOT blank! Preserving existing non-blank data.");
            Serial.println("[GbSave] Blanket format rejected to prevent data loss. Persistent save disabled.");
        }
    }

    if (!mounted) {
        s_fsReady = false;
        return false;
    }

    // 3. Verify read/write/delete functionality on the mounted filesystem
    if (!verifyFilesystemIO()) {
        Serial.println("[GbSave] ERROR: Filesystem I/O verification failed! Persistent save disabled.");
        LittleFS.end();
        s_fsReady = false;
        return false;
    }

    s_fsReady = true;

    // Report partition telemetry
    size_t total = LittleFS.totalBytes();
    size_t used  = LittleFS.usedBytes();
    Serial.printf("[GbSave] LittleFS verified & ready | Partition: %u KB | Used: %u KB | Free: %u KB\n",
                  (unsigned)(total / 1024), (unsigned)(used / 1024), (unsigned)((total - used) / 1024));

    // Recovery check for interrupted atomic commits:
    // If primary save is missing, check if a valid temporary or backup file is available
    if (!LittleFS.exists(SAVE_FILE_PATH)) {
        if (LittleFS.exists(TEMP_FILE_PATH)) {
            File f = LittleFS.open(TEMP_FILE_PATH, "r");
            if (f && f.size() == SRAM_TOTAL_SIZE) {
                f.close();
                LittleFS.rename(TEMP_FILE_PATH, SAVE_FILE_PATH);
                Serial.println("[GbSave] RECOVERY: Restored primary save from completed temporary file.");
            } else if (f) {
                f.close();
            }
        } else if (LittleFS.exists(BACKUP_FILE_PATH)) {
            File f = LittleFS.open(BACKUP_FILE_PATH, "r");
            if (f && f.size() == SRAM_TOTAL_SIZE) {
                f.close();
                LittleFS.rename(BACKUP_FILE_PATH, SAVE_FILE_PATH);
                Serial.println("[GbSave] RECOVERY: Restored primary save from backup file.");
            } else if (f) {
                f.close();
            }
        }
    }

    // Clean up any remaining temporary or backup files
    if (LittleFS.exists(TEMP_FILE_PATH)) {
        LittleFS.remove(TEMP_FILE_PATH);
    }
    if (LittleFS.exists(BACKUP_FILE_PATH) && LittleFS.exists(SAVE_FILE_PATH)) {
        LittleFS.remove(BACKUP_FILE_PATH);
    }

    return true;
}

bool GbSave::load() {
    if (!s_fsReady) {
        Serial.println("[GbSave] WARNING: LittleFS not ready. Skipping save load.");
        return false;
    }

    // 1. Verify all 4 cartridge RAM bank pointers
    for (int i = 0; i < 4; i++) {
        if (!cart.rambanks[i]) {
            Serial.printf("[GbSave] ERROR: cart.rambanks[%d] is NULL! Cannot load save.\n", i);
            return false;
        }
    }

    // 2. Check if primary save file exists
    if (!LittleFS.exists(SAVE_FILE_PATH)) {
        Serial.printf("[GbSave] No existing save found at %s. Starting clean cartridge RAM.\n", SAVE_FILE_PATH);
        cart.sram_dirty = 0;
        cart.sram_saved = 0;
        return true;
    }

    // 3. Open primary save file
    File f = LittleFS.open(SAVE_FILE_PATH, "r");
    if (!f) {
        Serial.printf("[GbSave] ERROR: Failed to open %s for reading!\n", SAVE_FILE_PATH);
        return false;
    }

    // 4. Validate exact file size (32,768 bytes)
    size_t fileSize = f.size();
    if (fileSize != SRAM_TOTAL_SIZE) {
        Serial.printf("[GbSave] ERROR: Invalid save file size (%u != %u)! Rejecting corrupted save.\n",
                      (unsigned)fileSize, (unsigned)SRAM_TOTAL_SIZE);
        f.close();
        // Clear RAM to clean 0xFF state to prevent running with partial/corrupt data
        for (int i = 0; i < 4; i++) {
            memset(cart.rambanks[i], 0xFF, SRAM_BANK_SIZE);
        }
        cart.sram_dirty = 0;
        cart.sram_saved = 0;
        return false;
    }

    // 5. Read all 4 banks sequentially
    uint32_t startMs = millis();
    bool readSuccess = true;
    f.seek(0);

    for (int i = 0; i < 4; i++) {
        size_t readLen = f.read(cart.rambanks[i], SRAM_BANK_SIZE);
        if (readLen != SRAM_BANK_SIZE) {
            Serial.printf("[GbSave] ERROR: Short read on bank %d (%u / %u bytes)!\n",
                          i, (unsigned)readLen, (unsigned)SRAM_BANK_SIZE);
            readSuccess = false;
            break;
        }
    }

    f.close();

    // 6. If reading failed partway through, restore clean RAM
    if (!readSuccess) {
        Serial.println("[GbSave] ERROR: Read failure during load! Restoring clean 0xFF RAM.");
        for (int i = 0; i < 4; i++) {
            memset(cart.rambanks[i], 0xFF, SRAM_BANK_SIZE);
        }
        cart.sram_dirty = 0;
        cart.sram_saved = 0;
        return false;
    }

    // 7. Mark RAM clean and saved
    cart.sram_dirty = 0;
    cart.sram_saved = 0x0F;
    s_lastDirtyBit  = 0;

    uint32_t durationMs = millis() - startMs;
    Serial.printf("[GbSave] Successfully loaded 32,768 B from %s across 4 banks in %u ms.\n",
                  SAVE_FILE_PATH, durationMs);
    Serial.printf("[GbSave] Restored Bank CRCs | Bank0: 0x%08X | Bank1: 0x%08X | Bank2: 0x%08X | Bank3: 0x%08X\n",
                  getBankCrc32(0), getBankCrc32(1), getBankCrc32(2), getBankCrc32(3));
    return true;
}

bool GbSave::save() {
    if (!s_fsReady) {
        Serial.println("[GbSave] ERROR: Cannot save - LittleFS not mounted.");
        return false;
    }

    // 1. Verify all 4 cartridge RAM bank pointers
    for (int i = 0; i < 4; i++) {
        if (!cart.rambanks[i]) {
            Serial.printf("[GbSave] ERROR: cart.rambanks[%d] is NULL! Aborting save.\n", i);
            return false;
        }
    }

    uint32_t startMs = millis();

    // 2. Remove any previous stale temporary file
    if (LittleFS.exists(TEMP_FILE_PATH)) {
        LittleFS.remove(TEMP_FILE_PATH);
    }

    // 3. Open temporary file for writing
    File f = LittleFS.open(TEMP_FILE_PATH, "w");
    if (!f) {
        Serial.println("[GbSave] ERROR: Failed to open temporary save file for writing!");
        return false;
    }

    // 4. Write all 4 banks sequentially (32,768 bytes total)
    size_t totalWritten = 0;
    for (int i = 0; i < 4; i++) {
        size_t written = f.write(cart.rambanks[i], SRAM_BANK_SIZE);
        if (written != SRAM_BANK_SIZE) {
            Serial.printf("[GbSave] ERROR: Write failure on bank %d (wrote %u / %u bytes)!\n",
                          i, (unsigned)written, (unsigned)SRAM_BANK_SIZE);
            f.close();
            LittleFS.remove(TEMP_FILE_PATH);
            return false;
        }
        totalWritten += written;
    }

    f.flush();
    f.close();

    if (totalWritten != SRAM_TOTAL_SIZE) {
        Serial.printf("[GbSave] ERROR: Incomplete write total (%u != %u)!\n",
                      (unsigned)totalWritten, (unsigned)SRAM_TOTAL_SIZE);
        LittleFS.remove(TEMP_FILE_PATH);
        return false;
    }

    // 5. Re-open and verify file size and contents
    File verify = LittleFS.open(TEMP_FILE_PATH, "r");
    if (!verify) {
        Serial.println("[GbSave] ERROR: Failed to re-open temporary file for verification!");
        LittleFS.remove(TEMP_FILE_PATH);
        return false;
    }

    if (verify.size() != SRAM_TOTAL_SIZE) {
        Serial.printf("[GbSave] ERROR: Verified size mismatch (%u != %u)!\n",
                      (unsigned)verify.size(), (unsigned)SRAM_TOTAL_SIZE);
        verify.close();
        LittleFS.remove(TEMP_FILE_PATH);
        return false;
    }

    // Verify written data matches RAM exactly
    uint8_t chkBuf[256];
    bool dataMatches = true;
    verify.seek(0);
    for (int i = 0; i < 4 && dataMatches; i++) {
        for (size_t offset = 0; offset < SRAM_BANK_SIZE; offset += sizeof(chkBuf)) {
            size_t r = verify.read(chkBuf, sizeof(chkBuf));
            if (r != sizeof(chkBuf) || memcmp(chkBuf, cart.rambanks[i] + offset, sizeof(chkBuf)) != 0) {
                dataMatches = false;
                break;
            }
        }
    }
    verify.close();

    if (!dataMatches) {
        Serial.println("[GbSave] ERROR: Data verification mismatch! Save rejected.");
        LittleFS.remove(TEMP_FILE_PATH);
        return false;
    }

    // 6. Safe atomic replacement with backup:
    // If a primary save already exists, back it up first
    bool hadExisting = LittleFS.exists(SAVE_FILE_PATH);
    if (hadExisting) {
        LittleFS.remove(BACKUP_FILE_PATH);
        if (!LittleFS.rename(SAVE_FILE_PATH, BACKUP_FILE_PATH)) {
            Serial.println("[GbSave] ERROR: Failed to rotate primary save to backup!");
            LittleFS.remove(TEMP_FILE_PATH);
            return false;
        }
    }

    // Rename temporary file to primary save file
    if (!LittleFS.rename(TEMP_FILE_PATH, SAVE_FILE_PATH)) {
        Serial.println("[GbSave] ERROR: Failed to commit temporary file to primary save!");
        // Restore backup if available
        if (hadExisting && LittleFS.exists(BACKUP_FILE_PATH)) {
            LittleFS.rename(BACKUP_FILE_PATH, SAVE_FILE_PATH);
        }
        return false;
    }

    // Commit succeeded! Safely remove backup
    if (hadExisting) {
        LittleFS.remove(BACKUP_FILE_PATH);
    }

    // 7. Clear dirty state and update telemetry
    cart.sram_dirty = 0;
    cart.sram_saved = 0x0F;
    s_lastDirtyBit  = 0;
    s_lastSaveMs    = millis();
    s_saveCount++;

    uint32_t durationMs = millis() - startMs;
    Serial.printf("[GbSave] Successfully saved 32,768 B to %s in %u ms (Save #%u)\n",
                  SAVE_FILE_PATH, durationMs, s_saveCount);
    Serial.printf("[GbSave] Committed Bank CRCs | Bank0: 0x%08X | Bank1: 0x%08X | Bank2: 0x%08X | Bank3: 0x%08X\n",
                  getBankCrc32(0), getBankCrc32(1), getBankCrc32(2), getBankCrc32(3));
    return true;
}

void GbSave::update() {
    if (!s_fsReady) return;

    unsigned dirty = cart.sram_dirty;
    uint32_t now   = millis();

    // If new writes have occurred, update dirty timestamp
    if (dirty != 0) {
        if (dirty != s_lastDirtyBit) {
            s_lastDirtyBit = dirty;
            s_lastDirtyMs  = now;
        }

        // If dirty state has settled (no new writes for SAVE_DEBOUNCE_MS)
        // and minimum interval between auto-saves has elapsed:
        if ((now - s_lastDirtyMs >= SAVE_DEBOUNCE_MS) &&
            (now - s_lastSaveMs >= MIN_SAVE_INTERVAL_MS)) {
            Serial.printf("[GbSave] Auto-save debounce triggered (dirty mask: 0x%X). Committing...\n", dirty);
            if (save()) {
                Serial.printf("[GbSave] Auto-save #%u committed successfully.\n", s_saveCount);
            } else {
                Serial.println("[GbSave] WARNING: Auto-save failed! Retaining dirty state.");
            }
        }
    } else {
        s_lastDirtyBit = 0;
    }
}

bool GbSave::isDirty() {
    return (cart.sram_dirty != 0);
}

void GbSave::deinit() {
    if (s_fsReady) {
        LittleFS.end();
        s_fsReady = false;
        Serial.println("[GbSave] LittleFS unmounted cleanly.");
    }
    s_lastDirtyBit = 0;
    s_lastDirtyMs  = 0;
}

bool GbSave::isFilesystemReady() {
    return s_fsReady;
}

size_t GbSave::getSaveFileSize() {
    if (!s_fsReady || !LittleFS.exists(SAVE_FILE_PATH)) return 0;
    File f = LittleFS.open(SAVE_FILE_PATH, "r");
    if (!f) return 0;
    size_t sz = f.size();
    f.close();
    return sz;
}

uint32_t GbSave::getLastSaveMs() {
    return s_lastSaveMs;
}

uint32_t GbSave::getSaveCount() {
    return s_saveCount;
}
