#include "SpotifyUI.h"
#include <math.h>

// Color Palette (RGB565)
#define COL_BG          0x0000  // Pure Black (#000000)
#define COL_CARD_BG     0x0841  // Very dark charcoal (#0a0a0a)
#define COL_GREEN       0x1DB2  // Spotify Green (#1DB954)
#define COL_WHITE       0xFFFF  // Crisp White (#FFFFFF)
#define COL_GRAY_TEXT   0x94B2  // Medium Gray (#909090)
#define COL_TRACK_BG    0x2104  // Dark Track Gray (#222222)
#define COL_RED         0xF800  // Red for Offline Warning

// Cached tracking for dirty partial updates
static String   lastRenderedTitle = "";
static String   lastRenderedArtist = "";
static bool     lastRenderedPlaying = false;
static bool     lastRenderedConnected = true;
static bool     lastRenderedHasArtwork = false;
static uint32_t lastRenderedProgSec = 0xFFFFFFFF;
static uint32_t lastRenderedDurSec = 0xFFFFFFFF;
static SpotifyControlSelection lastRenderedSelection = (SpotifyControlSelection)99;
static unsigned long lastWaveMillis = 0;

// Pixel Bitmaps
static const uint8_t wifi_icon_6x6[] PROGMEM = {
    0b11111100,
    0b00000000,
    0b01111000,
    0b00000000,
    0b00110000,
    0b00110000
};

static const uint8_t heart_icon_9x8[] PROGMEM = {
    0b01100110,
    0b11111111,
    0b11111111,
    0b11111111,
    0b01111110,
    0b00111100,
    0b00011000,
    0b00000000
};

static const uint8_t shuffle_icon_10x8[] PROGMEM = {
    0b10000001,
    0b01000010,
    0b00100100,
    0b00011000,
    0b00011000,
    0b00100100,
    0b01000010,
    0b10000001
};

static const uint8_t repeat_icon_10x8[] PROGMEM = {
    0b00111110,
    0b01000001,
    0b11100001,
    0b00000000,
    0b00000000,
    0b10000111,
    0b10000010,
    0b01111100
};

void SpotifyUI::init(Adafruit_ST7735 &tft) {
    tft.setRotation(0);
    tft.fillScreen(COL_BG);

    lastRenderedTitle = "\t";
    lastRenderedArtist = "\t";
    lastRenderedPlaying = false;
    lastRenderedConnected = false;
    lastRenderedHasArtwork = false;
    lastRenderedProgSec = 0xFFFFFFFF;
    lastRenderedDurSec = 0xFFFFFFFF;
    lastRenderedSelection = (SpotifyControlSelection)99;
    lastWaveMillis = 0;
}

void SpotifyUI::formatTime(uint32_t ms, char *outBuffer, size_t bufSize) {
    uint32_t totalSec = ms / 1000;
    uint32_t minutes = totalSec / 60;
    uint32_t seconds = totalSec % 60;
    snprintf(outBuffer, bufSize, "%02u:%02u", (unsigned int)minutes, (unsigned int)seconds);
}

static void drawCenteredText(Adafruit_ST7735 &tft, const String &text, int16_t y, uint16_t color, int maxChars = 19) {
    tft.setFont(NULL);
    tft.setTextSize(1);
    tft.setTextColor(color, COL_BG);

    String display = text;
    if ((int)display.length() > maxChars) {
        display = display.substring(0, maxChars - 2) + "..";
    }

    int16_t textW = (int16_t)display.length() * 6;
    int16_t drawX = (128 - textW) / 2;
    if (drawX < 2) drawX = 2;

    tft.setCursor(drawX, y);
    tft.print(display);
}

void SpotifyUI::drawStatusBar(Adafruit_ST7735 &tft, bool isConnected) {
    tft.fillRect(0, 0, 128, 9, COL_BG);

    // Wi-Fi Icon
    uint16_t wifiColor = isConnected ? COL_WHITE : COL_RED;
    tft.drawBitmap(92, 1, wifi_icon_6x6, 6, 6, wifiColor);

    // Battery Icon (Outline + Bar)
    tft.drawRect(102, 1, 11, 6, COL_WHITE);
    tft.drawFastVLine(113, 2, 4, COL_WHITE);
    tft.fillRect(104, 3, 3, 2, COL_WHITE);

    // "18%" Text
    tft.setFont(NULL);
    tft.setTextSize(1);
    tft.setTextColor(COL_WHITE, COL_BG);
    tft.setCursor(117, 1);
    tft.print("%");
}

void SpotifyUI::drawSpotifyHeader(Adafruit_ST7735 &tft) {
    tft.fillRect(0, 10, 128, 15, COL_BG);

    // Left Spotify Circular Logo
    int16_t logoX = 12;
    int16_t logoY = 17;
    tft.fillCircle(logoX, logoY, 6, COL_GREEN);
    tft.drawCircle(logoX - 1, logoY + 4, 6, COL_BG);
    tft.drawCircle(logoX - 1, logoY + 2, 4, COL_BG);
    tft.drawCircle(logoX - 1, logoY,     2, COL_BG);

    // Centered "SPOTIFY"
    tft.setFont(NULL);
    tft.setTextSize(1);
    tft.setTextColor(COL_WHITE, COL_BG);
    tft.setCursor(43, 14);
    tft.print("SPOTIFY");

    // Right 3 Vertical Dots Indicator
    tft.drawPixel(120, 13, COL_WHITE);
    tft.drawPixel(120, 17, COL_WHITE);
    tft.drawPixel(120, 21, COL_WHITE);
}

void SpotifyUI::drawArtwork(Adafruit_ST7735 &tft, const uint16_t *artworkBuffer, bool hasArtwork, bool isPlaying) {
    int16_t boxX = 40;
    int16_t boxY = 27;
    int16_t boxSize = 48;

    // Thin green frame around the artwork
    tft.drawRect(boxX - 1, boxY - 1, boxSize + 2, boxSize + 2, COL_GREEN);

    if (hasArtwork && artworkBuffer != nullptr) {
        // Render real 48x48 album artwork
        tft.drawRGBBitmap(boxX, boxY, (uint16_t*)artworkBuffer, boxSize, boxSize);
    } else {
        // Retro Vinyl Artwork Placeholder
        tft.fillRect(boxX, boxY, boxSize, boxSize, COL_CARD_BG);

        int16_t centerX = boxX + boxSize / 2; // 64
        int16_t centerY = boxY + boxSize / 2; // 51

        tft.fillCircle(centerX, centerY, 17, 0x18C3);
        tft.drawCircle(centerX, centerY, 17, 0x3186);
        tft.drawCircle(centerX, centerY, 12, 0x2104);
        tft.drawCircle(centerX, centerY, 7,  0x2945);

        tft.fillCircle(centerX, centerY, 4, isPlaying ? COL_GREEN : COL_GRAY_TEXT);
        tft.fillCircle(centerX, centerY, 1, COL_BG);
    }
}

void SpotifyUI::drawSideEqualizers(Adafruit_ST7735 &tft, bool isPlaying) {
    const int numBars = 8;
    const int baseY = 75;
    const int maxHeight = 46;
    unsigned long now = millis();

    // Partial erase only the two 33px equalizer columns (Zero screen flicker)
    tft.fillRect(4, baseY - maxHeight, 33, maxHeight + 2, COL_BG);
    tft.fillRect(91, baseY - maxHeight, 33, maxHeight + 2, COL_BG);

    for (int i = 0; i < numBars; i++) {
        int leftX = 5 + i * 4;
        int rightX = 92 + (numBars - 1 - i) * 4;

        int barHeight = 4;
        if (isPlaying) {
            float phase1 = (now / 120.0f) + (i * 0.9f);
            float phase2 = (now / 240.0f) + (i * 1.7f);
            float wave = (sin(phase1) + 1.0f) * 0.5f * 24.0f + (cos(phase2) + 1.0f) * 0.5f * 18.0f;
            barHeight = 4 + (int)wave;
            if (barHeight > maxHeight) barHeight = maxHeight;
            if (barHeight < 4) barHeight = 4;
        }

        // Draw segmented pixel equalizer blocks
        for (int h = 0; h < barHeight; h += 3) {
            int segY = baseY - h - 2;
            uint16_t segCol = isPlaying ? COL_GREEN : 0x0B88;
            tft.fillRect(leftX, segY, 3, 2, segCol);
            tft.fillRect(rightX, segY, 3, 2, segCol);
        }
    }
}

void SpotifyUI::drawTrackInfo(Adafruit_ST7735 &tft, const String &title, const String &artist) {
    // Erase only title + artist bounding box
    tft.fillRect(0, 80, 128, 23, COL_BG);

    String displayTitle = title.isEmpty() ? "No Track Playing" : title;
    drawCenteredText(tft, displayTitle, 82, COL_WHITE, 19);

    String displayArtist = artist.isEmpty() ? "Spotify Idle" : artist;
    drawCenteredText(tft, displayArtist, 93, COL_GRAY_TEXT, 20);
}

void SpotifyUI::drawProgressBar(Adafruit_ST7735 &tft, uint32_t progressMs, uint32_t durationMs) {
    char timeStr[10];

    // Left: Elapsed Time (Y = 105)
    formatTime(progressMs, timeStr, sizeof(timeStr));
    tft.setFont(NULL);
    tft.setTextSize(1);
    tft.fillRect(4, 104, 30, 9, COL_BG);
    tft.setTextColor(COL_WHITE, COL_BG);
    tft.setCursor(4, 105);
    tft.print(timeStr);

    // Right: Total Duration (Y = 105)
    formatTime(durationMs, timeStr, sizeof(timeStr));
    tft.fillRect(96, 104, 30, 9, COL_BG);
    tft.setTextColor(COL_WHITE, COL_BG);
    tft.setCursor(96, 105);
    tft.print(timeStr);

    // Center: Thick Progress Bar (X = 36 to X = 92, Width = 56, Height = 4)
    int16_t trackX = 36;
    int16_t trackY = 106;
    int16_t trackW = 56;
    int16_t trackH = 4;

    int16_t fillW = 0;
    if (durationMs > 0) {
        if (progressMs > durationMs) progressMs = durationMs;
        fillW = (int16_t)(((uint64_t)progressMs * trackW) / durationMs);
    }

    tft.fillRect(trackX, trackY, trackW, trackH, COL_TRACK_BG);
    if (fillW > 0) {
        tft.fillRect(trackX, trackY, fillW, trackH, COL_GREEN);
    }
}

void SpotifyUI::drawControls(Adafruit_ST7735 &tft, bool isPlaying, SpotifyControlSelection selection) {
    // Clear only controls area (Y = 117 .. 143)
    tft.fillRect(0, 117, 128, 27, COL_BG);

    // 1. Previous Button (|◀)
    int16_t prevX = 28;
    int16_t prevY = 130;
    uint16_t prevCol = (selection == CTRL_PREV) ? COL_GREEN : COL_WHITE;

    tft.drawFastVLine(prevX - 7, prevY - 5, 11, prevCol);
    tft.fillTriangle(prevX + 5, prevY - 5, prevX + 5, prevY + 5, prevX - 4, prevY, prevCol);

    if (selection == CTRL_PREV) {
        tft.drawRoundRect(prevX - 11, prevY - 9, 23, 19, 3, COL_GREEN);
    }

    // 2. Center Play/Pause Button
    int16_t playX = 64;
    int16_t playY = 130;
    tft.fillCircle(playX, playY, 11, COL_GREEN);

    if (isPlaying) {
        tft.fillRect(playX - 4, playY - 5, 3, 10, COL_BG);
        tft.fillRect(playX + 1, playY - 5, 3, 10, COL_BG);
    } else {
        tft.fillTriangle(playX - 3, playY - 5, playX - 3, playY + 5, playX + 5, playY, COL_BG);
    }

    if (selection == CTRL_PLAYPAUSE) {
        tft.drawCircle(playX, playY, 13, COL_WHITE);
    }

    // 3. Next Button (▶|)
    int16_t nextX = 100;
    int16_t nextY = 130;
    uint16_t nextCol = (selection == CTRL_NEXT) ? COL_GREEN : COL_WHITE;

    tft.fillTriangle(nextX - 5, nextY - 5, nextX - 5, nextY + 5, nextX + 4, nextY, nextCol);
    tft.drawFastVLine(nextX + 7, nextY - 5, 11, nextCol);

    if (selection == CTRL_NEXT) {
        tft.drawRoundRect(nextX - 11, prevY - 9, 23, 19, 3, COL_GREEN);
    }
}

void SpotifyUI::drawBottomBar(Adafruit_ST7735 &tft) {
    tft.fillRect(0, 145, 128, 15, COL_BG);
    tft.drawBitmap(24, 149, heart_icon_9x8, 8, 8, COL_GREEN);
    tft.drawBitmap(60, 149, shuffle_icon_10x8, 8, 8, COL_GREEN);
    tft.drawBitmap(96, 149, repeat_icon_10x8, 8, 8, COL_WHITE);
}

void SpotifyUI::drawOfflineState(Adafruit_ST7735 &tft) {
    tft.fillScreen(COL_BG);
    drawStatusBar(tft, false);
    drawSpotifyHeader(tft);

    tft.fillRoundRect(10, 48, 108, 64, 4, COL_CARD_BG);
    tft.drawRoundRect(10, 48, 108, 64, 4, COL_RED);

    drawCenteredText(tft, "BRIDGE OFFLINE", 60, COL_RED, 18);
    drawCenteredText(tft, "Check PC Bridge", 76, COL_WHITE, 18);
    drawCenteredText(tft, "[BACK] Return Home", 94, COL_GRAY_TEXT, 18);
}

void SpotifyUI::drawIdleState(Adafruit_ST7735 &tft) {
    tft.fillScreen(COL_BG);
    drawStatusBar(tft, true);
    drawSpotifyHeader(tft);
    drawArtwork(tft, nullptr, false, false);
    drawSideEqualizers(tft, false);

    drawCenteredText(tft, "NO MUSIC PLAYING", 82, COL_WHITE, 18);
    drawCenteredText(tft, "Open Spotify on PC", 93, COL_GRAY_TEXT, 18);

    drawProgressBar(tft, 0, 0);
    drawControls(tft, false, CTRL_PLAYPAUSE);
    drawBottomBar(tft);
}

void SpotifyUI::drawFullUI(Adafruit_ST7735 &tft, const SpotifyTrackState &state, SpotifyControlSelection selection, const uint16_t *artworkBuffer, bool hasArtwork) {
    if (!state.connected) {
        drawOfflineState(tft);
        lastRenderedConnected = false;
        return;
    }

    if (!state.playing && state.title.isEmpty()) {
        drawIdleState(tft);
        lastRenderedTitle = "";
        lastRenderedArtist = "";
        lastRenderedPlaying = false;
        lastRenderedConnected = true;
        lastRenderedSelection = selection;
        lastRenderedHasArtwork = false;
        return;
    }

    // Initial Full Layout Setup
    drawStatusBar(tft, state.connected);
    drawSpotifyHeader(tft);
    drawArtwork(tft, artworkBuffer, hasArtwork, state.playing);
    drawSideEqualizers(tft, state.playing);
    drawTrackInfo(tft, state.title, state.artist);
    drawProgressBar(tft, state.estimatedProgressMs(), state.durationMs);
    drawControls(tft, state.playing, selection);
    drawBottomBar(tft);

    lastRenderedTitle = state.title;
    lastRenderedArtist = state.artist;
    lastRenderedPlaying = state.playing;
    lastRenderedConnected = state.connected;
    lastRenderedHasArtwork = hasArtwork;
    lastRenderedProgSec = state.estimatedProgressMs() / 1000;
    lastRenderedDurSec = state.durationMs / 1000;
    lastRenderedSelection = selection;
}

void SpotifyUI::updateDynamicUI(Adafruit_ST7735 &tft, const SpotifyTrackState &state, SpotifyControlSelection selection, bool stateChanged, const uint16_t *artworkBuffer, bool hasArtwork) {
    if (!state.connected) {
        if (lastRenderedConnected) {
            drawOfflineState(tft);
            lastRenderedConnected = false;
        }
        return;
    }

    // If connection was restored from offline
    if (!lastRenderedConnected) {
        tft.fillScreen(COL_BG);
        drawFullUI(tft, state, selection, artworkBuffer, hasArtwork);
        return;
    }

    // Track or Artwork changed -> partial component refresh (no full screen erase)
    if (state.title != lastRenderedTitle || state.artist != lastRenderedArtist || hasArtwork != lastRenderedHasArtwork || stateChanged) {
        drawArtwork(tft, artworkBuffer, hasArtwork, state.playing);
        drawTrackInfo(tft, state.title, state.artist);
        lastRenderedTitle = state.title;
        lastRenderedArtist = state.artist;
        lastRenderedHasArtwork = hasArtwork;
    }

    // Play/Pause state changed -> update controls & artwork border
    if (state.playing != lastRenderedPlaying || selection != lastRenderedSelection) {
        lastRenderedPlaying = state.playing;
        lastRenderedSelection = selection;
        drawControls(tft, state.playing, selection);
        drawArtwork(tft, artworkBuffer, hasArtwork, state.playing);
    }

    // Dynamic Side Equalizers (every ~65ms when playing)
    unsigned long now = millis();
    if (state.playing && (now - lastWaveMillis >= 65)) {
        lastWaveMillis = now;
        drawSideEqualizers(tft, true);
    }

    // Dynamic Progress Bar (every second)
    uint32_t curProgSec = state.estimatedProgressMs() / 1000;
    uint32_t curDurSec = state.durationMs / 1000;

    if (curProgSec != lastRenderedProgSec || curDurSec != lastRenderedDurSec) {
        lastRenderedProgSec = curProgSec;
        lastRenderedDurSec = curDurSec;
        drawProgressBar(tft, state.estimatedProgressMs(), state.durationMs);
    }
}
