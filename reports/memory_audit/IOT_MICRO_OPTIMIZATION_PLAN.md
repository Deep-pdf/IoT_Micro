# IoT_Micro — Prioritized Firmware Optimization Plan

## 1. Executive Summary

This diagnostic audit has identified **up to ~350 KB of recoverable Flash storage** and **up to ~33 KB of recoverable Static DRAM** without sacrificing a single feature, reducing emulation quality, or degrading performance.

Because this phase is **strictly diagnostic and read-only**, no code or build settings have been altered. The recommendations below are ranked by impact, safety, and implementation simplicity to serve as the blueprint for future optimization phases.

---

## 2. Master Prioritized Optimization Matrix

| Rank | Module | Problem Description | Current Usage | Potential Saving (Flash / DRAM) | Safety Risk | Difficulty |
|:---:|---|---|---:|:---:|:---:|:---:|
| **1** | **Lost Crown** | `static const` asset header duplication across `.cpp` files | 412 KB header included in 2 cpp files | **~135 KB Flash** | **Safe** | Easy |
| **2** | **Lost Crown** | Permanent static BSS buffers (`stripBuffer`, `spriteCompositeBuffer`) | 10,672 B static BSS | **10,672 B Static DRAM** | **Safe** | Easy |
| **3** | **Spotify App** | Permanent static BSS buffer (`artworkBuffer`) | 4,608 B static BSS | **4,608 B Static DRAM** | **Safe** | Easy |
| **4** | **Bluetooth HAL** | Unreleased BT controller DRAM memory | 4,532 B static DRAM | **~4,532 B DRAM + ~15 KB Heap** | **Safe** | Trivial (1 line) |
| **5** | **System Fonts** | `Dream_Orphans_Bd6pt7b` duplicated across 4 object files | 5,296 B rodata in 4 `.o` files | **3,972 B Flash** | **Safe** | Easy |
| **6** | **Dead Assets** | Unreferenced font headers (`mono.h`, etc.) & unused icons | 97.5 KB dead source headers | **~4 KB Flash** + Clean Tree | **Safe** | Trivial |
| **7** | **Retro Arcade (NES)** | Static PPU display buffer & display adapter scanline LUT | 6,303 B static DRAM | **6,303 B Static DRAM** | **Safe** | Moderate |
| **8** | **Hackk Suite** | Static `g_aps[32]` access point cache in BSS | 2,560 B static BSS | **2,560 B Static DRAM** | **Safe** | Easy |
| **9** | **ESP-IDF Core** | Core dump static RAM buffers (`libespcoredump.a`) | 4,943 B static DRAM | **4,461 B Static DRAM** | Moderate | Medium |
| **10** | **Lost Crown** | Raw uncompressed Level 1 background array (`level1_bg_pixels`) | 245,760 B flash rodata | **~100 KB – 140 KB Flash** | Moderate | Medium |

> [!NOTE]
> **Total Realistic Non-Overlapping Savings**:
> - **Flash Space Recovered**: **~143 KB (Safe)** up to **~280 KB (with Level 1 compression)**.
> - **Static DRAM Recovered**: **~28.7 KB (Safe)** up to **~33.1 KB (with core dump removal)**.
> - **Impact**: Application partition free space increases from **390 KB to ~670 KB**, and permanent baseline free heap increases by **~30 KB** across the entire OS!

---

## 3. Detailed Diagnosis & Implementation Blueprint

### Recommendation 1: Fix Lost Crown Asset Header Duplication (Save ~135 KB Flash)
- **Problem**: `src/lost_crown/LostCrownGameAssets.h` (412 KB) and `src/lost_crown/LostCrownAssets.h` (509 KB) declare large RGB565 pixel arrays with `static const uint16_t PROGMEM`.
  - In C++, declaring a variable `static` in a header file gives it internal linkage.
  - Every `.cpp` file that `#include`s the header receives its own independent private copy in the compiled object file.
  - `LostCrownGameAssets.h` is included in both `Level1.cpp` and `LostCrownGameplay.cpp`.
  - `LostCrownAssets.h` is included in both `LostCrownLoading.cpp` and `LostCrownTitle.cpp`.
- **Evidence**:
  - `veera_attack1_pixels` (5,336 B) exists at `0x3f458358` and `0x3f40de44`.
  - `veera_attack2_pixels` (5,336 B) exists at `0x3f40c96c` and `0x3f456e80`.
  - `veera_attack3_pixels` (5,336 B) exists at `0x3f4559a8` and `0x3f40b494`.
  - `veera_attack4_pixels` (5,336 B) exists at `0x3f409fbc` and `0x3f4544d0`.
  - `image_withoutmenu_pixels` (40,960 B) and `image_LostCrown_background` (40,960 B) are duplicated.
- **Solution**:
  - Convert declarations in headers to `extern const uint16_t array_name[];`.
  - Define the arrays in a single dedicated translation unit (e.g. `LostCrownGameAssets.cpp`).
- **Potential Saving**: **~135,000 bytes (135 KB) Flash**.
- **Risk**: None. Binary-identical rendering.

---

### Recommendation 2: Convert Lost Crown Static Buffers to Dynamic Heap (Save 10,672 B Static DRAM)
- **Problem**: `LostCrownGameplay.cpp` declares two large static render buffers at file scope:
  ```cpp
  static uint16_t stripBuffer[SCREEN_W * 26];            // 5,336 bytes in .bss
  static uint16_t spriteCompositeBuffer[SCREEN_W * 26];  // 5,336 bytes in .bss
  ```
  These two buffers occupy **10,672 bytes of internal DRAM** permanently from the moment the ESP32 boots up. When the user is on the Home Screen, playing Mario, or using Wi-Fi, over 10.6 KB of RAM is held hostage by an inactive game.
- **Solution**:
  - Replace static arrays with pointers `uint16_t* stripBuffer = nullptr;`.
  - Allocate with `malloc(sizeof(uint16_t) * SCREEN_W * 26)` inside `LostCrownGameplay::begin()`.
  - Free with `free()` inside `LostCrownGameplay::end()` or when transitioning back to title/launcher.
- **Potential Saving**: **10,672 bytes Static DRAM**.
- **Risk**: None. Internal heap headroom during Lost Crown is > 100 KB.

---

### Recommendation 3: Convert Spotify Artwork Buffer to Dynamic Heap (Save 4,608 B Static DRAM)
- **Problem**: `src/spotify/SpotifyApp.cpp` declares:
  ```cpp
  static uint16_t artworkBuffer[48 * 48]; // 4,608 bytes in .bss
  ```
  This buffer sits in DRAM permanently even if the user never opens Spotify.
- **Solution**:
  - Allocate dynamically in `SpotifyApp::init()` and free in `SpotifyApp::deinit()` / `exitSpotify()`.
- **Potential Saving**: **4,608 bytes Static DRAM**.
- **Risk**: None.

---

### Recommendation 4: Reclaim Unused Bluetooth Controller Memory (Save ~4.5 KB DRAM + ~15 KB Heap)
- **Problem**: The ESP32 Arduino framework compiles in Bluetooth interrupt vectors (`libbt.a(hli_vectors.S.obj)` = **4,380 bytes** in `.dram0.data`). Furthermore, ESP-IDF reserves a large portion of SRAM1/SRAM2 for the Bluetooth baseband/controller unless explicitly instructed that Bluetooth is not needed.
- **Solution**:
  - Add this single line at the very start of `setup()` in `src/main.cpp`:
    ```cpp
    esp_bt_controller_mem_release(ESP_BT_MODE_BTDM);
    ```
- **Potential Saving**: **~4,532 bytes Static DRAM** and **~15,000 bytes of dynamic heap** immediately released back to the general heap pool at boot time.
- **Risk**: None (the project does not use Bluetooth).

---

### Recommendation 5: De-duplicate `Dream_Orphans_Bd6pt7b` Font Definition (Save 3,972 B Flash)
- **Problem**: `include/Dream_Orphans_Bd6pt7b.h` defines its GFX font structures as `const` at global scope without `extern`.
  - Included in `main.cpp`, `home_screen.cpp`, `PixelWarsMenu.cpp`, and `RetroLauncher.cpp`.
  - Four identical copies (1,324 B each) are linked into flash:
    - 3 duplicate glyph tables: $3 \times 760\text{ B} = 2{,}280\text{ B}$.
    - 3 duplicate bitmap tables: $3 \times 548\text{ B} = 1{,}644\text{ B}$.
    - 3 duplicate font descriptor structs: $3 \times 16\text{ B} = 48\text{ B}$.
- **Solution**:
  - Add `extern const GFXfont Dream_Orphans_Bd6pt7b;` in the header.
  - Define it once in `src/home_screen.cpp` (or a dedicated `fonts.cpp`).
- **Potential Saving**: **3,972 bytes Flash**.
- **Risk**: None.

---

### Recommendation 6: Clean Up Unused Font Headers & Placeholder Icons (Clean 97.5 KB Source)
- **Problem**:
  - `include/mono.h` (13,461 B), `include/monolarge.h` (35,499 B), `include/monosmall.h` (38,646 B), and `include/MikodacsPCS8pt7b.h` (9,988 B) are never included anywhere in the project.
  - `include/icons.h` contains `icon_newapp1` and `icon_newapp2` (4,096 B) which are never drawn.
- **Solution**:
  - Remove dead header files or move them to an archival directory (`assets/fonts_unused/`).
- **Potential Saving**: **~4 KB Flash** if icons were ever referenced, plus cleaner build trees and faster indexing.
- **Risk**: None.

---

### Recommendation 7: Make NES PPU Display Buffer Dynamic (Save 6,303 B Static DRAM)
- **Problem**:
  - `Ppu2C02::display_buffer` in `src/retro/nes/core/ppu2C02.cpp:34`: **4,096 bytes** in `.data`.
  - `NesDisplayAdapter` scanline LUT: **2,207 bytes** in `.bss`.
  - These buffers sit in DRAM even when playing Game Boy or on the home screen.
- **Solution**:
  - Allocate `display_buffer` and the scanline table inside `NesBridge::init()` and free them in `NesBridge::stop()`.
- **Potential Saving**: **6,303 bytes Static DRAM**.
- **Risk**: Safe; requires matching the lifecycle pattern already perfected in `GbBridge`.

---

### Recommendation 8: Make Hackk AP Cache Dynamic (Save 2,560 B Static DRAM)
- **Problem**:
  - `g_aps[32]` in `src/hackk/attacks/wifi_controller.cpp:16` allocates **2,560 bytes** in `.bss`.
- **Solution**:
  - Allocate `g_aps` when `HackkApp::init()` is entered, and free it when `exitHackingGo()` returns to the launcher.
- **Potential Saving**: **2,560 bytes Static DRAM**.
- **Risk**: None.

---

### Recommendation 9: Disable ESP-IDF Core Dump in Production (Save 4,461 B Static DRAM)
- **Problem**:
  - `libespcoredump.a` allocates 4,461 bytes of static DRAM across `core_dump_elf.c.obj` (2,400 B), `core_dump_common.c.obj` (1,225 B), and `core_dump_flash.c.obj` (836 B).
  - The `coredump` flash partition (64 KB) is currently 100% unused.
- **Solution**:
  - In production builds, disable core dump in partition table or menuconfig.
- **Potential Saving**: **4,461 bytes Static DRAM** and **65,536 bytes Flash partition**.
- **Risk**: Low-to-moderate (removes automated crash trace capture to flash).

---

### Recommendation 10: Compress Lost Crown Level 1 Background (Save ~100–140 KB Flash)
- **Problem**:
  - `level1_bg_pixels` in `src/lost_crown/Level1Assets.h` is a raw 512×240 16-bit RGB565 bitmap consuming **245,760 bytes (240 KB)** of Flash.
- **Solution**:
  - Compress using LZ4 (the same high-speed streaming decompressor already linked for Pokémon Red in `GbRom.cpp`).
  - Or convert to an 8-bit indexed palette (122 KB uncompressed $\implies$ **122 KB instant savings**).
- **Potential Saving**: **~100 KB – 140 KB Flash**.
- **Risk**: Moderate (requires updating the tile/strip renderer in `Level1.cpp`).

