#ifndef HOME_SCREEN_H
#define HOME_SCREEN_H

#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include "quote_library.h"
#include "config.h"

// Reserved Media Panel Layout (Right Column)
#define MEDIA_PANEL_X  62
#define MEDIA_PANEL_Y  10
#define MEDIA_PANEL_W  66
#define MEDIA_PANEL_H  112

// Selects a new random quote from the database
void selectRandomQuote();

// Returns the currently selected quote
const Quote* getCurrentQuote();

// Screen state definitions
enum ScreenState {
  STATE_HOME,
  STATE_QUOTE,
  STATE_PIXEL_WARS_LOADING,
  STATE_PIXEL_WARS_MENU,
  STATE_PIXEL_WARS_COUNTDOWN,
  STATE_PIXEL_WARS_GAMEPLAY,
  STATE_PIXEL_WARS_HIGH_SCORE,
  STATE_AI,
  STATE_SPOTIFY,
#if ENABLE_LOST_CROWN
  STATE_LOST_CROWN_LOADING,
  STATE_LOST_CROWN_TITLE,
  STATE_LOST_CROWN_GAMEPLAY,
  STATE_LOST_CROWN_LEVEL1,
#endif
  STATE_HACKING_GO,
  STATE_RETRO_ARCADE,
  STATE_FOCUS_TIMER
};

// Sub-pages for Apps / Home Screen
enum AppsPage {
  APPS_PAGE_1 = 0, // Existing Home Screen (Image 1)
  APPS_PAGE_2 = 1  // Apps Grid Screen (Image 2)
};

#if ENABLE_LOST_CROWN
// Selectable home screen elements (Lost Crown enabled)
enum FocusedElement {
  // Page 1 Elements
  FOCUS_QUOTE_CARD = 0,
  FOCUS_PIXEL_WARS = 1,
  FOCUS_AI         = 2,
  FOCUS_LOST_CROWN = 3,

  // Page 2 Elements
  FOCUS_PAGE2_PIXEL_WARS   = 4,
  FOCUS_PAGE2_AI           = 5,
  FOCUS_PAGE2_LOST_CROWN   = 6,
  FOCUS_PAGE2_SPOTIFY      = 7,
  FOCUS_PAGE2_CALCULATOR   = 8,
  FOCUS_PAGE2_HACKING_GO   = 9,
  FOCUS_PAGE2_RETRO_ARCADE = 10,
  FOCUS_PAGE2_FOCUS_TIMER  = 11
};
#else
// Selectable home screen elements (Lost Crown disabled - seamless layout)
enum FocusedElement {
  // Page 1 Elements
  FOCUS_QUOTE_CARD = 0,
  FOCUS_PIXEL_WARS = 1,
  FOCUS_AI         = 2,
  FOCUS_SPOTIFY    = 3,

  // Page 2 Elements (Grid with Focus Timer under Calculator)
  FOCUS_PAGE2_PIXEL_WARS   = 4,
  FOCUS_PAGE2_AI           = 5,
  FOCUS_PAGE2_SPOTIFY      = 6,
  FOCUS_PAGE2_CALCULATOR   = 7,
  FOCUS_PAGE2_HACKING_GO   = 8,
  FOCUS_PAGE2_RETRO_ARCADE = 9,
  FOCUS_PAGE2_FOCUS_TIMER  = 10
};
#endif

// Draws the static home screen elements in landscape orientation (Rotation 1)
void drawHomeScreen(Adafruit_ST7735 &tft);

// Draws the static Page 2 (Apps Grid) elements (Rotation 2)
void drawAppsPage2(Adafruit_ST7735 &tft);

// Updates the Wi-Fi icon color in the status bar (green if connected, white if disconnected)
void updateWiFiIcon(Adafruit_ST7735 &tft, bool connected);

// Updates the Wi-Fi icon color in the status bar on Page 2
void updateWiFiIconPage2(Adafruit_ST7735 &tft, bool connected);

// Updates the clock and day/date elements dynamically if they change
void updateTimeAndDate(Adafruit_ST7735 &tft);

// Draws/clears a visual focus highlight around a Home Screen element
void drawFocusHighlight(Adafruit_ST7735 &tft, FocusedElement element, bool highlighted);

// Formats, auto-wraps, and centers a fullscreen quote on a selected color theme
void drawFullscreenQuote(Adafruit_ST7735 &tft, const char *quote, uint16_t bgColor, uint16_t textColor);

// Forces the clock/date updater to redraw on the next frame
void invalidateTimeCache();

// Redraws the date/day and stacked clock text on top of animation pixels (Z-order)
void redrawHomeScreenText(Adafruit_ST7735 &tft);

#endif // HOME_SCREEN_H
