#ifndef CONFIG_H
#define CONFIG_H

// ── Wi-Fi Credentials ────────────────────────────────────────────────────────
#define WIFI_SSID     "Deep"
#define WIFI_PASSWORD "43211234"

// ── Spotify Bridge Settings ───────────────────────────────────────────────────
#define SPOTIFY_BRIDGE_HOST "10.240.13.7"
#define SPOTIFY_BRIDGE_PORT 8888
#define SPOTIFY_MOCK_MODE   true   // Set to false when connecting to live Python bridge

// ── Button / Joystick pins ───────────────────────────────────────────────────
#define BTN_ENTER 13            // Momentary push button (INPUT_PULLUP)
#define BTN_BACK  25            // Momentary push button (INPUT_PULLUP)
#define JOY_X     34            // Analog: VRx (ADC1_CH6)
#define JOY_Y     35            // Analog: VRy (ADC1_CH7)

// ── ST7735 pins (hardware SPI) ───────────────────────────────────────────────
#define TFT_CS    5             // Chip-select
#define TFT_DC    2             // Data/Command
#define TFT_RST   4             // Reset

#endif // CONFIG_H
