#include <Arduino.h>

#ifdef F
#undef F
#endif

#include "GbHeadlessTest.h"
#include "GbRomBridge.h"
#include "core/gnuboy.h"
#include "core/hw.h"
#include "core/cpu.h"

bool GbHeadlessTest::run(uint32_t targetFrames) {
    uint32_t heapBefore     = ESP.getFreeHeap();
    uint32_t maxBlockBefore = ESP.getMaxAllocHeap();

    Serial.println();
    Serial.println("==================================================================");
    Serial.println("       Gnuboy Headless Emulation Verification Test (Phase 2)       ");
    Serial.println("==================================================================");
    Serial.printf("[GbTest] Initial Free Heap:       %6u bytes\n", heapBefore);
    Serial.printf("[GbTest] Initial Max Free Block:  %6u bytes\n", maxBlockBefore);

    // 1. Initialize ROM Backend Bridge
    Serial.println("[GbTest] Step 1: Initializing GbRomBridge...");
    if (!gb_rom_bridge_init()) {
        Serial.println("[GbTest] ERROR: gb_rom_bridge_init() failed!");
        return false;
    }

    // 2. Validate Cartridge Metadata & Header
    Serial.println("[GbTest] Step 2: Validating Cartridge Header & Checksums...");
    gb_rom_metadata_t meta;
    if (!gb_rom_bridge_get_metadata(&meta)) {
        Serial.println("[GbTest] ERROR: Failed to read ROM metadata!");
        return false;
    }

    Serial.printf("[GbTest]   Title:              %s\n", meta.title);
    Serial.printf("[GbTest]   Cartridge Type:     0x%02X (Expected 0x13 = MBC3+RAM+BATTERY)\n", meta.cartridge_type);
    Serial.printf("[GbTest]   ROM Size Code:      0x%02X (Expected 0x05 = 1MB / 64 banks)\n", meta.rom_size_code);
    Serial.printf("[GbTest]   RAM Size Code:      0x%02X (Expected 0x03 = 32KB / 4 banks)\n", meta.ram_size_code);
    Serial.printf("[GbTest]   Header Checksum:    0x%02X\n", meta.header_checksum);
    Serial.printf("[GbTest]   Global Checksum:    0x%04X\n", meta.global_checksum);

    if (!gb_rom_bridge_validate_cartridge()) {
        Serial.println("[GbTest] ERROR: Cartridge validation failed!");
        return false;
    }
    Serial.println("[GbTest] Cartridge validation PASSED (100% exact match).");

    // 3. Initialize Gnuboy Core
    Serial.println("[GbTest] Step 3: Initializing Gnuboy Emulator Core...");
    if (gnuboy_init(0, GB_AUDIO_STEREO_S16, GB_PIXEL_PALETTED, NULL, NULL) < 0) {
        Serial.println("[GbTest] ERROR: gnuboy_init() failed!");
        return false;
    }

    // Explicitly set null framebuffer and soundbuffer for headless test
    gnuboy_set_framebuffer(NULL);
    gnuboy_set_soundbuffer(NULL, 0);

    // 4. Load Cartridge through Bridge
    Serial.println("[GbTest] Step 4: Loading ROM & Setting Up MBC3 via Bridge...");
    int loadRes = gnuboy_load_rom_from_bridge();
    if (loadRes != 0) {
        Serial.printf("[GbTest] ERROR: gnuboy_load_rom_from_bridge() failed with code %d\n", loadRes);
        gnuboy_deinit();
        return false;
    }

    uint32_t heapAfterInit     = ESP.getFreeHeap();
    uint32_t maxBlockAfterInit = ESP.getMaxAllocHeap();
    uint32_t initAllocBytes    = heapBefore - heapAfterInit;

    Serial.printf("[GbTest] Core Initialized Successfully.\n");
    Serial.printf("[GbTest] Free Heap after Init:    %6u bytes (Used by Init: %u bytes)\n",
                  heapAfterInit, initAllocBytes);
    Serial.printf("[GbTest] Max Free Block after:    %6u bytes\n", maxBlockAfterInit);

    // 5. Hard Reset Emulator State
    Serial.println("[GbTest] Step 5: Performing Hard Reset (gnuboy_reset)...");
    gnuboy_reset(true);
    Serial.printf("[GbTest] CPU Reset Complete: PC = 0x%04X, SP = 0x%04X\n",
                  (unsigned)GB.cpu->pc.w, (unsigned)GB.cpu->sp.w);

    // 6. Execute Emulation Frames
    Serial.printf("[GbTest] Step 6: Executing %u Headless Frames...\n", targetFrames);
    Serial.println("------------------------------------------------------------------");
    Serial.println(" Frame |   PC   |   SP   | ROM Bank | RAM Bank | Decomps | CacheHits ");
    Serial.println("------------------------------------------------------------------");

    uint32_t startTime = millis();

    for (uint32_t f = 1; f <= targetFrames; f++) {
        // Run frame headlessly without TFT rendering
        gnuboy_run(false);

        // Telemetry checkpoints
        if (f == 1 || f == 10 || f == 20 || f == 30 || f == 60 || f == 90 || f == 120 || f == targetFrames) {
            uint16_t pc       = GB.cpu ? GB.cpu->pc.w : 0;
            uint16_t sp       = GB.cpu ? GB.cpu->sp.w : 0;
            uint8_t  romBank  = cart.rombank;
            uint8_t  ramBank  = cart.rambank;
            uint32_t decomps  = gb_rom_bridge_get_decompression_count();
            uint32_t hits     = gb_rom_bridge_get_cache_hit_count();

            Serial.printf("  %4u | 0x%04X | 0x%04X |    %2u    |    %2u    |   %3u   |   %5u\n",
                          f, pc, sp, romBank, ramBank, decomps, hits);
        }
    }

    uint32_t elapsedTime = millis() - startTime;
    uint32_t heapAfterRun = ESP.getFreeHeap();

    Serial.println("------------------------------------------------------------------");
    Serial.printf("[GbTest] Completed %u frames in %u ms (Average: %.2f ms/frame, %.1f FPS)\n",
                  targetFrames, elapsedTime,
                  (float)elapsedTime / targetFrames,
                  (float)targetFrames * 1000.0f / (elapsedTime > 0 ? elapsedTime : 1));

    // 7. Teardown & Leak Detection
    Serial.println("[GbTest] Step 7: Tearing Down & Checking for Memory Leaks...");
    gnuboy_deinit();

    uint32_t heapAfterTeardown = ESP.getFreeHeap();
    int32_t  memoryDelta       = (int32_t)heapBefore - (int32_t)heapAfterTeardown;

    Serial.printf("[GbTest] Free Heap after Deinit:  %6u bytes\n", heapAfterTeardown);
    Serial.printf("[GbTest] Residual Heap Delta:     %6d bytes\n", memoryDelta);

    Serial.println("==================================================================");
    if (GB.frames >= targetFrames) {
        Serial.println("       Gnuboy Headless Emulation Verification: SUCCESS!           ");
        Serial.println("==================================================================");
        return true;
    } else {
        Serial.println("       Gnuboy Headless Emulation Verification: FAILED!            ");
        Serial.println("==================================================================");
        return false;
    }
}
