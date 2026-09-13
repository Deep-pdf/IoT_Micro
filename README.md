# IoT_Micro (DEADDEEP Handheld Console & Smart Companion)

An advanced, feature-rich handheld device built on the **ESP32** microcontroller and an **ST7735 128×160 SPI TFT** color display. It blends retro gaming, a dynamic daily companion OS, Spotify remote media controls, and a generative AI chat client into an ultra-compact form factor.

---

## 📸 System Overview

```
+-------------------------------------------------------------+
|                     IoT_Micro ESP32                         |
|                                                             |
|  [ ST7735 1.8" TFT (128x160) ]      [ Analog Joystick ]    |
|  [ ENTER Button (GPIO 13)    ]      [ BACK Button (GPIO 25)]|
+------------------------------+------------------------------+
                               | (Wi-Fi 2.4 GHz)
              +----------------+----------------+
              |                                 |
              v                                 v
   [ Spotify Bridge Server ]           [ Phone AI Bridge ]
   (Python Flask / Port 8888)          (Python Flask / Port 8080)
              |                                 |
              v                                 v
     [ Spotify Web API ]               [ Google Gemini API ]
```

### Key Highlights
- **Dual-Screen OS Launcher**: Intuitive navigation with dynamic focus borders, real-time Wi-Fi status, battery gauge, and SNTP live time/date.
- **Pre-compiled Quote Widget ("Maan ki Baat")**: Automatically cycles quotes hourly and provides a fullscreen reader with word wrapping and dynamic color themes.
- **Pixel Wars Game**: A complete retro space shooter featuring 3-layer parallax starfields, multi-behavior enemy formations, homing bombs, particle explosions, and high-score flash memory saving.
- **AI Chat Client**: Complete on-screen virtual keyboard with smart cursor physics that queries Google Gemini via a local lightweight Python proxy.
- **Spotify Remote Control**: Live track metadata, progress scrubbers, animated equalizer, play/pause/skip controls, and 48×48 RGB565 album art streaming.
- **Lost Crown Cinematic**: Rich pixel-art loading screen with multi-layer animated shaders (torch flames, river shimmer, eye pulse) and an interactive title menu.
- **Network Diagnostic / Audit Tool**: On-device 802.11 Wi-Fi scanner and BLE interface.

---

## 🛠 Hardware & Pin Configuration

| Component | ESP32 GPIO | Description / Role |
|---|---|---|
| **TFT CS** | `GPIO 5` | Display Chip Select (SPI) |
| **TFT DC** | `GPIO 2` | Display Data / Command Select |
| **TFT RST** | `GPIO 4` | Display Hardware Reset |
| **TFT MOSI / SDA** | `GPIO 23` | VSPI Master Out Slave In |
| **TFT SCLK / SCK** | `GPIO 18` | VSPI Serial Clock |
| **TFT VCC / GND** | `3.3V / GND` | Power Supply |
| **Joystick X (VRx)** | `GPIO 34` | Analog input (ADC1_CH6) |
| **Joystick Y (VRy)** | `GPIO 35` | Analog input (ADC1_CH7) |
| **Joystick Button (SW)** | `GPIO 32` | Physical switch input |
| **ENTER Button** | `GPIO 13` | Momentary push button (`INPUT_PULLUP`), debounced |
| **BACK Button** | `GPIO 25` | Momentary push button (`INPUT_PULLUP`), short/long press |

---

## 📂 Project Architecture

```
IoT_Micro/
├── platformio.ini              # PlatformIO environment config (huge_app partition)
├── generate_quotes.py          # Python pre-build script parsing Markdown quotes to C++
├── include/
│   ├── config.h                # User credentials (Wi-Fi, Spotify Bridge host, Mock mode)
│   ├── home_screen.h           # OS layout, status bar, and state machine declarations
│   ├── quote_library.h         # Auto-generated C++ array of quotes in PROGMEM
│   ├── icons.h                 # 32x32 RGB565 pixel art icons for the app dock & grid
│   └── button.h                # Hardware debouncing and press edge detection
├── src/
│   ├── main.cpp                # Core firmware lifecycle, boot animation, app dispatcher
│   ├── home_screen.cpp         # Main dashboard and Page 2 app launcher rendering
│   ├── button.cpp              # Non-blocking button routines
│   ├── ai/                     # AI Chat Application (Keyboard, HTTP connection, UI)
│   ├── spotify/                # Spotify UI, state engine, and HTTP REST client
│   ├── pixel_wars/             # Pixel Wars arcade game engine
│   ├── lost_crown/             # Lost Crown loading screen and title menu
│   └── hackk/                  # Network radio and diagnostic interface
├── spotify-bridge/             # Python OAuth 2.0 bridge server for Spotify
└── PhoneAI-bridge/             # Python bridge forwarding questions to Google Gemini
```

---

## 🚀 Applications & Features

### 1. OS Home Screen & App Launcher
- **Page 1 (Dashboard)**:
  - **Status Bar**: Live Wi-Fi indicator (green when connected, white when offline) and battery level.
  - **Clock & Date**: Automatically synchronized against NTP servers (`pool.ntp.org`, IST GMT+5:30 offset).
  - **Maan ki Baat Widget**: Displays a truncated motivational quote. Selecting it opens the fullscreen reader. Quotes rotate automatically every hour or upon exiting apps.
  - **Bottom Dock**: Quick-launch shortcuts for *Pixel Wars*, *Gemini AI*, and *Lost Crown*.
- **Page 2 (Apps Grid)**:
  - Navigating down past the bottom dock flips to Page 2.
  - 3×2 grid showing all system applications with dedicated pixel art icons.

### 2. Maan ki Baat (Quote Reader)
- Opens quotes in clean, distraction-free fullscreen typography.
- Uses dynamic word wrapping to fit the 128-pixel display width cleanly without splitting words.
- Randomizes between three distinct retro color themes (Midnight Black, High-Contrast Orange, and Paper White) each time it opens.
- Powered by an automated pipeline: edit `include/maan_ki_baat_quote_library.md`, and `generate_quotes.py` automatically compiles them into flash memory (`PROGMEM`) at build time.

### 3. Pixel Wars (Space Shooter)
- **Engine**: Custom fixed-update game loop optimized for the ST7735 display.
- **Parallax Starfield**: Three independent depth planes (far, mid, and near) with variable velocities, twinkling colors, and particle streaks.
- **Enemies**: 5 distinct movement behaviors (Straight, Drift, Sine Oscillate, Zig-Zag, and Step Formations).
- **Weapons & Powerups**: Rapid laser projectiles, enemy targeted bullets, descending bombs with flashing HUD warnings, and heart health recovery drops.
- **Persistence**: High scores persist across power cycles using the ESP32 Non-Volatile Storage (`Preferences` / NVS).

### 4. AI Chat Companion (Powered by Gemini)
- **Virtual QWERTY Keyboard**: Includes lowercase, uppercase (Shift toggle), numerals, punctuation, space, backspace, and submit.
- **Cursor Physics**: Joystick input snaps to the closest horizontal key center when changing rows.
- **Local Bridge**: Queries are sent via Wi-Fi HTTP POST to `PhoneAI-bridge`, which interfaces with Google's Gemini models and returns concise answers formatted for small screens.

### 5. Spotify Remote Control
- **Display**: Track name, artist, total duration, real-time elapsed playback position bar, and animated equalizer waves.
- **Album Artwork**: Automatically requests and renders 48×48 RGB565 album art fetched and downsampled by the Python bridge.
- **Controls**: Play/Pause, Next Track, and Previous Track mapped directly to joystick gestures and the ENTER button.
- **Mock Mode**: Built-in simulator mode allows full UI testing and animations even when the PC bridge is offline.

### 6. Lost Crown Cinematic Experience
- Displays custom 16-bit color fantasy artwork.
- Animated multi-layer shader effects: flickering torchlight, glowing villain eyes, and shimmering river reflections.
- Ornate 5-level vertical amber gradient progress bar before launching into the interactive title screen.

---

## ⚡ Getting Started

### Prerequisites
- [PlatformIO IDE](https://platformio.org/) (VS Code extension or CLI)
- Python 3.8+ (for running the bridge servers)
- ESP32 Development Board (`esp32dev`)

### 1. ESP32 Firmware Configuration
Open [include/config.h](file:///include/config.h) and set your Wi-Fi credentials and PC bridge IP address:

```cpp
// Wi-Fi Credentials
#define WIFI_SSID "Your_WiFi_Name"
#define WIFI_PASSWORD "Your_WiFi_Password"

// Spotify Bridge IP (find your PC IP with 'ipconfig' or 'ifconfig')
#define SPOTIFY_BRIDGE_HOST "192.168.1.100"
#define SPOTIFY_BRIDGE_PORT 8888
#define SPOTIFY_MOCK_MODE   false   // Set to true to test without bridge
```

### 2. Compiling and Flashing
Connect the ESP32 via USB and run:

```bash
# Build firmware (automatically generates quote headers)
pio run

# Flash to device
pio run --target upload

# Open serial monitor
pio run --target monitor
```

*Note: The project uses the `huge_app.csv` partition layout to accommodate rich graphics, fonts, and networking stacks.*

---

## 🌉 Companion Bridge Setup

### Spotify Bridge (`spotify-bridge/`)
1. Create a free developer app at the [Spotify Developer Dashboard](https://developer.spotify.com/dashboard).
2. Set the Redirect URI to `http://127.0.0.1:8888/callback`.
3. In `spotify-bridge/`, copy `.env.example` to `.env`:
   ```bash
   cd spotify-bridge
   pip install -r requirements.txt
   cp .env.example .env
   ```
4. Fill in `SPOTIFY_CLIENT_ID` and `SPOTIFY_CLIENT_SECRET`.
5. Start the server:
   ```bash
   python server.py
   ```
6. Open `http://127.0.0.1:8888/login` in your browser once to authorize your Spotify account.

### Phone AI Bridge (`PhoneAI-bridge/`)
1. Obtain a free API key from [Google AI Studio](https://aistudio.google.com/).
2. In `PhoneAI-bridge/`:
   ```bash
   cd PhoneAI-bridge
   pip install -r requirements.txt
   cp .env.example .env
   ```
3. Add your Gemini API key to `.env`:
   ```env
   GEMINI_API_KEY=your_gemini_api_key_here
   ```
4. Start the server:
   ```bash
   python server.py
   ```

---

## 🎮 Controls & Navigation

- **Joystick Left / Right / Up / Down**: Move cursor, navigate apps, and steer player ship.
- **ENTER Button (GPIO 13)**: Select focused app, fire weapons in Pixel Wars, type keys in AI Chat, and toggle Play/Pause in Spotify.
- **BACK Button (GPIO 25)**: Return to previous screen or exit to Home Screen.

---

## 📄 License
This project is open-source and intended for educational, maker, and IoT exploration purposes.
