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
