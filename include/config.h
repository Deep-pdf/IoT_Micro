#ifndef CONFIG_H
#define CONFIG_H

// -- Wi-Fi Credentials --------------------------------------------------------
#define WIFI_SSID     "Deep"
#define WIFI_PASSWORD "43211234"

// -- PC / Server Bridge IP Configuration --------------------------------------
// When running companion bridge servers on your PC/laptop, update PC_BRIDGE_HOST
// to your PC's current local Wi-Fi IPv4 address (found via `ipconfig` on Windows).
// Both Spotify Bridge and Phone AI Bridge inherit this host automatically.
#define PC_BRIDGE_HOST        "10.26.243.234"

// -- Spotify Bridge Settings --------------------------------------------------
#define SPOTIFY_BRIDGE_HOST   PC_BRIDGE_HOST
#define SPOTIFY_BRIDGE_PORT   8888
#define SPOTIFY_MOCK_MODE     false  // Set to true when testing without a live Python bridge

// -- Phone AI (Gemini) Bridge Settings ----------------------------------------
#define PHONE_AI_BRIDGE_HOST  PC_BRIDGE_HOST
#define PHONE_AI_BRIDGE_PORT  8080

// -- Button / Joystick pins & Universal Reader --------------------------------
#define BTN_ENTER 13            // Momentary push button (INPUT_PULLUP)
#define BTN_BACK  25            // Momentary push button (INPUT_PULLUP)
#define JOY_X     34            // Analog: VRx (ADC1_CH6)
#define JOY_Y     35            // Analog: VRy (ADC1_CH7)
#include "joystick.h"

// -- ST7735 pins (hardware SPI) -----------------------------------------------
#define TFT_CS    5             // Chip-select
#define TFT_DC    2             // Data/Command
#define TFT_RST   4             // Reset

// -- Feature Flags ------------------------------------------------------------
// 0 = Lost Crown temporarily disabled (frees ~496 KB flash, ~11.5 KB DRAM)
// 1 = Lost Crown fully enabled and restored in launcher grid & lifecycle
#define ENABLE_LOST_CROWN 0

// -- Retro Game Boy Hardware Test Mode ----------------------------------------
// (Hardware test mode retired - Pokémon Red integrated into Retro Arcade)

// -- Game Boy Display Palette Mode --------------------------------------------
// 0 = Original Pokémon Red GBC Colorization (red/sepia tint)
// 1 = High-Contrast Game Boy Pocket Monochrome (crisp B&W)
// 2 = Classic DMG Retro Green
#define GB_DISPLAY_DEFAULT_PALETTE 0

// -- Game Boy Persistent Save Configuration -----------------------------------
// Safe First-Time Initialization: detects virgin (100% 0xFF) partition and formats exactly once.
// Preserves existing filesystems and never blindly erases user data on mount failure.
#define GB_SAVE_SAFE_FIRST_TIME_INIT true

#endif // CONFIG_H
