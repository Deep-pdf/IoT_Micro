#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include "mono.h"
#include "MikodacsPCS8pt7b.h"
#include "Dream_Orphans_Bd6pt7b.h"

#define TFT_CS   5
#define TFT_DC   2
#define TFT_RST  4

Adafruit_ST7735 tft(TFT_CS, TFT_DC, TFT_RST);

void setup() {
  tft.initR(INITR_BLACKTAB);
  tft.setRotation(2);

  // Exact orange color from the image (RGB: 255, 136, 0)
  uint16_t myOrange = tft.color565(251, 84, 43);
  
  // Draw the backgrounds
  tft.fillScreen(ST77XX_BLACK);
  tft.fillRect(0, 0, 128, 60, myOrange); // Top orange background

  // Set the font
  tft.setFont(&Dream_Orphans_Bd6pt7b);
  
  // 1. Draw "Hello" (black on the orange background)
  tft.setTextColor(ST77XX_BLACK);
  tft.setTextSize(3);
  tft.setCursor(12, 55); // Baseline at Y=60 allows the bottom of the letters to truncate at the boundary
  tft.print("Hello");

  // 2. Draw "Dead" (orange on the black background)
  tft.setTextColor(myOrange);
  tft.setTextSize(2);
  tft.setCursor(12, 84);
  tft.print("Dead");

  // 3. Draw "Deep" (orange on the black background)
  tft.setTextColor(myOrange);
  tft.setTextSize(3);
  tft.setCursor(12, 117);
  tft.print("Deep");

  // 4. Draw the thin orange line below "Deep"
  tft.fillRect(13, 129, 68, 2, myOrange);

  // 5. Draw the thick orange bar with the two vertical black slits near the right side
  tft.fillRect(13, 133, 30, 5, myOrange); // Main bar
  tft.fillRect(44, 133, 3, 5, myOrange);  // Second segment
  tft.fillRect(48, 133, 3, 5, myOrange);  // Third segment
}

void loop() {
}