#include "home_screen.h"
#include "Dream_Orphans_Bd6pt7b.h"
#include "icons.h"

// 6x6 Wi-Fi icon bitmap (padded to 8 bits wide per row)
const uint8_t wifi_bitmap[] PROGMEM = {
  0b11111100, // Row 0
  0b00000000, // Row 1
  0b01111000, // Row 2
  0b00000000, // Row 3
  0b00110000, // Row 4
  0b00110000  // Row 5
};

// Helper function to draw centered text with Dream_Orphans_Bd6pt7b (centered on 128 width)
static void drawCenteredText(Adafruit_ST7735 &tft, const char *text, int16_t y, uint16_t color, uint8_t size) {
  tft.setFont(&Dream_Orphans_Bd6pt7b);
  tft.setTextColor(color);
  tft.setTextSize(size);
  int16_t x1, y1;
  uint16_t w, h;
  tft.getTextBounds(text, 0, y, &x1, &y1, &w, &h);
  int16_t x = (128 - w) / 2 - x1;
  tft.setCursor(x, y);
  tft.print(text);
}

void drawHomeScreen(Adafruit_ST7735 &tft) {
  // Ensure we are in Rotation 2 (Portrait)
  tft.setRotation(2);

  // Exact orange color (#FF7A00)
  uint16_t myOrange = tft.color565(255, 122, 0);

  // 1. Clear main screen background (everything from status bar down to bottom bar)
  tft.fillRect(0, 10, 128, 112, ST77XX_BLACK);

  // 2. Draw Top Status Bar (Orange background, width 128)
  tft.fillRect(0, 0, 128, 10, myOrange);

  // 2.1 Draw Wi-Fi Icon (White)
  tft.drawBitmap(100, 2, wifi_bitmap, 6, 6, ST77XX_WHITE);

  // 2.2 Draw Battery Icon (White outline & tip, partial fill for 18%)
  tft.drawRect(112, 2, 10, 6, ST77XX_WHITE);
  tft.drawFastVLine(122, 3, 4, ST77XX_WHITE); // Tip
  tft.fillRect(114, 4, 2, 2, ST77XX_WHITE);   // 18% charge indicator (low battery)

  // 3. Draw Day Text ("thrusday" centered)
  drawCenteredText(tft, "thrusday", 22, myOrange, 1);

  // 4. Draw Large Clock ("12:40" centered, size 3 for 128px screen width, moved upward)
  drawCenteredText(tft, "12:40", 54, myOrange, 3);

  // 5. Draw Quote Card
  // White rounded rectangle card (width 96, height 28, centered horizontally)
  tft.fillRoundRect(16, 78, 96, 28, 4, ST77XX_WHITE);
  // Centered black text "random quote"
  drawCenteredText(tft, "random quote", 96, ST77XX_BLACK, 1);

  // 6. Draw Bottom Application/Game Area (Orange background, starts at Y = 122, height 38)
  tft.fillRect(0, 122, 128, 38, myOrange);

  // 6.1 Draw Three Pixel Art Icons
  // Width: 32px, Height: 32px, Vertically centered (Y = 125)
  // Horizontal layout: X = 8 (Pixel Wars), 48 (AI Icon), 88 (Lost Crown)
  tft.drawRGBBitmap(8, 125, icon_pixelwars, 32, 32);
  tft.drawRGBBitmap(48, 125, icon_gpt, 32, 32);
  tft.drawRGBBitmap(88, 125, icon_lostcrown, 32, 32);
}
