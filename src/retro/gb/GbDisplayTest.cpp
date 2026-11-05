#include <Arduino.h>

#ifdef F
#undef F
#endif

#include "GbDisplayTest.h"
#include "GbDisplayAdapter.h"
#include "GbRomBridge.h"
#include "GbRom.h"
#include "GbInput.h"
#include "core/gnuboy.h"
#include "core/hw.h"
#include "core/cpu.h"
#include "config.h"

static bool s_running = false;
static Adafruit_ST7735* s_activeTft = nullptr;

// Video frame callback invoked synchronously by gnuboy_run() on VBLANK start
static void onGbVideoFrame(void* buffer) {
    if (buffer) {
        GbDisplayAdapter::renderFrame((const uint8_t*)buffer, GB.video.palette);
    }
}

bool GbDisplayTest::init(Adafruit_ST7735& tft) {
    s_activeTft = &tft;

    gb_diag_log_heap("1. Start of GbDisplayTest::init()", -1, 0, NULL, NULL);

    // 1. Initialize Display & Letterbox borders
    s_activeTft->setRotation(0);
    s_activeTft->setSPISpeed(40000000UL);
    s_activeTft->fillScreen(0x0000);

    // 2. Initialize ROM Backend Bridge
    if (!gb_rom_bridge_init()) {
        Serial.println("[GbDisplayTest] ERROR: gb_rom_bridge_init() failed!");
        return false;
    }

    gb_diag_log_heap("2. After GbRom::init()", -1, 0, NULL, NULL);

    // 3. Validate Cartridge
    if (!gb_rom_bridge_validate_cartridge()) {
        Serial.println("[GbDisplayTest] ERROR: Cartridge validation failed!");
        GbRom::deinit();
        return false;
    }

    // 4. Initialize Display Adapter (allocates 23,040-byte framebuffer)
    if (!GbDisplayAdapter::init(s_activeTft)) {
        Serial.println("[GbDisplayTest] ERROR: GbDisplayAdapter::init() failed!");
        GbRom::deinit();
        return false;
    }

    gb_diag_log_heap("After Display Adapter Init", -1, 0, NULL, NULL);

    GbDisplayAdapter::clearBorders();

    gb_diag_log_heap("Before Gnuboy Initialization", -1, 0, NULL, NULL);

    // 5. Initialize Gnuboy Core with GB_PIXEL_PALETTED format and synchronous video callback
    if (gnuboy_init(0, GB_AUDIO_STEREO_S16, GB_PIXEL_PALETTED, onGbVideoFrame, NULL) < 0) {
        Serial.println("[GbDisplayTest] ERROR: gnuboy_init() failed!");
        GbDisplayAdapter::deinit();
        GbRom::deinit();
        return false;
    }

    gb_diag_log_heap("After gnuboy_init()", -1, 0, NULL, NULL);

    // Bind allocated framebuffer
    gnuboy_set_framebuffer(GbDisplayAdapter::getFramebuffer());
    gnuboy_set_soundbuffer(NULL, 0);

    // 6. Load Cartridge & MBC3 via Bridge
    int loadRes = gnuboy_load_rom_from_bridge();
    if (loadRes != 0) {
        Serial.printf("[GbDisplayTest] ERROR: gnuboy_load_rom_from_bridge() failed: %d\n", loadRes);
        gnuboy_deinit();
        GbDisplayAdapter::deinit();
        GbRom::deinit();
        return false;
    }

    // 7. Hard reset CPU & PPU
    gnuboy_reset(true);
    GbInput::init();
    s_running = true;

    return true;
}

void GbDisplayTest::update(Adafruit_ST7735& tft) {
    if (!s_running) return;
    GbInput::update();
    if (GbInput::isExitRequested()) {
        deinit();
        GbInput::waitForExitRelease();
        return;
    }
    gnuboy_set_pad(GbInput::getPadState());
    gnuboy_run(true);
}

void GbDisplayTest::deinit() {
    if (s_running) {
        // 1. Stop frame execution and rendering
        s_running = false;
        s_activeTft = nullptr;
        GbInput::deinit();

        gb_diag_log_heap("Teardown 1: Before Teardown", -1, 0, NULL, NULL);

        // 2. Gnuboy Core teardown (frees Cartridge RAM, WRAM, VRAM, Sound, LCD BUF)
        gnuboy_deinit();
        gb_diag_log_heap("Teardown 2: After gnuboy_deinit()", -1, 0, NULL, NULL);

        // 3. Display Adapter teardown (frees Framebuffer & Burst Buffer)
        GbDisplayAdapter::deinit();
        gb_diag_log_heap("Teardown 3: After GbDisplayAdapter::deinit()", -1, 0, NULL, NULL);

        // 4. ROM Subsystem teardown (frees 16 KB switchable ROM bank buffer)
        GbRom::deinit();
        gb_diag_log_heap("Teardown 4: After GbRom::deinit()", -1, 0, NULL, NULL);

        // 5. End of complete teardown sequence
        gb_diag_log_heap("Teardown 5: End of GbDisplayTest::deinit()", -1, 0, NULL, NULL);
    }
}

bool GbDisplayTest::isRunning() {
    return s_running;
}

bool GbDisplayTest::run(Adafruit_ST7735& tft, uint32_t targetFrames) {
    uint32_t heapBefore     = ESP.getFreeHeap();
    uint32_t maxBlockBefore = ESP.getMaxAllocHeap();

    Serial.println();
    Serial.println("==================================================================");
    Serial.println("       Gnuboy TFT Display Integration Test (Phase 3)              ");
    Serial.println("==================================================================");
    Serial.printf("[GbTest] Initial Free Heap:       %6u bytes\n", heapBefore);
    Serial.printf("[GbTest] Initial Max Free Block:  %6u bytes\n", maxBlockBefore);

    Serial.println("[GbTest] Step 1: Initializing Hardware Display & Core...");
    if (!init(tft)) {
        Serial.println("[GbTest] ERROR: Initialization failed!");
        return false;
    }

    uint32_t heapAfterInit     = ESP.getFreeHeap();
    uint32_t maxBlockAfterInit = ESP.getMaxAllocHeap();
    uint32_t initAllocBytes    = (heapBefore > heapAfterInit) ? (heapBefore - heapAfterInit) : 0;

    gb_rom_metadata_t meta;
    gb_rom_bridge_get_metadata(&meta);

    Serial.printf("[GbTest] Cartridge: %s | MBC3 | ROM Code: 0x%02X | RAM Code: 0x%02X\n",
                  meta.title, meta.rom_size_code, meta.ram_size_code);
    Serial.printf("[GbTest] Display: 128x160 TFT Portrait | Active Game Area: 128x115 at (0, 22)\n");
    Serial.printf("[GbTest] Free Heap after Init:    %6u bytes (Used by Init: %u bytes)\n",
                  heapAfterInit, initAllocBytes);
    Serial.printf("[GbTest] Max Free Block after:    %6u bytes\n", maxBlockAfterInit);
    Serial.printf("[GbTest] CPU Reset Complete: PC = 0x%04X, SP = 0x%04X\n",
                  (unsigned)GB.cpu->pc.w, (unsigned)GB.cpu->sp.w);

    Serial.printf("[GbTest] Step 2: Executing & Rendering %u Frames on TFT...\n", targetFrames);
    Serial.println("[GbTest] Controls: JOY=D-pad, GPIO13=A, GPIO32=B, GPIO25=START (hold 2s to exit)");
    Serial.println("--------------------------------------------------------------------------------------------------------");
    Serial.println(" Frame |   PC   |   SP   | ROM/RAM | Emu (ms) | Scale (us) | TFT (ms) | Total (ms) |  FPS  | FreeHeap | Decomp/Hit");
    Serial.println("--------------------------------------------------------------------------------------------------------");

    uint32_t wallStartMs = millis();
    uint32_t lastPrintMs = wallStartMs;

    uint32_t accumEmuUs    = 0;
    uint32_t accumScaleUs  = 0;
    uint32_t accumTftUs    = 0;
    uint32_t accumTotalUs  = 0;
    uint32_t frameCountSec = 0;

    bool exitedEarly = false;

    for (uint32_t f = 1; f <= targetFrames; f++) {
        // Poll physical Game Boy controls (D-Pad, A, B, START, and safe exit gesture)
        GbInput::update();
        if (GbInput::isExitRequested()) {
            Serial.printf("[GbTest] Safe exit triggered via BTN_BACK long-hold at frame %u.\n", f);
            exitedEarly = true;
            break;
        }
        gnuboy_set_pad(GbInput::getPadState());

        uint32_t fStart = micros();

        // Execute 1 complete emulated Game Boy frame with actual TFT rendering
        gnuboy_run(true);

        uint32_t fTotal = micros() - fStart;
        uint32_t scaleUs = GbDisplayAdapter::getLastRenderTimeUs();
        uint32_t tftUs   = GbDisplayAdapter::getLastTftTimeUs();
        uint32_t dispUs  = scaleUs + tftUs;
        uint32_t emuUs   = (fTotal > dispUs) ? (fTotal - dispUs) : 0;

        accumEmuUs   += emuUs;
        accumScaleUs += scaleUs;
        accumTftUs   += tftUs;
        accumTotalUs += fTotal;
        frameCountSec++;

        // Print telemetry checkpoint at frame 1, 10, 30, and every 60 frames (~1s interval)
        if (f == 1 || f == 10 || f == 30 || (f % 60 == 0) || f == targetFrames) {
            float avgEmuMs   = (float)accumEmuUs   / (frameCountSec * 1000.0f);
            float avgScaleUs = (float)accumScaleUs / (float)frameCountSec;
            float avgTftMs   = (float)accumTftUs   / (frameCountSec * 1000.0f);
            float avgTotalMs = (float)accumTotalUs / (frameCountSec * 1000.0f);
            float fps        = (avgTotalMs > 0.0f) ? (1000.0f / avgTotalMs) : 0.0f;

            uint8_t curRomBank = cart.rombank;
            uint8_t curRamBank = cart.rambank;

            Serial.printf("%6u | 0x%04X | 0x%04X |  %2u/%-2u  |  %5.2f   |   %5.0f    |  %5.2f   |   %5.2f    | %5.1f |  %6u  | %4u/%-4u\n",
                          f,
                          (unsigned)GB.cpu->pc.w,
                          (unsigned)GB.cpu->sp.w,
                          curRomBank, curRamBank,
                          avgEmuMs, avgScaleUs, avgTftMs, avgTotalMs, fps,
                          ESP.getFreeHeap(),
                          gb_rom_bridge_get_decompression_count(),
                          gb_rom_bridge_get_cache_hit_count());

            accumEmuUs    = 0;
            accumScaleUs  = 0;
            accumTftUs    = 0;
            accumTotalUs  = 0;
            frameCountSec = 0;
        }
    }

    uint32_t totalDurationMs = millis() - wallStartMs;
    uint32_t totalFramesRendered = GbDisplayAdapter::getTotalFramesRendered();
    float overallFps = (totalDurationMs > 0) ? ((float)totalFramesRendered * 1000.0f / totalDurationMs) : 0.0f;

    Serial.println("--------------------------------------------------------------------------------------------------------");
    Serial.printf("[GbTest] Rendered %u frames in %u ms (Overall Average: %.1f FPS)\n",
                  totalFramesRendered, totalDurationMs, overallFps);

    if (exitedEarly) {
        GbInput::waitForExitRelease();
    }

    Serial.println("[GbTest] Step 3: Tearing Down & Checking Memory Safety...");
    uint32_t heapBeforeDeinit    = ESP.getFreeHeap();
    deinit();

    uint32_t heapAfterDeinit     = ESP.getFreeHeap();
    uint32_t maxBlockAfterDeinit = ESP.getMaxAllocHeap();
    uint32_t deinitFreedBytes    = (heapAfterDeinit > heapBeforeDeinit) ? (heapAfterDeinit - heapBeforeDeinit) : 0;
    int32_t emulatorLeak         = (int32_t)(initAllocBytes - deinitFreedBytes);
    int32_t residualLeak         = (int32_t)(heapBefore - heapAfterDeinit);

    Serial.printf("[GbTest] Emulator Allocated in Init:  %6u bytes\n", initAllocBytes);
    Serial.printf("[GbTest] Emulator Reclaimed in Deinit: %6u bytes\n", deinitFreedBytes);
    Serial.printf("[GbTest] Emulator Net Leak:         %6d bytes\n", emulatorLeak);
    Serial.printf("[GbTest] Free Heap after Deinit:   %6u bytes\n", heapAfterDeinit);
    Serial.printf("[GbTest] Max Free Block after:     %6u bytes\n", maxBlockAfterDeinit);
    Serial.printf("[GbTest] Residual Heap Delta:      %6d bytes\n", residualLeak);
    if (residualLeak > 0 && residualLeak <= 512) {
        Serial.printf("[GbTest] Note: %d-byte residual delta is within expected asynchronous Wi-Fi/lwIP stack activity.\n", residualLeak);
    }

    bool success = (!exitedEarly || totalFramesRendered >= 10) && (residualLeak <= 512);
    Serial.println("==================================================================");
    Serial.printf("       Gnuboy TFT Display Integration Test: %s           \n",
                  success ? "SUCCESS!" : "FAILED!");
    Serial.println("==================================================================");

    return success;
}
