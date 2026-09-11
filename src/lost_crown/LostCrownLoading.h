#pragma once

#include <Adafruit_ST7735.h>

namespace LostCrownLoading {
  // Initializes and renders the static background, empty progress bar, and editable text
  void begin(Adafruit_ST7735 &tft);

  // Updates animations (flame, villain eye, cape, water, progress bar) non-blockingly using millis().
  // Returns true after the progress bar reaches 100% and holds for LC_COMPLETE_HOLD_MS.
  bool update(Adafruit_ST7735 &tft);

  // Resets internal state
  void reset();
}
