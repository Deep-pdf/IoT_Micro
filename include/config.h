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

#endif // CONFIG_H
