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

// Helper function to draw centered text with Dream_Orphans_Bd6pt7b
static void drawCenteredText(Adafruit_ST7735 &tft, const char *text, int16_t y, uint16_t color, uint8_t size) {
  tft.setFont(&Dream_Orphans_Bd6pt7b);
  tft.setTextColor(color);
  tft.setTextSize(size);
  int16_t x1, y1;
  uint16_t w, h;
  tft.getTextBounds(text, 0, y, &x1, &y1, &w, &h);
  int16_t x = (160 - w) / 2 - x1;
  tft.setCursor(x, y);
  tft.print(text);
}

void drawHomeScreen(Adafruit_ST7735 &tft) {
  // Ensure we are in landscape orientation
  tft.setRotation(1);

  // Exact orange color (#FF7A00)
  uint16_t myOrange = tft.color565(255, 122, 0);

  // 1. Clear main screen background (everything from status bar down to bottom bar)
  tft.fillRect(0, 10, 160, 80, ST77XX_BLACK);

  // 2. Draw Top Status Bar (Orange background)
  tft.fillRect(0, 0, 160, 10, myOrange);

  // 2.1 Draw Wi-Fi Icon (White)
  tft.drawBitmap(120, 2, wifi_bitmap, 6, 6, ST77XX_WHITE);

  // 2.2 Draw Battery Icon (White outline & tip, partial fill for 18%)
  tft.drawRect(132, 2, 10, 6, ST77XX_WHITE);
  tft.drawFastVLine(142, 3, 4, ST77XX_WHITE); // Tip
  tft.fillRect(134, 4, 2, 2, ST77XX_WHITE);   // 18% charge indicator (low battery)

  // 2.3 Draw Battery Percentage text
  tft.setFont(&Dream_Orphans_Bd6pt7b);
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(1);
  tft.setCursor(144, 8);
  tft.print("18%");

  // 3. Draw Day Text ("thrusday" centered)
  drawCenteredText(tft, "thrusday", 22, myOrange, 1);

  // 4. Draw Large Clock ("12:40" centered)
  drawCenteredText(tft, "12:40", 52, myOrange, 4);

  // 5. Draw Quote Card
  // White rounded rectangle card
  tft.fillRoundRect(22, 62, 116, 22, 4, ST77XX_WHITE);
  // Centered black text "random quote"
  drawCenteredText(tft, "random quote", 77, ST77XX_BLACK, 1);

  // 6. Draw Bottom Application/Game Area (Orange background)
  tft.fillRect(0, 90, 160, 38, myOrange);

  // 6.1 Draw Three Pixel Art Icons
  // Width: 32px, Height: 32px, Vertically centered (Y = 93)
  tft.drawRGBBitmap(16, 93, icon_pixelwars, 32, 32);
  tft.drawRGBBitmap(64, 93, icon_gpt, 32, 32);
  tft.drawRGBBitmap(112, 93, icon_lostcrown, 32, 32);
}
