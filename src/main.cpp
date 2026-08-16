#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <WiFi.h>
#include "mono.h"
#include "MikodacsPCS8pt7b.h"
#include "Dream_Orphans_Bd6pt7b.h"
#include "home_screen.h"
#include "config.h"

#define TFT_CS   5
#define TFT_DC   2
#define TFT_RST  4

// Joystick Pins
#define JOY_X  34
#define JOY_Y  35
#define JOY_SW 32

Adafruit_ST7735 tft(TFT_CS, TFT_DC, TFT_RST);

// Global States
ScreenState currentScreen = STATE_HOME;
FocusedElement currentFocus = FOCUS_QUOTE_CARD;

bool lastWiFiConnected = false;
unsigned long lastUpdateTick = 0;

// Joystick control state
bool joystickCentered = true;
unsigned long lastBtnTime = 0;

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

// Handles joystick click (SW button pressed)
void handleButtonPress() {
  if (currentScreen == STATE_HOME) {
    // Click only triggers if "Maan ki Baat" is focused
    if (currentFocus == FOCUS_QUOTE_CARD) {
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
  } else if (currentScreen == STATE_QUOTE) {
    // Return back to Home Screen
    currentScreen = STATE_HOME;

    // Restore original layout (draws the same quote card text)
    drawHomeScreen(tft);

    // Forces immediate clock / day/date redraw on next tick
    invalidateTimeCache();

    // Redraw highlight state
    drawFocusHighlight(tft, currentFocus, true);

    // Redraw connection status
    bool currentWiFiConnected = (WiFi.status() == WL_CONNECTED);
    updateWiFiIcon(tft, currentWiFiConnected);
    lastWiFiConnected = currentWiFiConnected;
  }
}

void setup() {
  // Initialize Joystick Pins
  pinMode(JOY_X, INPUT);
  pinMode(JOY_Y, INPUT);
  pinMode(JOY_SW, INPUT_PULLUP);

  // Seed random generator using float read on empty analog pin
  randomSeed(analogRead(36));

  tft.initR(INITR_BLACKTAB);
  tft.setRotation(2);

  // Asynchronously initialize WiFi & NTP timezone (IST = GMT+5:30 = 19800 seconds)
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  WiFi.setAutoReconnect(true);
  configTime(19800, 0, "pool.ntp.org", "time.nist.gov");

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

  // Wait for 4.5 seconds to display the boot screen
  delay(4500);

  // Retro Dither Dissolve Fade-out Transition in 4 phases (remains in Rotation 2)
  uint16_t step_delay = 100; // 100ms per phase (total 400ms transition)
  
  // Phase 1: Clear even X, even Y pixels
  for (int y = 0; y < 160; y += 2) {
    for (int x = 0; x < 128; x += 2) {
      tft.drawPixel(x, y, ST77XX_BLACK);
    }
  }
  delay(step_delay);

  // Phase 2: Clear odd X, odd Y pixels
  for (int y = 1; y < 160; y += 2) {
    for (int x = 1; x < 128; x += 2) {
      tft.drawPixel(x, y, ST77XX_BLACK);
    }
  }
  delay(step_delay);

  // Phase 3: Clear odd X, even Y pixels
  for (int y = 0; y < 160; y += 2) {
    for (int x = 1; x < 128; x += 2) {
      tft.drawPixel(x, y, ST77XX_BLACK);
    }
  }
  delay(step_delay);

  // Phase 4: Clear even X, odd Y pixels
  for (int y = 1; y < 160; y += 2) {
    for (int x = 0; x < 128; x += 2) {
      tft.drawPixel(x, y, ST77XX_BLACK);
    }
  }
  delay(step_delay);

  // Select a random quote from the library
  selectRandomQuote();

  // Draw the Initial Home Screen UI (Wi-Fi icon begins as White)
  drawHomeScreen(tft);
  
  // Draw initial highlight around "Maan ki Baat" card
  drawFocusHighlight(tft, currentFocus, true);

  // Cache initial connection status
  lastWiFiConnected = (WiFi.status() == WL_CONNECTED);
  updateWiFiIcon(tft, lastWiFiConnected);
}

void loop() {
  // 1. Process Button Clicks (Non-blocking debouncer)
  bool currentBtnState = (digitalRead(JOY_SW) == LOW);
  if (currentBtnState && (millis() - lastBtnTime > 300)) {
    lastBtnTime = millis();
    handleButtonPress();
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

  // 3. Process background updates (Wi-Fi state and SNTP time tracking)
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

    // Process clock updates (Only visible in HOME state)
    if (currentScreen == STATE_HOME) {
      updateTimeAndDate(tft);
    }
  }
}