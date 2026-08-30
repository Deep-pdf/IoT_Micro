# Spotify Bridge for ESP32 micro_IoT

A dedicated local bridge that connects the ESP32 `micro_IoT` remote control to your Spotify account using Spotify's official Web API and OAuth 2.0.

---

## Architecture

```
[ ESP32 Remote UI ] 
        ↕ (Local Wi-Fi HTTP requests)
[ Python Spotify Bridge (Port 8888) ]
        ↕ (OAuth 2.0 Web API)
[ Spotify Cloud API / Desktop Player ]
```

---

## Setup Guide

### 1. Install Dependencies

Navigate to the `spotify-bridge` directory and install the required Python packages:

```bash
cd spotify-bridge
pip install -r requirements.txt
```

### 2. Create Spotify Developer App & Get Credentials

1. Go to the [Spotify Developer Dashboard](https://developer.spotify.com/dashboard) and log in.
2. Click **Create App**:
   - **App Name**: `ESP32 Spotify Remote` (or any name you like)
   - **App Description**: `Remote control bridge for ESP32`
   - **Redirect URIs**: Enter `http://127.0.0.1:8888/callback` (must match exactly, do not use localhost).
   - Which API/SDKs are you planning to use: Check **Web API**.
3. Save the app and go to **Settings**:
   - Copy the **Client ID**.
   - Click **View client secret** and copy the **Client Secret**.

### 3. Create and Configure `.env`

Copy `.env.example` to `.env`:

```bash
cp .env.example .env
```
*(On Windows PowerShell: `Copy-Item .env.example .env`)*

Open `.env` and paste your Spotify credentials:

```env
SPOTIFY_CLIENT_ID=your_actual_client_id_here
SPOTIFY_CLIENT_SECRET=your_actual_client_secret_here
SPOTIFY_REDIRECT_URI=http://127.0.0.1:8888/callback
SPOTIFY_BRIDGE_HOST=0.0.0.0
SPOTIFY_BRIDGE_PORT=8888
```

### 4. Start the Bridge Server

Run the server:

```bash
python server.py
```

### 5. Authorize with Spotify

1. Open your web browser and navigate to:
   ```
   http://127.0.0.1:8888/login
   ```
2. Log in with your Spotify account and click **Agree**.
3. Once redirected, you will see the **Spotify Authorization Successful** confirmation.
4. The access and refresh tokens will be stored automatically in `tokens.json` (which is gitignored).

---

## Testing Endpoints

Open Spotify on your computer or phone and start playing any track. Then test these endpoints in your browser or with curl:

- **Get Playback State**:
  ```bash
  curl http://127.0.0.1:8888/spotify/state
  ```
- **Toggle Play / Pause**:
  ```bash
  curl -X POST http://127.0.0.1:8888/spotify/playpause
  ```
- **Skip to Next Track**:
  ```bash
  curl -X POST http://127.0.0.1:8888/spotify/next
  ```
- **Previous Track**:
  ```bash
  curl -X POST http://127.0.0.1:8888/spotify/previous
  ```
- **Bridge Dashboard**:
  Visit `http://127.0.0.1:8888/` in your browser for an interactive web dashboard.

---

## ESP32 Configuration

In your ESP32 project file [include/config.h](file:///include/config.h):

### Find your PC's Local IP Address
- On Windows: Run `ipconfig` in Command Prompt/PowerShell and look for `IPv4 Address` (e.g. `192.168.1.100` or `10.240.x.x`).

### Configure `config.h`
```cpp
// Spotify Bridge Configuration
#define SPOTIFY_BRIDGE_HOST "192.168.1.100"  // Set to your computer's local Wi-Fi IP
#define SPOTIFY_BRIDGE_PORT 8888

// Set to false when ready to connect to the live Python bridge
// Set to true to test with simulated tracks without running the bridge
#define SPOTIFY_MOCK_MODE false
```

### How to Toggle Mock Mode
- `SPOTIFY_MOCK_MODE true`: Runs simulated track playback with animated equalizer waves and allows joystick navigation testing even if PC is offline.
- `SPOTIFY_MOCK_MODE false`: Connects live over Wi-Fi to your PC bridge and syncs real-time Spotify tracks and controls.
