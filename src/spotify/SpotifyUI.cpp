#include "SpotifyUI.h"
#include "Dream_Orphans_Bd6pt7b.h"
#include <math.h>

// Color Definitions
#define COL_BG          0x0000  // Pure Black
#define COL_CARD_BG     0x1082  // Very dark slate (#121212)
#define COL_GREEN       0x1DB2  // Spotify Vibrant Green (#1DB954)
#define COL_GREEN_DIM   0x0B88  // Dim Green
#define COL_WHITE       0xFFFF  // Crisp White
#define COL_GRAY_LIGHT  0xAD55  // Light Gray
#define COL_GRAY_DARK   0x3186  // Dark Slate
#define COL_HIGHLIGHT   0x25F5  // Neon Highlight Border

// Cached state tracking for flicker-free dirty updates
static String   lastTitle = "";
static String   lastArtist = "";
static bool     lastPlaying = false;
static uint32_t lastProgressSec = 0xFFFFFFFF;
static uint32_t lastDurationSec = 0xFFFFFFFF;
static SpotifyControlSelection lastSelection = (SpotifyControlSelection)99;
static unsigned long lastWaveUpdate = 0;
static int lastMarqueeOffset = 0;
static unsigned long lastMarqueeTick = 0;

void SpotifyUI::init(Adafruit_ST7735 &tft) {
    tft.setRotation(2);
    tft.fillScreen(COL_BG);

    lastTitle = "\t"; // Force refresh
    lastArtist = "\t";
    lastPlaying = false;
    lastProgressSec = 0xFFFFFFFF;
    lastDurationSec = 0xFFFFFFFF;
    lastSelection = (SpotifyControlSelection)99;
    lastWaveUpdate = 0;
    lastMarqueeOffset = 0;
    lastMarqueeTick = millis();
}

void SpotifyUI::formatTime(uint32_t ms, char *outBuffer, size_t bufSize) {
    uint32_t totalSec = ms / 1000;
    uint32_t minutes = totalSec / 60;
    uint32_t seconds = totalSec % 60;
    snprintf(outBuffer, bufSize, "%02u:%02u", (unsigned int)minutes, (unsigned int)seconds);
}

void SpotifyUI::drawHeader(Adafruit_ST7735 &tft, bool isConnected) {
    tft.fillRect(0, 0, 128, 14, COL_BG);

    // Spotify green dot on the left
    tft.fillCircle(10, 7, 3, COL_GREEN);

    // Header Title "SPOTIFY" in crisp font
    tft.setFont(NULL);
    tft.setTextSize(1);
    tft.setTextColor(COL_WHITE, COL_BG);
    tft.setCursor(20, 4);
    tft.print("SPOTIFY");

    // Right status indicator: Green dot if bridge connected, Gray/Red if disconnected
    if (isConnected) {
        tft.fillCircle(118, 7, 3, COL_GREEN);
    } else {
        tft.drawCircle(118, 7, 3, COL_GRAY_DARK);
    }

    // Subtle divider line
    tft.drawFastHLine(0, 14, 128, COL_GRAY_DARK);
}

void SpotifyUI::drawArtwork(Adafruit_ST7735 &tft, bool isPlaying) {
    int16_t boxX = 40;
    int16_t boxY = 18;
    int16_t boxW = 48;
    int16_t boxH = 48;

    // Outer artwork frame
    tft.fillRoundRect(boxX, boxY, boxW, boxH, 4, COL_CARD_BG);
    tft.drawRoundRect(boxX, boxY, boxW, boxH, 4, isPlaying ? COL_GREEN : COL_GRAY_DARK);

    // Stylized Retro Cassette / Vinyl graphic
    int16_t centerX = boxX + boxW / 2; // 64
    int16_t centerY = boxY + boxH / 2; // 42

    // Vinyl outer circle
    tft.fillCircle(centerX, centerY, 18, 0x18C3);
    tft.drawCircle(centerX, centerY, 18, COL_GRAY_DARK);
    tft.drawCircle(centerX, centerY, 13, 0x2104); // Vinyl groove
    tft.drawCircle(centerX, centerY, 8,  0x2945);

    // Center disc label
    tft.fillCircle(centerX, centerY, 6, isPlaying ? COL_GREEN : COL_GRAY_LIGHT);
    tft.fillCircle(centerX, centerY, 2, COL_BG); // Center hole

    // Small musical wave accents inside artwork corners
    if (isPlaying) {
        tft.drawPixel(boxX + 4, boxY + 4, COL_GREEN);
        tft.drawPixel(boxX + boxW - 5, boxY + 4, COL_GREEN);
    }
}

void SpotifyUI::drawTrackInfo(Adafruit_ST7735 &tft, const String &title, const String &artist) {
    tft.setFont(NULL);
    tft.setTextSize(1);

    // 1. Draw Title (Y = 71, Height = 10)
    tft.fillRect(0, 70, 128, 11, COL_BG);
    tft.setTextColor(COL_WHITE, COL_BG);

    String displayTitle = title.isEmpty() ? "No Track Playing" : title;
    int titleLen = displayTitle.length();

    if (titleLen <= 19) {
        // Fits comfortably centered
        int16_t textW = titleLen * 6;
        int16_t x = (128 - textW) / 2;
        if (x < 2) x = 2;
        tft.setCursor(x, 72);
        tft.print(displayTitle);
    } else {
        // Truncate cleanly with ellipsis for static display
        String shortTitle = displayTitle.substring(0, 17) + "..";
        int16_t textW = shortTitle.length() * 6;
        int16_t x = (128 - textW) / 2;
        if (x < 2) x = 2;
        tft.setCursor(x, 72);
        tft.print(shortTitle);
    }

    // 2. Draw Artist (Y = 82, Height = 10)
    tft.fillRect(0, 82, 128, 10, COL_BG);
    tft.setTextColor(COL_GRAY_LIGHT, COL_BG);

    String displayArtist = artist.isEmpty() ? "Spotify Idle" : artist;
    int artistLen = displayArtist.length();

    if (artistLen <= 19) {
        int16_t textW = artistLen * 6;
        int16_t x = (128 - textW) / 2;
        if (x < 2) x = 2;
        tft.setCursor(x, 83);
        tft.print(displayArtist);
    } else {
        String shortArtist = displayArtist.substring(0, 17) + "..";
        int16_t textW = shortArtist.length() * 6;
        int16_t x = (128 - textW) / 2;
        if (x < 2) x = 2;
        tft.setCursor(x, 83);
        tft.print(shortArtist);
    }
}

void SpotifyUI::drawEqualizerWaves(Adafruit_ST7735 &tft, bool isPlaying) {
    const int numBars = 11;
    const int barWidth = 3;
    const int startX = 28;
    const int baseY = 106; // Bottom of wave area
    const int maxHeight = 10;

    unsigned long now = millis();

    // Clear wave area
    tft.fillRect(startX - 2, baseY - maxHeight, (numBars * 7) + 4, maxHeight + 2, COL_BG);

    for (int i = 0; i < numBars; i++) {
        int barX = startX + i * 7;
        int barHeight = 2;

        if (isPlaying) {
            // Dynamic pseudo-random equalizer visual based on local timing
            float phase1 = (now / 110.0f) + (i * 0.85f);
            float phase2 = (now / 230.0f) + (i * 1.6f);
            float wave = (sin(phase1) + 1.0f) * 0.5f * 6.0f + (cos(phase2) + 1.0f) * 0.5f * 4.0f;
            barHeight = 2 + (int)wave;
            if (barHeight > maxHeight) barHeight = maxHeight;
            if (barHeight < 2) barHeight = 2;
        }

        int barY = baseY - barHeight;
        uint16_t barColor = isPlaying ? COL_GREEN : COL_GRAY_DARK;
        tft.fillRect(barX, barY, barWidth, barHeight, barColor);
    }
}

void SpotifyUI::drawProgressBar(Adafruit_ST7735 &tft, uint32_t progressMs, uint32_t durationMs) {
    char timeStr[10];

    // Elapsed Time (X = 4, Y = 114)
    formatTime(progressMs, timeStr, sizeof(timeStr));
    tft.setFont(NULL);
    tft.setTextSize(1);
    tft.fillRect(4, 113, 30, 9, COL_BG);
    tft.setTextColor(COL_WHITE, COL_BG);
    tft.setCursor(4, 114);
    tft.print(timeStr);

    // Duration Time (X = 94, Y = 114)
    formatTime(durationMs, timeStr, sizeof(timeStr));
    tft.fillRect(94, 113, 30, 9, COL_BG);
    tft.setTextColor(COL_GRAY_LIGHT, COL_BG);
    tft.setCursor(94, 114);
    tft.print(timeStr);

    // Progress Track (X = 36, Y = 116, W = 54, H = 3)
    int16_t trackX = 36;
    int16_t trackY = 116;
    int16_t trackW = 54;
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

    // Small progress thumb
    int16_t thumbX = trackX + fillW;
    if (thumbX > trackX + trackW) thumbX = trackX + trackW;
    tft.drawFastVLine(thumbX, trackY - 1, 5, COL_WHITE);
}

void SpotifyUI::drawControls(Adafruit_ST7735 &tft, bool isPlaying, SpotifyControlSelection selection) {
    // Clear controls region (Y = 127 .. 159)
    tft.fillRect(0, 127, 128, 33, COL_BG);

    // 1. Previous Button (Center X = 28, Center Y = 142)
    int16_t prevX = 28;
    int16_t prevY = 142;
    uint16_t prevCol = (selection == CTRL_PREV) ? COL_GREEN : COL_WHITE;

    // Previous Icon: Left Bar + Left Triangle
    tft.drawFastVLine(prevX - 7, prevY - 5, 11, prevCol);
    tft.drawFastVLine(prevX - 6, prevY - 5, 11, prevCol);
    tft.fillTriangle(prevX + 5, prevY - 5, prevX + 5, prevY + 5, prevX - 5, prevY, prevCol);

    // 2. Play/Pause Button (Center X = 64, Center Y = 142)
    int16_t playX = 64;
    int16_t playY = 142;
    uint16_t playBtnBg = (selection == CTRL_PLAYPAUSE) ? COL_GREEN : COL_WHITE;
    uint16_t playIconCol = COL_BG;

    // Circular button background
    tft.fillCircle(playX, playY, 11, playBtnBg);

    if (isPlaying) {
        // Pause Icon: Two vertical bars
        tft.fillRect(playX - 4, playY - 5, 3, 10, playIconCol);
        tft.fillRect(playX + 1, playY - 5, 3, 10, playIconCol);
    } else {
        // Play Icon: Right Triangle
        tft.fillTriangle(playX - 3, playY - 5, playX - 3, playY + 5, playX + 5, playY, playIconCol);
    }

    // 3. Next Button (Center X = 100, Center Y = 142)
    int16_t nextX = 100;
    int16_t nextY = 142;
    uint16_t nextCol = (selection == CTRL_NEXT) ? COL_GREEN : COL_WHITE;

    // Next Icon: Right Triangle + Right Bar
    tft.fillTriangle(nextX - 5, nextY - 5, nextX - 5, nextY + 5, nextX + 5, nextY, nextCol);
    tft.drawFastVLine(nextX + 6, nextY - 5, 11, nextCol);
    tft.drawFastVLine(nextX + 7, nextY - 5, 11, nextCol);

    // Focus Cursor Highlights (Subtle neon underline indicator)
    if (selection == CTRL_PREV) {
        tft.drawFastHLine(prevX - 10, 156, 20, COL_GREEN);
        tft.drawFastHLine(prevX - 7,  157, 14, COL_GREEN);
    } else if (selection == CTRL_PLAYPAUSE) {
        tft.drawFastHLine(playX - 12, 156, 24, COL_GREEN);
        tft.drawFastHLine(playX - 8,  157, 16, COL_GREEN);
    } else if (selection == CTRL_NEXT) {
        tft.drawFastHLine(nextX - 10, 156, 20, COL_GREEN);
        tft.drawFastHLine(nextX - 7,  157, 14, COL_GREEN);
    }
}

void SpotifyUI::drawOfflineBanner(Adafruit_ST7735 &tft) {
    tft.fillRoundRect(10, 50, 108, 48, 6, 0x2000);
    tft.drawRoundRect(10, 50, 108, 48, 6, 0xF800); // Red border

    tft.setFont(NULL);
    tft.setTextSize(1);
    tft.setTextColor(COL_WHITE, 0x2000);
    tft.setCursor(20, 58);
    tft.print("BRIDGE OFFLINE");

    tft.setTextColor(COL_GRAY_LIGHT, 0x2000);
    tft.setCursor(18, 72);
    tft.print("Check PC Server");
    tft.setCursor(16, 84);
    tft.print("Press BACK to exit");
}

void SpotifyUI::drawFullUI(Adafruit_ST7735 &tft, const SpotifyTrackState &state, SpotifyControlSelection selection) {
    tft.fillScreen(COL_BG);

    drawHeader(tft, state.connected);
    drawArtwork(tft, state.playing);
    drawTrackInfo(tft, state.title, state.artist);
    drawEqualizerWaves(tft, state.playing);
    drawProgressBar(tft, state.estimatedProgressMs(), state.durationMs);
    drawControls(tft, state.playing, selection);

    if (!state.connected && state.trackId.isEmpty()) {
        drawOfflineBanner(tft);
    }

    lastTitle = state.title;
    lastArtist = state.artist;
    lastPlaying = state.playing;
    lastProgressSec = state.estimatedProgressMs() / 1000;
    lastDurationSec = state.durationMs / 1000;
    lastSelection = selection;
}

void SpotifyUI::updateDynamicUI(Adafruit_ST7735 &tft, const SpotifyTrackState &state, SpotifyControlSelection selection, bool stateChanged) {
    uint32_t curProgSec = state.estimatedProgressMs() / 1000;
    uint32_t curDurSec = state.durationMs / 1000;
    unsigned long now = millis();

    // 1. Full redraw if track changed or connection state changed
    if (stateChanged || state.title != lastTitle || state.artist != lastArtist || state.playing != lastPlaying) {
        drawFullUI(tft, state, selection);
        return;
    }

    // 2. Dynamic Retro Equalizer Waves (animate smoothly every 70ms when playing)
    if (state.playing && (now - lastWaveUpdate >= 70)) {
        lastWaveUpdate = now;
        drawEqualizerWaves(tft, true);
    }

    // 3. Dynamic Progress Bar (update every second)
    if (curProgSec != lastProgressSec || curDurSec != lastDurationSec) {
        lastProgressSec = curProgSec;
        lastDurationSec = curDurSec;
        drawProgressBar(tft, state.estimatedProgressMs(), state.durationMs);
    }

    // 4. Update Controls only if selection changed
    if (selection != lastSelection) {
        lastSelection = selection;
        drawControls(tft, state.playing, selection);
    }
}
