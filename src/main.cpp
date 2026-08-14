#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include "mono.h"
#include "MikodacsPCS8pt7b.h"

#define TFT_CS   5
#define TFT_DC   2
#define TFT_RST  4

Adafruit_ST7735 tft(TFT_CS, TFT_DC, TFT_RST);

void setup() {
  tft.initR(INITR_BLACKTAB);
  tft.setRotation(2);

  tft.fillScreen(ST77XX_ORANGE);

  tft.setFont(&MikodacsPCS8pt7b);
  tft.setTextColor(ST77XX_BLACK);
  tft.setTextSize(1);
  
  // Fits on a single line (approx 118px wide) on the 128px screen
  tft.setCursor(5, 80);
  tft.print("Hello");
  tft.print(" DeadDeep");
}

void loop() {
}