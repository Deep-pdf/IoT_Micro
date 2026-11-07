# IoT_Micro — Master Firmware RAM & Flash Memory Audit

## Executive Summary

A comprehensive, project-wide RAM, flash memory, and storage architecture audit was performed across the entire **`IoT_Micro`** handheld firmware repository for the **ESP32-D0WD-V3** (Revision 3.1).

This audit covers **every single directory, module, application, game, background task, dependency, embedded asset, and third-party library** in the project.

### Core Discoveries at a Glance

1. **Flash Space (`app0`) is 87.6% Full**:
   - Out of the 3,145,728-byte (3.0 MB) application partition, the firmware binary currently consumes **2,754,960 bytes (2.63 MB)**, leaving only **390,768 bytes (~381.6 KB, 12.4%)** for future code and games.
   - **Top Flash Consumers**: Retro Arcade NES (548 KB, 21.1%), Lost Crown (496 KB, 19.1%), Retro Arcade Game Boy (491 KB, 18.9%), and the Wi-Fi/LwIP networking stack (361 KB, 13.9%).
2. **~140 KB of Flash is Wasted by Header Asset Duplications**:
   - `LostCrownGameAssets.h` and `LostCrownAssets.h` declare arrays as `static const uint16_t PROGMEM`. Because they are `#include`d across multiple `.cpp` files, the compiler emits multiple identical copies of pixel arrays in flash, wasting **~135 KB**.
   - `Dream_Orphans_Bd6pt7b.h` is included in 4 translation units, creating **4 duplicate copies** in flash (3,972 B duplicate).
3. **Static DRAM is Healthy at 25.0%, but ~20 KB is Unnecessarily Held Inactive**:
   - Static DRAM (`.dram0.data` + `.bss`) consumes **81,896 bytes** out of ~320 KB usable internal DRAM.
   - However, **10,672 bytes** are permanently consumed by two static render buffers in `LostCrownGameplay.cpp` (`stripBuffer` and `spriteCompositeBuffer`), **4,608 bytes** by Spotify's `artworkBuffer`, **4,532 bytes** by unreleased Bluetooth controller memory, and **6,303 bytes** by NES display structures—even when those apps are completely inactive!
4. **Dynamic Heap is Perfectly Managed in Retro Arcade, but Constrained Under Peak Load**:
   - The baseline free heap before launching Game Boy emulation is **161,040 bytes**.
   - During active Pokémon Red gameplay, heap drops to **58,660 bytes** (a 102 KB dynamic allocation pool).
   - Upon exit, **100% of Game Boy memory is cleanly reclaimed** with zero residual leakage.
5. **Onboard Filesystem (LittleFS) is Verified**:
   - The 896 KB `spiffs` partition contains the active 32,768-byte cartridge save `/pokemon_red.sav`, leaving **~868 KB of free non-volatile storage**.

---

## 2. Master Feature & Subsystem Inventory

| Module / Feature | Status | Source Directory | Primary Purpose | Flash Footprint | Static RAM | Dynamic Heap Peak |
|---|---|---|---|---:|---:|---:|
| **System OS & Launcher** | **Active** | `src/` | Home screen, apps grid, status bar, joystick & button input | 34,820 B | 230 B | 0 B |
| **Maan Ki Baat Quotes** | **Active** | `include/quote_library.h` | 27 Shayari quotes, random themes, fullscreen viewer | ~4,715 B | 0 B | 0 B |
| **Pixel Wars Game** | **Active** | `src/pixel_wars/` | Custom retro arcade space shooter engine | 25,184 B | 3,182 B | 0 B |
| **Lost Crown Game** | **Active** | `src/lost_crown/` | Action-platformer engine (Loading, Title, Training, Level 1) | 496,028 B | 11,543 B | 40,960 B |
| **Retro Arcade (NES)** | **Active** | `src/retro/nes/` | NES emulator: Super Mario Bros. 3 (393 KB) & Contra (139 KB) | 548,377 B | 7,905 B | ~24,576 B |
| **Retro Arcade (Game Boy)**| **Active** | `src/retro/gb/` | Gnuboy emulator: Pokémon Red (456 KB LZ4 ROM), LittleFS save | 490,964 B | 482 B | ~120,000 B |
| **Retro Arcade (Launcher)**| **Active** | `src/retro/` | Game selector carousel, cover art thumbnails | 71,293 B | 317 B | ~512 B |
| **Spotify Remote App** | **Active** | `src/spotify/` | WebSockets/HTTP client for Spotify companion bridge | 18,791 B | 4,847 B | ~16,000 B |
| **AI Assistant (Gemini)** | **Active** | `src/ai/` | Virtual keyboard & REST client for PhoneAI companion bridge | 8,003 B | 1,678 B | ~8,192 B |
| **Hackk Security Suite** | **Active** | `src/hackk/` | Wi-Fi pentest suite (Deauth, Evil Twin, packet sniffer) | 8,615 B | 2,932 B | ~2,048 B |
| **Calculator App** | **Stub** | `src/main.cpp` | Placeholder menu action | ~120 B | 0 B | 0 B |
| **Status LED Service** | **Active** | `src/status_led.cpp` | FreeRTOS breathing background task (GPIO 22) | 1,180 B | 16 B | 2,048 B (Stack) |
| **Wi-Fi & Network Stack** | **Active** | IDF / Framework | Net80211, LwIP, WPA Supplicant, mbedTLS | 361,264 B | 8,457 B | ~45,000 B |
| **ESP-IDF Core & Drivers** | **Active** | IDF / Framework | Drivers (SPI, GPIO, UART), VFS, NVS, LittleFS, Core Dump | 210,099 B | 17,122 B | ~10,000 B |
| **C/C++ Standard Runtime** | **Active** | Toolchain LibC | Newlib LibC, LibM, LibStdC++, LibGCC | 259,390 B | 4,477 B | ~4,096 B |
| **Adafruit GFX / ST7735** | **Active** | PlatformIO Libs | ST7735 128×160 display driver & drawing primitives | 12,630 B | 64 B | 0 B |
| **spotify-bridge** | **Standalone** | `spotify-bridge/` | Node.js/Python Spotify OAuth companion server | 0 B (Repo only) | 0 B | 0 B |
| **PhoneAI-bridge** | **Standalone** | `PhoneAI-bridge/` | Python Gemini API proxy companion server | 0 B (Repo only) | 0 B | 0 B |
| **Anemoia-ESP32-main** | **Reference** | `Anemoia-ESP32-main/` | Upstream NES emulator reference clone | 0 B (Repo only) | 0 B | 0 B |
| **retro-go-gmt024-s3** | **Reference** | `retro-go-gmt024.../`| Upstream Gnuboy emulator reference clone | 0 B (Repo only) | 0 B | 0 B |

---

## 3. Dedicated Diagnostic Reports

Detailed findings, symbol traces, and data dumps are documented in the companion audit reports:

1. [IOT_MICRO_FLASH_MAP.md](file:///c:/Users/Deep/Documents/Test%20Files/IOT/IoT_Micro/reports/memory_audit/IOT_MICRO_FLASH_MAP.md): Complete partition boundaries, ELF section mappings, top 30 flash objects, asset inventory, and duplication analysis.
2. [IOT_MICRO_RAM_MAP.md](file:///c:/Users/Deep/Documents/Test%20Files/IOT/IoT_Micro/reports/memory_audit/IOT_MICRO_RAM_MAP.md): Physical SRAM architecture, static DRAM breakdown, dynamic heap sites, FreeRTOS stacks, and runtime telemetry.
3. [IOT_MICRO_MODULE_BREAKDOWN.md](file:///c:/Users/Deep/Documents/Test%20Files/IOT/IoT_Micro/reports/memory_audit/IOT_MICRO_MODULE_BREAKDOWN.md): Deep-dive into each subsystem, source file ownership, memory lifecycles, and shared resource analysis.
4. [IOT_MICRO_OPTIMIZATION_PLAN.md](file:///c:/Users/Deep/Documents/Test%20Files/IOT/IoT_Micro/reports/memory_audit/IOT_MICRO_OPTIMIZATION_PLAN.md): Ranked, evidence-based optimization recommendations with risk/difficulty ratings and exact savings projections.

---

## 4. Final Summary Dashboard

| Metric | Measured Result | Audit Assessment |
|---|---|---|
| **Physical Flash Capacity** | **4,194,304 bytes (4.0 MB)** | 100% mapped across bootloader, NVS, app0, LittleFS, coredump |
| **Firmware Binary Size** | **2,754,960 bytes (2,690.4 KB)** | Linked from 69 object files, 15 libraries, and 3 embedded ROMs |
| **Application Partition Remaining** | **390,768 bytes (381.6 KB, 12.4%)** | Sufficient for small updates; constrained for additional ROMs |
| **Filesystem Total / Used / Free** | **917,504 B (896 KB) / ~48 KB / ~869 KB** | 869 KB free on LittleFS; `/pokemon_red.sav` is 32,768 B |
| **Static DRAM Usage** | **81,896 bytes (25.0%)** | 30,240 B `.data` + 51,656 B `.bss` (Healthy baseline) |
| **IRAM Usage** | **93,054 bytes (71.0%)** | 92,027 B code + 1,027 B vectors (38 KB headroom remaining) |
| **Boot Free Heap** | **~245 KB (Theoretical) / 161,040 B (Init)** | 161,040 B measured after OS & Wi-Fi subsystems initialize |
| **Minimum Observed Free Heap** | **58,636 bytes** | Measured during Pokémon Red active emulation |
| **Largest Free Heap Block** | **~85,000 bytes** (Idle) / **~38,000 bytes** (Emu) | Healthy contiguous block; zero fatal fragmentation observed |
| **Number of Discovered Modules** | **20 modules & subsystems** | 12 active firmware subsystems + 2 companion bridges + 2 upstream repos |
| **Number of Linked Modules** | **12 firmware subsystems (69 source files)** | All 69 source files in `src/` are compiled and linked |
| **Largest Flash Consumer** | **Retro Arcade (NES): 548,377 bytes (21.1%)** | Dominated by Mario (393 KB) and Contra (139 KB) ROMs |
| **Largest Static RAM Consumer** | **Lost Crown Game: 11,543 bytes (14.1%)** | Dominated by `stripBuffer` and `spriteCompositeBuffer` (10.7 KB) |
| **Largest Runtime Heap Consumer**| **Retro Arcade (Game Boy): ~120 KB peak** | 64 KB ROM cache + 32 KB Cart RAM + 16 KB VRAM + 24 KB Framebuffer |
| **Estimated Safe Flash Savings** | **~143 KB – 280 KB** | By eliminating header asset duplications & font de-duplication |
| **Estimated Safe RAM Savings** | **~28.7 KB – 33.1 KB Static DRAM** | By lazy-allocating inactive game buffers & releasing Bluetooth memory |

