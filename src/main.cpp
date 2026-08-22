#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <WiFi.h>
#include "home_screen.h"
#include "config.h"
#include "button.h"
#include "Dream_Orphans_Bd6pt7b.h"
#include "pixel_wars/PixelWars.h"
#include "pixel_wars/PixelWarsShared.h"
#include "pixel_wars/PixelWarsMenu.h"

#define TFT_CS   5
#define TFT_DC   2
#define TFT_RST  4

// Joystick Pins
#define JOY_X  34
#define JOY_Y  35
#define JOY_SW 32 // Joystick switch pin (remains physically connected, action is disabled)

Adafruit_ST7735 tft(TFT_CS, TFT_DC, TFT_RST);

// Global States
ScreenState currentScreen = STATE_HOME;
FocusedElement currentFocus = FOCUS_QUOTE_CARD;

bool lastWiFiConnected = false;
unsigned long lastUpdateTick = 0;
unsigned long lastQuoteChangeTime = 0;

// Joystick control state
bool joystickCentered = true;

enum JoyDirection {
  DIR_NONE,
  DIR_UP,
  DIR_DOWN,
  DIR_LEFT,
  DIR_RIGHT
};

// Handles navigation state transitions between selectable Home Screen elements
void handleNavigation(JoyDirection dir) {
  if (currentScreen != STATE_HOME) return;

  FocusedElement nextFocus = currentFocus;

  if (currentFocus == FOCUS_QUOTE_CARD) {
    if (dir == DIR_DOWN) {
      nextFocus = FOCUS_PIXEL_WARS;
    }
  } else if (currentFocus == FOCUS_PIXEL_WARS) {
    if (dir == DIR_UP) {
      nextFocus = FOCUS_QUOTE_CARD;
    } else if (dir == DIR_RIGHT) {
      nextFocus = FOCUS_AI;
    }
  } else if (currentFocus == FOCUS_AI) {
    if (dir == DIR_UP) {
      nextFocus = FOCUS_QUOTE_CARD;
    } else if (dir == DIR_LEFT) {
      nextFocus = FOCUS_PIXEL_WARS;
    } else if (dir == DIR_RIGHT) {
      nextFocus = FOCUS_LOST_CROWN;
    }
  } else if (currentFocus == FOCUS_LOST_CROWN) {
    if (dir == DIR_UP) {
      nextFocus = FOCUS_QUOTE_CARD;
    } else if (dir == DIR_LEFT) {
      nextFocus = FOCUS_AI;
    }
  }

  // Update visual highlights if focus changed
  if (nextFocus != currentFocus) {
    drawFocusHighlight(tft, currentFocus, false); // Clear old highlight
    currentFocus = nextFocus;
    drawFocusHighlight(tft, currentFocus, true);  // Draw new highlight
  }
}

// Shows the Shayari/quote screen (using the current quote)
void enterMaanKiBaat() {
  currentScreen = STATE_QUOTE;

  // Get the EXACT SAME selected quote from memory
  const Quote* q = getCurrentQuote();
  const char *quote = q ? q->text : "No Quote Loaded";

  // Select random visual theme (0 = Black, 1 = Orange, 2 = White)
  int themeMode = random(3);
  uint16_t bgColor = ST77XX_BLACK;
  uint16_t textColor = ST77XX_WHITE;

  if (themeMode == 1) {
    bgColor = tft.color565(255, 122, 0); // Orange
    textColor = ST77XX_BLACK;
  } else if (themeMode == 2) {
    bgColor = ST77XX_WHITE;
    textColor = ST77XX_BLACK;
  }

  // Draw the fullscreen quote with proper wrapping and centering
  drawFullscreenQuote(tft, quote, bgColor, textColor);
}

// Returns to the Home Screen from Maan Ki Baat
void exitMaanKiBaat() {
  currentScreen = STATE_HOME;

  // Select a new random quote!
  selectRandomQuote();
  lastQuoteChangeTime = millis();

  // Restore original layout (draws the new quote card text)
  drawHomeScreen(tft);

  // Redraw highlight state
  drawFocusHighlight(tft, currentFocus, true);

  // Redraw connection status
  bool currentWiFiConnected = (WiFi.status() == WL_CONNECTED);
  updateWiFiIcon(tft, currentWiFiConnected);
  lastWiFiConnected = currentWiFiConnected;
}

// Launches Pixel Wars app cleanly
void enterPixelWars() {
  currentScreen = STATE_PIXEL_WARS_LOADING;
  pixelWarsLoadStartTime = millis();
  lastProgress = -1;
  drawPixelWarsLoadingScreen(0);
}

// Exits Pixel Wars back to Home Screen
void exitPixelWars() {
  currentScreen = STATE_HOME;

  // Select a new random quote!
  selectRandomQuote();
  lastQuoteChangeTime = millis();

  // Restore original layout
  drawHomeScreen(tft);

  // Redraw highlight state
  drawFocusHighlight(tft, currentFocus, true);

  // Redraw connection status
  bool currentWiFiConnected = (WiFi.status() == WL_CONNECTED);
  updateWiFiIcon(tft, currentWiFiConnected);
  lastWiFiConnected = currentWiFiConnected;
}

void enterAIPlaceholder() {
  Serial.println("Placeholder Action: Open AI Application");
}

void enterLostCrownPlaceholder() {
  Serial.println("Placeholder Action: Enter Lost Crown Game");
}

// Dispatches action based on the current focused menu item
void handleCurrentSelection() {
  if (currentScreen == STATE_HOME) {
    switch (currentFocus) {
      case FOCUS_QUOTE_CARD:
        enterMaanKiBaat();
        break;
      case FOCUS_PIXEL_WARS:
        enterPixelWars();
        break;
      case FOCUS_AI:
        enterAIPlaceholder();
        break;
      case FOCUS_LOST_CROWN:
        enterLostCrownPlaceholder();
        break;
    }
  } else if (currentScreen == STATE_QUOTE) {
    exitMaanKiBaat();
  }
}

void setup() {
  // Initialize Joystick Pins
  pinMode(JOY_X, INPUT);
  pinMode(JOY_Y, INPUT);
  pinMode(JOY_SW, INPUT_PULLUP);

  // Initialize Enter Push Button (GPIO 13)
  setupButton();

  // Seed random generator using float read on empty analog pin
  randomSeed(analogRead(36));

  // Initialize Pixel Wars game data (like loading high scores)
  pixelWars.begin();

  tft.initR(INITR_BLACKTAB);
  tft.sendCommand(0x28);        // Turn display OFF immediately to hide previous frame buffer garbage
  tft.setRotation(2);
  tft.fillScreen(ST77XX_BLACK); // Clear display RAM to black while display is OFF
  tft.sendCommand(0x29);        // Turn display ON now that RAM is cleanly cleared

  // Asynchronously initialize WiFi & NTP timezone (IST = GMT+5:30 = 19800 seconds)
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  WiFi.setAutoReconnect(true);
  configTime(19800, 0, "pool.ntp.org", "time.nist.gov");

  // HELLO DEADDEEP Boot Screen
  uint16_t myOrange = tft.color565(251, 84, 43);
  
  tft.fillScreen(ST77XX_BLACK);
  tft.fillRect(0, 0, 128, 60, myOrange); // Top orange background

  tft.setFont(&Dream_Orphans_Bd6pt7b);
  
  tft.setTextColor(ST77XX_BLACK);
  tft.setTextSize(3);
  tft.setCursor(12, 55);
  tft.print("Hello");

  tft.setTextColor(myOrange);
  tft.setTextSize(2);
  tft.setCursor(12, 84);
  tft.print("Dead");

  tft.setTextColor(myOrange);
  tft.setTextSize(3);
  tft.setCursor(12, 117);
  tft.print("Deep");

  tft.fillRect(13, 129, 68, 2, myOrange);

  tft.fillRect(13, 133, 30, 5, myOrange); // Main bar
  tft.fillRect(44, 133, 3, 5, myOrange);  // Second segment
  tft.fillRect(48, 133, 3, 5, myOrange);  // Third segment

  // Wait for 4.5 seconds to display the boot screen
  delay(4500);

  // Retro Dither Dissolve Fade-out Transition in 4 phases
  uint16_t step_delay = 100;
  
  for (int y = 0; y < 160; y += 2) {
    for (int x = 0; x < 128; x += 2) {
      tft.drawPixel(x, y, ST77XX_BLACK);
    }
  }
  delay(step_delay);

  for (int y = 1; y < 160; y += 2) {
    for (int x = 1; x < 128; x += 2) {
      tft.drawPixel(x, y, ST77XX_BLACK);
    }
  }
  delay(step_delay);

  for (int y = 0; y < 160; y += 2) {
    for (int x = 1; x < 128; x += 2) {
      tft.drawPixel(x, y, ST77XX_BLACK);
    }
  }
  delay(step_delay);

  for (int y = 1; y < 160; y += 2) {
    for (int x = 0; x < 128; x += 2) {
      tft.drawPixel(x, y, ST77XX_BLACK);
    }
  }
  delay(step_delay);

  // Select a random quote from the library
  selectRandomQuote();
  lastQuoteChangeTime = millis();

  // Draw the Initial Home Screen UI (Wi-Fi icon begins as White)
  drawHomeScreen(tft);
  
  // Draw initial highlight around "Maan ki Baat" card
  drawFocusHighlight(tft, currentFocus, true);

  // Cache initial connection status
  lastWiFiConnected = (WiFi.status() == WL_CONNECTED);
  updateWiFiIcon(tft, lastWiFiConnected);
}

void loop() {
  // Update enter button state
  updateButton();

  // Check if current screen is any Pixel Wars state
  if (currentScreen == STATE_PIXEL_WARS_LOADING ||
      currentScreen == STATE_PIXEL_WARS_MENU ||
      currentScreen == STATE_PIXEL_WARS_COUNTDOWN ||
      currentScreen == STATE_PIXEL_WARS_GAMEPLAY ||
      currentScreen == STATE_PIXEL_WARS_HIGH_SCORE) {
    pixelWars.update();
  } else {
    // 1. Process Enter Button Click (Non-blocking debounced edge detection)
    if (isEnterPressed()) {
      handleCurrentSelection();
    }

    // 2. Process joystick movements (Only active on Home Screen)
    if (currentScreen == STATE_HOME) {
      int vrx = analogRead(JOY_X);
      int vry = analogRead(JOY_Y);

      // Joystick deadzone filtering (Centered around 1500..2700)
      bool isCentered = (vrx > 1500 && vrx < 2700 && vry > 1500 && vry < 2700);

      if (isCentered) {
        joystickCentered = true;
      } else if (joystickCentered) {
        // Decode direction based on thresholds
        if (vry < 1000) {
          handleNavigation(DIR_UP);
          joystickCentered = false;
        } else if (vry > 3000) {
          handleNavigation(DIR_DOWN);
          joystickCentered = false;
        } else if (vrx < 1000) {
          handleNavigation(DIR_LEFT);
          joystickCentered = false;
        } else if (vrx > 3000) {
          handleNavigation(DIR_RIGHT);
          joystickCentered = false;
        }
      }
    }

    // 3. Process background updates (Wi-Fi state, SNTP time tracking, and auto-quote rotation)
    unsigned long now = millis();
    if (now - lastUpdateTick >= 500) {
      lastUpdateTick = now;

      // Check Wi-Fi state changes
      bool currentWiFiConnected = (WiFi.status() == WL_CONNECTED);
      if (currentWiFiConnected != lastWiFiConnected) {
        lastWiFiConnected = currentWiFiConnected;
        if (currentScreen == STATE_HOME) {
          updateWiFiIcon(tft, lastWiFiConnected);
        }
      }

      // Process clock and auto-quote updates (Only visible in HOME state)
      if (currentScreen == STATE_HOME) {
        updateTimeAndDate(tft);

        // Auto-change quote every 1 hour (3600000 ms)
        if (now - lastQuoteChangeTime >= 3600000ULL) {
          lastQuoteChangeTime = now;
          selectRandomQuote();
          
          // Redraw Home Screen to show the new quote
          drawHomeScreen(tft);
          drawFocusHighlight(tft, currentFocus, true);
          updateWiFiIcon(tft, WiFi.status() == WL_CONNECTED);
        }
      }
    }
  }
}