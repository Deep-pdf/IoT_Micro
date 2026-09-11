#pragma once

#include <Adafruit_ST7735.h>

namespace LostCrownLoading {
  void begin(Adafruit_ST7735 &tft);
  // Returns true after the completed screen has been held briefly.
  bool update(Adafruit_ST7735 &tft);
}
