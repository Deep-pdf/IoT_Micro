#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>

#define TFT_CS   5
#define TFT_DC   2
#define TFT_RST  4

Adafruit_ST7735 tft(TFT_CS, TFT_DC, TFT_RST);

void setup() {
  tft.initR(INITR_BLACKTAB);
  tft.setRotation(2);

  tft.fillScreen(ST77XX_BLACK);

  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(1);
  tft.setCursor(30, 50);

  tft.println("Hello World!");
}

void loop() {
}