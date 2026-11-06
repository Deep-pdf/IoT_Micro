#include <Arduino.h>

#ifdef F
#undef F
#endif

#include "GbBridge.h"
#include "GbDisplayAdapter.h"
#include "GbRomBridge.h"
#include "GbRom.h"
#include "GbInput.h"
#include "GbSave.h"
#include "core/gnuboy.h"
#include "core/hw.h"
#include "core/cpu.h"

static bool s_running = false;
static Adafruit_ST7735* s_activeTft = nullptr;

// Video frame callback invoked synchronously by gnuboy_run() on VBLANK start
static void onGbVideoFrame(void* buffer) {
    if (buffer) {
        GbDisplayAdapter::renderFrame((const uint8_t*)buffer, GB.video.palette);
    }
}

bool GbBridge::init(Adafruit_ST7735* display) {
    if (!display) return false;
    s_activeTft = display;

    // 1. Initialize Display & Letterbox borders
    s_activeTft->setRotation(0);
    s_activeTft->setSPISpeed(40000000UL);
    s_activeTft->fillScreen(0x0000);

    // 2. Initialize ROM Bridge Subsystem (allocates 4-slot LRU cache)
    if (!gb_rom_bridge_init()) {
        Serial.println("[GbBridge] ERROR: gb_rom_bridge_init() failed!");
        return false;
    }

    // 3. Validate Cartridge Header
    if (!gb_rom_bridge_validate_cartridge()) {
        Serial.println("[GbBridge] ERROR: Cartridge validation failed!");
        GbRom::deinit();
        return false;
    }

    // 4. Initialize Display Adapter (allocates 23,040-byte framebuffer)
    if (!GbDisplayAdapter::init(s_activeTft)) {
        Serial.println("[GbBridge] ERROR: GbDisplayAdapter::init() failed!");
        GbRom::deinit();
        return false;
    }

    GbDisplayAdapter::clearBorders();

    // 5. Initialize Gnuboy Core with GB_PIXEL_PALETTED format and synchronous video callback
    if (gnuboy_init(0, GB_AUDIO_STEREO_S16, GB_PIXEL_PALETTED, onGbVideoFrame, NULL) < 0) {
        Serial.println("[GbBridge] ERROR: gnuboy_init() failed!");
        GbDisplayAdapter::deinit();
        GbRom::deinit();
        return false;
    }

    // Bind allocated framebuffer
    gnuboy_set_framebuffer(GbDisplayAdapter::getFramebuffer());
    gnuboy_set_soundbuffer(NULL, 0);

    // 6. Load Cartridge & MBC3 via Bridge (allocates 4 cartridge RAM banks)
    int loadRes = gnuboy_load_rom_from_bridge();
    if (loadRes != 0) {
        Serial.printf("[GbBridge] ERROR: gnuboy_load_rom_from_bridge() failed: %d\n", loadRes);
        gnuboy_deinit();
        GbDisplayAdapter::deinit();
        GbRom::deinit();
        return false;
    }

    // 7. Hard reset CPU & PPU (initializes cartridge RAM to 0xFF)
    gnuboy_reset(true);

    // 8. Persistent Save: Mount LittleFS and restore cartridge SRAM
    GbSave::init();
    GbSave::load();

    // 9. Initialize Game Boy controls
    GbInput::init();
    s_running = true;

    Serial.println("[GbBridge] Pokémon Red initialized successfully.");
    return true;
}

void GbBridge::runFrame() {
    if (!s_running) return;

    // Poll physical Game Boy controls
    GbInput::update();
    gnuboy_set_pad(GbInput::getPadState());

    // Execute 1 complete emulated Game Boy frame with actual TFT rendering
    gnuboy_run(true);

    // Service debounced persistent save if Cartridge RAM was modified
    GbSave::update();
}

void GbBridge::stop() {
    if (s_running) {
        s_running = false;
        s_activeTft = nullptr;
        GbInput::deinit();

        // 1. Flush any pending dirty SRAM to flash before freeing memory
        if (GbSave::isDirty()) {
            Serial.println("[GbBridge] SRAM is dirty on exit. Flushing to flash...");
            if (GbSave::save()) {
                Serial.println("[GbBridge] Persistent save committed successfully on exit.");
            } else {
                Serial.println("[GbBridge] ERROR: Failed to commit persistent save on exit!");
            }
        }
        GbSave::deinit();

        // 2. Gnuboy Core teardown (frees Cartridge RAM, WRAM, VRAM, Sound, LCD BUF)
        gnuboy_deinit();

        // 3. Display Adapter teardown (frees Framebuffer & Burst Buffer)
        GbDisplayAdapter::deinit();

        // 4. ROM Subsystem teardown (frees 4-slot 64 KB ROM cache)
        GbRom::deinit();

        Serial.println("[GbBridge] Teardown complete. All Game Boy resources freed.");
    }
}

bool GbBridge::isRunning() {
    return s_running;
}

bool GbBridge::isExitRequested() {
    return GbInput::isExitRequested();
}

void GbBridge::waitForExitRelease() {
    GbInput::waitForExitRelease();
}
