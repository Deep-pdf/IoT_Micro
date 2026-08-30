#include "SpotifyUI.h"
#include "BLADRMF_4pt7b.h"
#include <math.h>

// Color Palette (RGB565)
#define COL_BG          0x0000  // Pure Black
#define COL_CARD_BG     0x1082  // Deep Slate (#121212)
#define COL_GREEN       0x1DB2  // Spotify Green (#1DB954)
#define COL_WHITE       0xFFFF  // Crisp White
#define COL_GRAY_LIGHT  0xAD55  // Light Gray
#define COL_GRAY_DARK   0x3186  // Dark Gray Border/Track
#define COL_RED         0xF800  // Red for Offline Warning
#define COL_PAUSED      0xFEA0  // Amber / Warm Yellow

// Cached variables for dirty tracking
static String   lastRenderedTitle = "";
static String   lastRenderedArtist = "";
static bool     lastRenderedPlaying = false;
static bool     lastRenderedConnected = true;
static uint32_t lastRenderedProgSec = 0xFFFFFFFF;
static uint32_t lastRenderedDurSec = 0xFFFFFFFF;
static unsigned long lastWaveMillis = 0;

void SpotifyUI::init(Adafruit_ST7735 &tft) {
    tft.setRotation(2);
    tft.fillScreen(COL_BG);

    lastRenderedTitle = "\t"; // Force initial draw
    lastRenderedArtist = "\t";
    lastRenderedPlaying = false;
    lastRenderedConnected = false;
    lastRenderedProgSec = 0xFFFFFFFF;
    lastRenderedDurSec = 0xFFFFFFFF;
    lastWaveMillis = 0;
}

void SpotifyUI::formatTime(uint32_t ms, char *outBuffer, size_t bufSize) {
    uint32_t totalSec = ms / 1000;
    uint32_t minutes = totalSec / 60;
    uint32_t seconds = totalSec % 60;
    snprintf(outBuffer, bufSize, "%02u:%02u", (unsigned int)minutes, (unsigned int)seconds);
}

// Safely fits text to maxWidth using exact GFX font metrics
static String fitTextToBounds(Adafruit_ST7735 &tft, const String &text, int16_t maxWidth) {
    if (text.isEmpty()) return "";
    int16_t x1, y1;
    uint16_t w, h;
    tft.getTextBounds(text.c_str(), 0, 0, &x1, &y1, &w, &h);
    if (w <= maxWidth) return text;

    String truncated = text;
    while (truncated.length() > 0) {
        truncated.remove(truncated.length() - 1);
        String testStr = truncated + "..";
        tft.getTextBounds(testStr.c_str(), 0, 0, &x1, &y1, &w, &h);
        if (w <= maxWidth) {
            return testStr;
        }
    }
    return "..";
}

// Draws centered text using BLADRMF_4pt7b font metrics
static void drawCenteredGfxText(Adafruit_ST7735 &tft, const String &text, int16_t baselineY, uint16_t color, int16_t maxWidth = 120) {
    tft.setFont(&BLADRMF_4pt7b);
    String fitted = fitTextToBounds(tft, text, maxWidth);

    int16_t x1, y1;
    uint16_t w, h;
    tft.getTextBounds(fitted.c_str(), 0, baselineY, &x1, &y1, &w, &h);

    int16_t drawX = (128 - (int16_t)w) / 2 - x1;
    if (drawX < 2) drawX = 2;

    tft.setTextColor(color);
    tft.setCursor(drawX, baselineY);
    tft.print(fitted);
}

void SpotifyUI::drawHeader(Adafruit_ST7735 &tft, bool isConnected) {
    tft.fillRect(0, 0, 128, 14, COL_BG);

    // Spotify Green Logo Dot on left
    tft.fillCircle(8, 7, 3, COL_GREEN);

    // Header title "SPOTIFY" in BLADRMF_4pt7b font
    tft.setFont(&BLADRMF_4pt7b);
    tft.setTextColor(COL_WHITE);
    tft.setCursor(16, 9);
    tft.print("SPOTIFY");

    // Right Connection Indicator Dot
    uint16_t dotColor = isConnected ? COL_GREEN : COL_RED;
    tft.fillCircle(120, 7, 2, dotColor);

    // Header bottom divider line
    tft.drawFastHLine(0, 14, 128, COL_GRAY_DARK);
}

void SpotifyUI::drawArtworkPlaceholder(Adafruit_ST7735 &tft, bool isPlaying) {
    int16_t boxX = 43;
    int16_t boxY = 18;
    int16_t boxW = 42;
    int16_t boxH = 42;

    // Artwork Box Background & Rounded Frame
    tft.fillRoundRect(boxX, boxY, boxW, boxH, 4, COL_CARD_BG);
    tft.drawRoundRect(boxX, boxY, boxW, boxH, 4, isPlaying ? COL_GREEN : COL_GRAY_DARK);

    // Inner Retro Vinyl Disc Graphic
    int16_t centerX = boxX + boxW / 2; // 64
    int16_t centerY = boxY + boxH / 2; // 39

    tft.fillCircle(centerX, centerY, 15, 0x18C3); // Outer Vinyl
    tft.drawCircle(centerX, centerY, 15, COL_GRAY_DARK);
    tft.drawCircle(centerX, centerY, 11, 0x2104); // Vinyl Groove
    tft.drawCircle(centerX, centerY, 7,  0x2945);

    // Center Spindle Hub
    tft.fillCircle(centerX, centerY, 4, isPlaying ? COL_GREEN : COL_GRAY_LIGHT);
    tft.fillCircle(centerX, centerY, 1, COL_BG);  // Spindle Hole
}

void SpotifyUI::drawTrackInfo(Adafruit_ST7735 &tft, const String &title, const String &artist, const String &album, bool isPlaying) {
    // 1. Title Area (Y = 64..76)
    tft.fillRect(0, 64, 128, 13, COL_BG);
    String displayTitle = title.isEmpty() ? "No Track Playing" : title;
    drawCenteredGfxText(tft, displayTitle, 73, COL_WHITE, 120);

    // 2. Artist Area (Y = 78..89)
    tft.fillRect(0, 78, 128, 12, COL_BG);
    String displayArtist = artist.isEmpty() ? "Spotify Idle" : artist;
    drawCenteredGfxText(tft, displayArtist, 86, COL_GRAY_LIGHT, 120);

    // 3. Playback State / Album Area (Y = 91..101)
    tft.fillRect(0, 91, 128, 11, COL_BG);
    if (!isPlaying && !title.isEmpty()) {
        drawCenteredGfxText(tft, "PAUSED", 98, COL_PAUSED, 120);
    } else if (!album.isEmpty()) {
        drawCenteredGfxText(tft, album, 98, COL_GRAY_DARK, 116);
    }
}

void SpotifyUI::drawProgressBar(Adafruit_ST7735 &tft, uint32_t progressMs, uint32_t durationMs) {
    char timeStr[10];

    // Elapsed Time String (Left)
    formatTime(progressMs, timeStr, sizeof(timeStr));
    tft.setFont(NULL);
    tft.setTextSize(1);
    tft.fillRect(2, 107, 30, 9, COL_BG);
    tft.setTextColor(COL_WHITE, COL_BG);
    tft.setCursor(3, 108);
    tft.print(timeStr);

    // Total Duration String (Right)
    formatTime(durationMs, timeStr, sizeof(timeStr));
    tft.fillRect(96, 107, 30, 9, COL_BG);
    tft.setTextColor(COL_GRAY_LIGHT, COL_BG);
    tft.setCursor(97, 108);
    tft.print(timeStr);

    // Progress Bar Track (X = 35 to X = 93, W = 58, H = 3)
    int16_t trackX = 35;
    int16_t trackY = 110;
    int16_t trackW = 58;
    int16_t trackH = 3;

    int16_t fillW = 0;
    if (durationMs > 0) {
        if (progressMs > durationMs) progressMs = durationMs;
        fillW = (int16_t)(((uint64_t)progressMs * trackW) / durationMs);
    }

    tft.fillRect(trackX, trackY, trackW, trackH, COL_GRAY_DARK);
    if (fillW > 0) {
        tft.fillRect(trackX, trackY, fillW, trackH, COL_GREEN);
    }

    // Small Thumb Indicator
    int16_t thumbX = trackX + fillW;
    if (thumbX > trackX + trackW - 1) thumbX = trackX + trackW - 1;
    tft.drawFastVLine(thumbX, trackY - 1, 5, COL_WHITE);
}

void SpotifyUI::drawControlsVisual(Adafruit_ST7735 &tft, bool isPlaying) {
    // Clear Controls Area (Y = 120 .. 138)
    tft.fillRect(0, 120, 128, 19, COL_BG);

    // 1. Previous Icon (|<<) at X=28, Y=129
    int16_t prevX = 28;
    int16_t prevY = 129;
    tft.drawFastVLine(prevX - 6, prevY - 4, 9, COL_WHITE);
    tft.fillTriangle(prevX + 4, prevY - 4, prevX + 4, prevY + 4, prevX - 4, prevY, COL_WHITE);

    // 2. Play/Pause Icon at X=64, Y=129
    int16_t playX = 64;
    int16_t playY = 129;

    if (isPlaying) {
        // Pause icon: Two vertical bars
        tft.fillRect(playX - 4, playY - 5, 3, 10, COL_WHITE);
        tft.fillRect(playX + 1, playY - 5, 3, 10, COL_WHITE);
    } else {
        // Play icon: Right triangle
        tft.fillTriangle(playX - 4, playY - 5, playX - 4, playY + 5, playX + 5, playY, COL_WHITE);
    }

    // 3. Next Icon (>>|) at X=100, Y=129
    int16_t nextX = 100;
    int16_t nextY = 129;
    tft.fillTriangle(nextX - 4, nextY - 4, nextX - 4, nextY + 4, nextX + 4, nextY, COL_WHITE);
    tft.drawFastVLine(nextX + 6, nextY - 4, 9, COL_WHITE);
}

void SpotifyUI::drawEqualizerWaves(Adafruit_ST7735 &tft, bool isPlaying) {
    const int numBars = 11;
    const int barWidth = 3;
    const int startX = 28;
    const int baseY = 156;
    const int maxHeight = 12;

    unsigned long now = millis();

    // Clear wave area
    tft.fillRect(startX - 2, baseY - maxHeight, (numBars * 7) + 4, maxHeight + 2, COL_BG);

    for (int i = 0; i < numBars; i++) {
        int barX = startX + i * 7;
        int barHeight = 2;

        if (isPlaying) {
            float phase1 = (now / 110.0f) + (i * 0.85f);
            float phase2 = (now / 230.0f) + (i * 1.6f);
            float wave = (sin(phase1) + 1.0f) * 0.5f * 6.0f + (cos(phase2) + 1.0f) * 0.5f * 5.0f;
            barHeight = 2 + (int)wave;
            if (barHeight > maxHeight) barHeight = maxHeight;
            if (barHeight < 2) barHeight = 2;
        }

        int barY = baseY - barHeight;
        uint16_t barColor = isPlaying ? COL_GREEN : COL_GRAY_DARK;
        tft.fillRect(barX, barY, barWidth, barHeight, barColor);
    }
}

void SpotifyUI::drawOfflineState(Adafruit_ST7735 &tft) {
    tft.fillScreen(COL_BG);
    drawHeader(tft, false);

    tft.fillRoundRect(10, 48, 108, 64, 6, COL_CARD_BG);
    tft.drawRoundRect(10, 48, 108, 64, 6, COL_RED);

    drawCenteredGfxText(tft, "BRIDGE OFFLINE", 66, COL_RED, 100);
    drawCenteredGfxText(tft, "Check PC Server", 82, COL_WHITE, 100);
    drawCenteredGfxText(tft, "Press BACK to exit", 98, COL_GRAY_LIGHT, 100);
}

void SpotifyUI::drawIdleState(Adafruit_ST7735 &tft) {
    tft.fillScreen(COL_BG);
    drawHeader(tft, true);
    drawArtworkPlaceholder(tft, false);

    drawCenteredGfxText(tft, "NO MUSIC PLAYING", 74, COL_WHITE, 116);
    drawCenteredGfxText(tft, "Start Spotify on PC", 88, COL_GRAY_LIGHT, 116);

    drawProgressBar(tft, 0, 0);
    drawControlsVisual(tft, false);
    drawEqualizerWaves(tft, false);
}

void SpotifyUI::drawFullUI(Adafruit_ST7735 &tft, const SpotifyTrackState &state) {
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
        return;
    }

    tft.fillScreen(COL_BG);

    drawHeader(tft, state.connected);
    drawArtworkPlaceholder(tft, state.playing);
    drawTrackInfo(tft, state.title, state.artist, state.album, state.playing);
    drawProgressBar(tft, state.estimatedProgressMs(), state.durationMs);
    drawControlsVisual(tft, state.playing);
    drawEqualizerWaves(tft, state.playing);

    lastRenderedTitle = state.title;
    lastRenderedArtist = state.artist;
    lastRenderedPlaying = state.playing;
    lastRenderedConnected = state.connected;
    lastRenderedProgSec = state.estimatedProgressMs() / 1000;
    lastRenderedDurSec = state.durationMs / 1000;
}

void SpotifyUI::updateDynamicUI(Adafruit_ST7735 &tft, const SpotifyTrackState &state, bool stateChanged) {
    if (!state.connected) {
        if (lastRenderedConnected) {
            drawOfflineState(tft);
            lastRenderedConnected = false;
        }
        return;
    }

    // Full UI Redraw if track changed or connection restored
    if (stateChanged || !lastRenderedConnected || state.title != lastRenderedTitle || state.artist != lastRenderedArtist || state.playing != lastRenderedPlaying) {
        drawFullUI(tft, state);
        return;
    }

    // Dynamic Retro Equalizer Waves (every ~65ms when playing)
    unsigned long now = millis();
    if (state.playing && (now - lastWaveMillis >= 65)) {
        lastWaveMillis = now;
        drawEqualizerWaves(tft, true);
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
