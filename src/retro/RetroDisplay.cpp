#include "RetroDisplay.h"
#include <pgmspace.h>

// Static 128-pixel scanline buffer: 128 * 2 = 256 bytes total in SRAM (zero heap allocations)
static uint16_t scanlineBuffer[128];

void RetroDisplay::drawFrame(Adafruit_ST7735 &tft, const uint8_t *compressedData, size_t dataSize) {
    if (!compressedData || dataSize == 0) return;

    tft.setRotation(0); // 128x160 Portrait
    tft.startWrite();
    tft.setAddrWindow(0, 0, 128, 160);

    const uint8_t *ptr = compressedData;
    const uint8_t *end = compressedData + dataSize;

    uint32_t pixelsDecoded = 0;
    int linePixelIdx = 0;

    while (ptr < end && pixelsDecoded < 20480) {
        uint8_t ctrl = pgm_read_byte(ptr++);
        if (ctrl & 0x80) {
            // Run packet: (ctrl & 0x7F) + 1 repeats of the following 16-bit color
            int count = (ctrl & 0x7F) + 1;
            uint16_t high = pgm_read_byte(ptr++);
            uint16_t low = pgm_read_byte(ptr++);
            uint16_t color = (high << 8) | low;

            while (count > 0 && pixelsDecoded < 20480) {
                scanlineBuffer[linePixelIdx++] = color;
                pixelsDecoded++;
                count--;

                if (linePixelIdx == 128) {
                    tft.writePixels(scanlineBuffer, 128);
                    linePixelIdx = 0;
                }
            }
        } else {
            // Literal packet: ctrl + 1 distinct 16-bit colors
            int count = ctrl + 1;
            while (count > 0 && pixelsDecoded < 20480) {
                uint16_t high = pgm_read_byte(ptr++);
                uint16_t low = pgm_read_byte(ptr++);
                uint16_t color = (high << 8) | low;

                scanlineBuffer[linePixelIdx++] = color;
                pixelsDecoded++;
                count--;

                if (linePixelIdx == 128) {
                    tft.writePixels(scanlineBuffer, 128);
                    linePixelIdx = 0;
                }
            }
        }
    }

    // Flush any remaining pixels if needed
    if (linePixelIdx > 0) {
        tft.writePixels(scanlineBuffer, linePixelIdx);
    }

    tft.endWrite();
}

void RetroDisplay::drawLaunchNotification(Adafruit_ST7735 &tft, const char *gameTitle) {
    // Semi-translucent/accented notification box in lower center of screen
    int16_t bx = 10;
    int16_t by = 118;
    int16_t bw = 108;
    int16_t bh = 28;

    tft.fillRoundRect(bx, by, bw, bh, 4, 0x0000); // Black backing
    tft.drawRoundRect(bx, by, bw, bh, 4, 0xFD40); // Amber border
    tft.drawRoundRect(bx + 1, by + 1, bw - 2, bh - 2, 3, 0xFFFF);

    tft.setFont(NULL);
    tft.setTextSize(1);
    tft.setTextColor(0xFD40);
    tft.setCursor(bx + 8, by + 5);
    tft.print("LAUNCHING...");

    tft.setTextColor(0xFFFF);
    tft.setCursor(bx + 8, by + 16);
    tft.print(gameTitle);
}

void RetroDisplay::drawUnavailableNotification(Adafruit_ST7735 &tft, const char *gameTitle) {
    int16_t bx = 10;
    int16_t by = 118;
    int16_t bw = 108;
    int16_t bh = 28;

    tft.fillRoundRect(bx, by, bw, bh, 4, 0x0000); // Black backing
    tft.drawRoundRect(bx, by, bw, bh, 4, 0xF800); // Red border
    tft.drawRoundRect(bx + 1, by + 1, bw - 2, bh - 2, 3, 0xFFFF);

    tft.setFont(NULL);
    tft.setTextSize(1);
    tft.setTextColor(0xF800); // Red
    tft.setCursor(bx + 8, by + 5);
    tft.print("NOT AVAILABLE");

    tft.setTextColor(0xFFFF);
    tft.setCursor(bx + 8, by + 16);
    tft.print(gameTitle ? gameTitle : "Core Not Available");
}

void RetroDisplay::drawSettingsSection(Adafruit_ST7735 &tft, bool isFocused, uint8_t displayMode) {
    // Replaces old "Select / Play" bottom region (X=8..119, Y=137..152)
    int16_t sx = 8;
    int16_t sy = 137;
    int16_t sw = 112;
    int16_t sh = 16;

    tft.fillRoundRect(sx, sy, sw, sh, 3, 0x0842); // Clean dark card surface

    if (isFocused) {
        tft.drawRoundRect(sx, sy, sw, sh, 3, 0xFD40); // Amber glow
        tft.drawRoundRect(sx + 1, sy + 1, sw - 2, sh - 2, 2, 0xFFFF); // Crisp inner highlight
        tft.setFont(NULL);
        tft.setTextSize(1);
        tft.setTextColor(0xFFFF);
        tft.setCursor(sx + 16, sy + 4);
        tft.print("> SETTINGS <");
    } else {
        tft.drawRoundRect(sx, sy, sw, sh, 3, 0x39E7); // Subtle metallic border
        tft.setFont(NULL);
        tft.setTextSize(1);
        tft.setTextColor(0xAD55); // Soft label
        tft.setCursor(sx + 12, sy + 4);
        tft.print("SETTINGS: ");
        if (displayMode == 0) { // HYBRID
            tft.setTextColor(0x07E0); // Bright emerald green
            tft.print("HYBRID");
        } else { // FULL
            tft.setTextColor(0x07FF); // Cyan
            tft.print("FULL");
        }
    }
}

void RetroDisplay::drawSettingsMenu(Adafruit_ST7735 &tft, int menuSelection, uint8_t savedMode) {
    int16_t mx = 10;
    int16_t my = 46;
    int16_t mw = 108;
    int16_t mh = 68;

    tft.fillRoundRect(mx, my, mw, mh, 4, 0x0000); // Solid black backing
    tft.drawRoundRect(mx, my, mw, mh, 4, 0xFD40); // Amber primary frame
    tft.drawRoundRect(mx + 1, my + 1, mw - 2, mh - 2, 3, 0x4208); // Subtle inner bevel

    tft.setFont(NULL);
    tft.setTextSize(1);
    tft.setTextColor(0xFD40);
    tft.setCursor(mx + 18, my + 6);
    tft.print("DISPLAY MODE");

    tft.drawFastHLine(mx + 6, my + 17, mw - 12, 0x5AEB); // Accent divider

    // Option 0: HYBRID
    int y0 = my + 23;
    bool isSel0 = (menuSelection == 0);
    bool isSaved0 = (savedMode == 0);
    if (isSel0) {
        tft.fillRoundRect(mx + 6, y0 - 2, mw - 12, 12, 2, 0x2104);
        tft.setTextColor(0xFFFF);
        tft.setCursor(mx + 10, y0);
        tft.print(isSaved0 ? "> (*) HYBRID" : "> ( ) HYBRID");
    } else {
        tft.setTextColor(isSaved0 ? 0x07E0 : 0xAD55);
        tft.setCursor(mx + 10, y0);
        tft.print(isSaved0 ? "  (*) HYBRID" : "  ( ) HYBRID");
    }

    // Option 1: FULL GAME
    int y1 = my + 37;
    bool isSel1 = (menuSelection == 1);
    bool isSaved1 = (savedMode == 1);
    if (isSel1) {
        tft.fillRoundRect(mx + 6, y1 - 2, mw - 12, 12, 2, 0x2104);
        tft.setTextColor(0xFFFF);
        tft.setCursor(mx + 10, y1);
        tft.print(isSaved1 ? "> (*) FULL GAME" : "> ( ) FULL GAME");
    } else {
        tft.setTextColor(isSaved1 ? 0x07E0 : 0xAD55);
        tft.setCursor(mx + 10, y1);
        tft.print(isSaved1 ? "  (*) FULL GAME" : "  ( ) FULL GAME");
    }

    // Navigation hints
    tft.setTextColor(0x7BEF);
    tft.setCursor(mx + 10, my + 53);
    tft.print("[CLICK]OK [BACK]");
}
