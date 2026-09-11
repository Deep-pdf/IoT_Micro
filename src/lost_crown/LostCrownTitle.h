/**
 * LostCrownTitle.h
 *
 * Modular Main Title Screen for Lost Crown on the 128x160 ST7735 TFT display.
 * Displays the full artwork with LOST CROWN logo, resting warrior, and
 * the 5-item menu with CONTINUE selected by default.
 */
#pragma once

#include <Adafruit_ST7735.h>
#include "LostCrownConfig.h"

namespace LostCrownTitle {
  // Initializes and renders the full main title screen and menu
  void begin(Adafruit_ST7735 &tft);

  // Per-frame non-blocking update hook
  void update(Adafruit_ST7735 &tft);

  // Full redraw of background + menu + highlight
  void drawMainTitle(Adafruit_ST7735 &tft);

  // Draws the static 128x160 background asset (logo, scenery, resting warrior)
  void drawMainTitleBackground(Adafruit_ST7735 &tft);

  // Draws all 5 menu options with drop-shadows
  void drawMenu(Adafruit_ST7735 &tft);

  // Renders the highlight (arrow indicator and gold color) for the specified menu option
  void drawMenuHighlight(Adafruit_ST7735 &tft, uint8_t index);

  // Menu state accessors (ready for future joystick navigation)
  void setSelectedItem(uint8_t index);
  uint8_t getSelectedItem();
}
