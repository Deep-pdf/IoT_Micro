#include "GbSave.h"
#include "config.h"
#include <LittleFS.h>
#include <Arduino.h>

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

bool GbSave::init() {
    if (s_fsReady) return true;

    Serial.println("[GbSave] Initializing LittleFS storage subsystem...");

    bool formatOnFail = false;
#ifdef GB_SAVE_FORMAT_ON_FAIL
    formatOnFail = GB_SAVE_FORMAT_ON_FAIL;
#endif

    // Mount LittleFS on the default 'spiffs' partition label
    if (!LittleFS.begin(formatOnFail)) {
        Serial.println("[GbSave] ERROR: LittleFS.begin() failed! Persistent save disabled.");
        s_fsReady = false;
        return false;
    }

    s_fsReady = true;

    // Report partition telemetry
    size_t total = LittleFS.totalBytes();
    size_t used  = LittleFS.usedBytes();
    Serial.printf("[GbSave] LittleFS mounted successfully | Partition: %u KB | Used: %u KB | Free: %u KB\n",
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
