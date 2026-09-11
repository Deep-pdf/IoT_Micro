/**
 * LostCrownTitle.cpp
 *
 * Implementation of the Lost Crown Main Title Screen.
 * Displays the 128x160 artwork with the LOST CROWN logo and warrior scene,
 * rendering the 5-item menu with bold text, generous padding, and
 * responsive joystick navigation.
 */

#include "LostCrownTitle.h"
#include "LostCrownAssets.h"
#include "LostCrownConfig.h"
#include <Adafruit_GFX.h>

#define JOY_X 34
#define JOY_Y 35

namespace LostCrownTitle {

// Current selected menu option (defaults to CONTINUE)
static uint8_t selectedMenuItem      = LC_MENU_CONTINUE;
static bool    titleJoystickCentered = true;

void drawMainTitleBackground(Adafruit_ST7735 &tft) {
  tft.drawRGBBitmap(0, 0, image_withoutmenu_pixels, 128, 160);
}

static void drawBoldText(Adafruit_ST7735 &tft, const char *text, int16_t x, int16_t y, uint16_t color, uint16_t shadowColor) {
  // 1. Drop shadow (offset +1, +1 and +2, +1)
  tft.setTextColor(shadowColor);
  tft.setCursor(x + 1, y + 1);
  tft.print(text);
  tft.setCursor(x + 2, y + 1);
  tft.print(text);

  // 2. Bold text (rendered twice at x and x+1 for a thick horizontal stroke)
  tft.setTextColor(color);
  tft.setCursor(x, y);
  tft.print(text);
  tft.setCursor(x + 1, y);
  tft.print(text);
}

static void drawArrowIndicator(Adafruit_ST7735 &tft, int16_t x, int16_t y) {
  // Drop shadow
  tft.fillTriangle(x + 1, y, x + 5, y + 3, x + 1, y + 6, LC_COLOR_SHADOW);
  // Bright golden arrow ▶
  tft.fillTriangle(x, y - 1, x + 4, y + 2, x, y + 5, LC_COLOR_SELECTED);
}

static void restoreRow(Adafruit_ST7735 &tft, uint8_t itemIndex) {
  if (itemIndex >= LC_MENU_COUNT) return;
  int16_t y = LC_TITLE_MENU_Y + (itemIndex * LC_TITLE_MENU_SPACING) - 1;
  int16_t h = LC_TITLE_MENU_SPACING + 2;
  int16_t x = LC_TITLE_ARROW_X - 1;
  int16_t w = 74;

  for (int16_t r = 0; r < h; r++) {
    int16_t py = y + r;
    if (py >= 160) break;
    for (int16_t c = 0; c < w; c++) {
      int16_t px = x + c;
      if (px >= 128) break;
      uint16_t color = pgm_read_word(&image_withoutmenu_pixels[py * 128 + px]);
      tft.drawPixel(px, py, color);
    }
  }
}

static void drawMenuItem(Adafruit_ST7735 &tft, uint8_t i) {
  int16_t y = LC_TITLE_MENU_Y + (i * LC_TITLE_MENU_SPACING);
  int16_t x = LC_TITLE_MENU_X;

  if (i == selectedMenuItem) {
    drawArrowIndicator(tft, LC_TITLE_ARROW_X, y + 1);
    drawBoldText(tft, LC_MENU_LABELS[i], x, y, LC_COLOR_SELECTED, LC_COLOR_SHADOW);
  } else {
    drawBoldText(tft, LC_MENU_LABELS[i], x, y, LC_COLOR_UNSELECTED, LC_COLOR_SHADOW);
  }
}

void drawMenu(Adafruit_ST7735 &tft) {
  tft.setFont(NULL); // Reset to default clean GFX 5x7 font
  tft.setTextSize(1);

  for (uint8_t i = 0; i < LC_MENU_COUNT; i++) {
    drawMenuItem(tft, i);
  }
}

void drawMenuHighlight(Adafruit_ST7735 &tft, uint8_t newIndex) {
  if (newIndex >= LC_MENU_COUNT || newIndex == selectedMenuItem) return;
  uint8_t oldIndex = selectedMenuItem;
  selectedMenuItem = newIndex;

  tft.setFont(NULL);
  tft.setTextSize(1);

  // Restore pristine background behind the previously and newly selected rows
  restoreRow(tft, oldIndex);
  restoreRow(tft, newIndex);

  // Redraw the two affected menu lines
  drawMenuItem(tft, oldIndex);
  drawMenuItem(tft, newIndex);
}

void drawMainTitle(Adafruit_ST7735 &tft) {
  drawMainTitleBackground(tft);
  drawMenu(tft);
}

void begin(Adafruit_ST7735 &tft) {
  selectedMenuItem      = LC_MENU_CONTINUE;
  titleJoystickCentered = true;
  drawMainTitle(tft);
}

void update(Adafruit_ST7735 &tft) {
  // Handle joystick navigation (UP / DOWN)
  int vrx = analogRead(JOY_X);
  int vry = analogRead(JOY_Y);

  // Joystick deadzone filtering (Centered around 1500..2700)
  bool isCentered = (vrx > 1500 && vrx < 2700 && vry > 1500 && vry < 2700);

  if (isCentered) {
    titleJoystickCentered = true;
  } else if (titleJoystickCentered) {
    if (vry < 1000) {
      // UP
      uint8_t nextItem = (selectedMenuItem + LC_MENU_COUNT - 1) % LC_MENU_COUNT;
      drawMenuHighlight(tft, nextItem);
      titleJoystickCentered = false;
    } else if (vry > 3000) {
      // DOWN
      uint8_t nextItem = (selectedMenuItem + 1) % LC_MENU_COUNT;
      drawMenuHighlight(tft, nextItem);
      titleJoystickCentered = false;
    }
  }
}

void setSelectedItem(uint8_t index) {
  if (index < LC_MENU_COUNT) {
    selectedMenuItem = index;
  }
}

uint8_t getSelectedItem() {
  return selectedMenuItem;
}

} // namespace LostCrownTitle
