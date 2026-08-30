#include "SpotifyUI.h"
#include <math.h>

// Color Palette (RGB565)
#define COL_BG          0x0000  // Pure Black
#define COL_CARD_BG     0x1082  // Deep Slate (#121212)
#define COL_GREEN       0x1DB2  // Spotify Green (#1DB954)
#define COL_GREEN_DIM   0x0B88  // Dim Green
#define COL_WHITE       0xFFFF  // Crisp White
#define COL_GRAY_LIGHT  0xAD55  // Light Gray
#define COL_GRAY_DARK   0x3186  // Dark Gray Track / Border
#define COL_RED         0xF800  // Red for Offline Warning
#define COL_PAUSED      0xFEA0  // Warm Amber / Yellow

// Cached variables for dirty tracking
static String   lastRenderedTitle = "";
static String   lastRenderedArtist = "";
static bool     lastRenderedPlaying = false;
static bool     lastRenderedConnected = true;
static bool     lastRenderedHasArtwork = false;
static uint32_t lastRenderedProgSec = 0xFFFFFFFF;
static uint32_t lastRenderedDurSec = 0xFFFFFFFF;
static SpotifyControlSelection lastRenderedSelection = (SpotifyControlSelection)99;
static unsigned long lastWaveMillis = 0;

void SpotifyUI::init(Adafruit_ST7735 &tft) {
    tft.setRotation(2);
    tft.fillScreen(COL_BG);

    lastRenderedTitle = "\t"; // Force initial draw
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

static void drawCenteredSimpleText(Adafruit_ST7735 &tft, const String &text, int16_t y, uint16_t color, int maxChars = 20) {
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

void SpotifyUI::drawHeader(Adafruit_ST7735 &tft, bool isConnected) {
    tft.fillRect(0, 0, 128, 14, COL_BG);

    // Spotify Green Logo Dot on left
    tft.fillCircle(8, 7, 3, COL_GREEN);

    // Clean, readable "SPOTIFY" title
    tft.setFont(NULL);
    tft.setTextSize(1);
    tft.setTextColor(COL_WHITE, COL_BG);
    tft.setCursor(18, 4);
    tft.print("SPOTIFY");

    // Right Connection Indicator Dot
    uint16_t dotColor = isConnected ? COL_GREEN : COL_RED;
    tft.fillCircle(120, 7, 2, dotColor);

    // Header bottom divider line
    tft.drawFastHLine(0, 14, 128, COL_GRAY_DARK);
}

void SpotifyUI::drawArtwork(Adafruit_ST7735 &tft, const uint16_t *artworkBuffer, bool hasArtwork, bool isPlaying) {
    int16_t boxX = 44;
    int16_t boxY = 16;
    int16_t boxSize = 40;

    if (hasArtwork && artworkBuffer != nullptr) {
        // Draw real album artwork bitmap
        tft.drawRGBBitmap(boxX, boxY, (uint16_t*)artworkBuffer, boxSize, boxSize);
        // Outer glowing border
        tft.drawRect(boxX - 1, boxY - 1, boxSize + 2, boxSize + 2, isPlaying ? COL_GREEN : COL_GRAY_DARK);
    } else {
        // Fallback Retro Vinyl Disc
        tft.fillRoundRect(boxX, boxY, boxSize, boxSize, 3, COL_CARD_BG);
        tft.drawRoundRect(boxX - 1, boxY - 1, boxSize + 2, boxSize + 2, 3, isPlaying ? COL_GREEN : COL_GRAY_DARK);

        int16_t centerX = boxX + boxSize / 2; // 64
        int16_t centerY = boxY + boxSize / 2; // 36

        tft.fillCircle(centerX, centerY, 13, 0x18C3);
        tft.drawCircle(centerX, centerY, 13, COL_GRAY_DARK);
        tft.drawCircle(centerX, centerY, 9, 0x2104);
        tft.drawCircle(centerX, centerY, 6, 0x2945);

        tft.fillCircle(centerX, centerY, 3, isPlaying ? COL_GREEN : COL_GRAY_LIGHT);
        tft.fillCircle(centerX, centerY, 1, COL_BG);
    }
}

void SpotifyUI::drawTrackInfo(Adafruit_ST7735 &tft, const String &title, const String &artist, const String &album, bool isPlaying) {
    // 1. Title Area (Y = 59..69)
    tft.fillRect(0, 59, 128, 11, COL_BG);
    String displayTitle = title.isEmpty() ? "No Track Playing" : title;
    drawCenteredSimpleText(tft, displayTitle, 60, COL_WHITE, 20);

    // 2. Artist Area (Y = 71..81)
    tft.fillRect(0, 71, 128, 11, COL_BG);
    String displayArtist = artist.isEmpty() ? "Spotify Idle" : artist;
    drawCenteredSimpleText(tft, displayArtist, 72, COL_GRAY_LIGHT, 20);

    // 3. Playback State / Album Area (Y = 83..92)
    tft.fillRect(0, 83, 128, 10, COL_BG);
    if (!isPlaying && !title.isEmpty()) {
        drawCenteredSimpleText(tft, "PAUSED", 84, COL_PAUSED, 20);
    } else if (!album.isEmpty()) {
        drawCenteredSimpleText(tft, album, 84, COL_GRAY_DARK, 20);
    }
}

void SpotifyUI::drawProgressBar(Adafruit_ST7735 &tft, uint32_t progressMs, uint32_t durationMs) {
    char timeStr[10];

    // Elapsed Time (Left)
    formatTime(progressMs, timeStr, sizeof(timeStr));
    tft.setFont(NULL);
    tft.setTextSize(1);
    tft.fillRect(2, 95, 32, 9, COL_BG);
    tft.setTextColor(COL_WHITE, COL_BG);
    tft.setCursor(3, 96);
    tft.print(timeStr);

    // Total Duration (Right)
    formatTime(durationMs, timeStr, sizeof(timeStr));
    tft.fillRect(94, 95, 32, 9, COL_BG);
    tft.setTextColor(COL_GRAY_LIGHT, COL_BG);
    tft.setCursor(96, 96);
    tft.print(timeStr);

    // Progress Bar Track (X = 36 to X = 92, W = 56, H = 3)
    int16_t trackX = 36;
    int16_t trackY = 98;
    int16_t trackW = 56;
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

void SpotifyUI::drawControls(Adafruit_ST7735 &tft, bool isPlaying, SpotifyControlSelection selection) {
    // Clear Controls Area (Y = 110 .. 138)
    tft.fillRect(0, 110, 128, 29, COL_BG);

    // 1. Previous Button (Center X = 28, Center Y = 123)
    int16_t prevX = 28;
    int16_t prevY = 123;
    uint16_t prevColor = (selection == CTRL_PREV) ? COL_GREEN : COL_WHITE;

    tft.drawFastVLine(prevX - 6, prevY - 4, 9, prevColor);
    tft.fillTriangle(prevX + 4, prevY - 4, prevX + 4, prevY + 4, prevX - 4, prevY, prevColor);

    if (selection == CTRL_PREV) {
        tft.drawRoundRect(prevX - 10, prevY - 8, 21, 17, 3, COL_GREEN);
    }

    // 2. Play/Pause Button (Center X = 64, Center Y = 123)
    int16_t playX = 64;
    int16_t playY = 123;
    uint16_t playBtnBg = (selection == CTRL_PLAYPAUSE) ? COL_GREEN : COL_WHITE;
    uint16_t playIconCol = COL_BG;

    // Circular Play/Pause button
    tft.fillCircle(playX, playY, 10, playBtnBg);

    if (isPlaying) {
        // Pause icon: Two vertical bars
        tft.fillRect(playX - 4, playY - 4, 3, 9, playIconCol);
        tft.fillRect(playX + 1, playY - 4, 3, 9, playIconCol);
    } else {
        // Play icon: Right triangle
        tft.fillTriangle(playX - 3, playY - 4, playX - 3, playY + 4, playX + 4, playY, playIconCol);
    }

    if (selection == CTRL_PLAYPAUSE) {
        tft.drawCircle(playX, playY, 12, COL_GREEN);
    }

    // 3. Next Button (Center X = 100, Center Y = 123)
    int16_t nextX = 100;
    int16_t nextY = 123;
    uint16_t nextColor = (selection == CTRL_NEXT) ? COL_GREEN : COL_WHITE;

    tft.fillTriangle(nextX - 4, nextY - 4, nextX - 4, nextY + 4, nextX + 4, nextY, nextColor);
    tft.drawFastVLine(nextX + 6, nextY - 4, 9, nextColor);

    if (selection == CTRL_NEXT) {
        tft.drawRoundRect(nextX - 10, nextY - 8, 21, 17, 3, COL_GREEN);
    }
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

    drawCenteredSimpleText(tft, "BRIDGE OFFLINE", 58, COL_RED, 18);
    drawCenteredSimpleText(tft, "Check PC Bridge", 74, COL_WHITE, 18);
    drawCenteredSimpleText(tft, "[BACK] Return Home", 92, COL_GRAY_LIGHT, 18);
}

void SpotifyUI::drawIdleState(Adafruit_ST7735 &tft) {
    tft.fillScreen(COL_BG);
    drawHeader(tft, true);
    drawArtwork(tft, nullptr, false, false);

    drawCenteredSimpleText(tft, "NO MUSIC PLAYING", 64, COL_WHITE, 18);
    drawCenteredSimpleText(tft, "Open Spotify on PC", 78, COL_GRAY_LIGHT, 18);

    drawProgressBar(tft, 0, 0);
    drawControls(tft, false, CTRL_PLAYPAUSE);
    drawEqualizerWaves(tft, false);
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

    tft.fillScreen(COL_BG);

    drawHeader(tft, state.connected);
    drawArtwork(tft, artworkBuffer, hasArtwork, state.playing);
    drawTrackInfo(tft, state.title, state.artist, state.album, state.playing);
    drawProgressBar(tft, state.estimatedProgressMs(), state.durationMs);
    drawControls(tft, state.playing, selection);
    drawEqualizerWaves(tft, state.playing);

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

    // Full UI Redraw if track changed, artwork loaded, or connection restored
    if (stateChanged || !lastRenderedConnected || (hasArtwork != lastRenderedHasArtwork) || state.title != lastRenderedTitle || state.artist != lastRenderedArtist || state.playing != lastRenderedPlaying) {
        drawFullUI(tft, state, selection, artworkBuffer, hasArtwork);
        return;
    }

    // Controls focus update if selection changed
    if (selection != lastRenderedSelection) {
        lastRenderedSelection = selection;
        drawControls(tft, state.playing, selection);
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
