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
#include "ai/AIApp.h"
#include "spotify/SpotifyApp.h"
#include "lost_crown/LostCrownLoading.h"

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
AppsPage currentAppsPage = APPS_PAGE_1;
FocusedElement currentFocus = FOCUS_QUOTE_CARD;

bool lastWiFiConnected = false;
unsigned long lastUpdateTick = 0;
unsigned long lastQuoteChangeTime = 0;

// Joystick control state
bool joystickCentered = true;
bool lostCrownLaunchSignalled = false;

enum JoyDirection {
  DIR_NONE,
  DIR_UP,
  DIR_DOWN,
  DIR_LEFT,
  DIR_RIGHT
};

// Redraws the current active apps page (Page 1 or Page 2)
void redrawCurrentAppsPage() {
  if (currentAppsPage == APPS_PAGE_2) {
    drawAppsPage2(tft);
    drawFocusHighlight(tft, currentFocus, true);
    bool currentWiFiConnected = (WiFi.status() == WL_CONNECTED);
    updateWiFiIconPage2(tft, currentWiFiConnected);
  } else {
    drawHomeScreen(tft);
    drawFocusHighlight(tft, currentFocus, true);
    bool currentWiFiConnected = (WiFi.status() == WL_CONNECTED);
    updateWiFiIcon(tft, currentWiFiConnected);
  }
}

// Handles navigation state transitions between selectable Home & Apps Screen elements
void handleNavigation(JoyDirection dir) {
  if (currentScreen != STATE_HOME) return;

  if (currentAppsPage == APPS_PAGE_1) {
    // -------------------------------------------------------------
    // PAGE 1 NAVIGATION (Existing Home Screen - Image 1)
    // -------------------------------------------------------------
    if (currentFocus == FOCUS_QUOTE_CARD) {
      if (dir == DIR_DOWN) {
        // Move from quote card down to first icon in last row
        drawFocusHighlight(tft, currentFocus, false);
        currentFocus = FOCUS_PIXEL_WARS;
        drawFocusHighlight(tft, currentFocus, true);
      }
    } else if (currentFocus == FOCUS_PIXEL_WARS) {
      if (dir == DIR_UP) {
        drawFocusHighlight(tft, currentFocus, false);
        currentFocus = FOCUS_QUOTE_CARD;
        drawFocusHighlight(tft, currentFocus, true);
      } else if (dir == DIR_RIGHT) {
        drawFocusHighlight(tft, currentFocus, false);
        currentFocus = FOCUS_AI;
        drawFocusHighlight(tft, currentFocus, true);
      } else if (dir == DIR_DOWN) {
        // ON LAST ROW + DOWN AGAIN -> SWITCH TO PAGE 2!
        currentAppsPage = APPS_PAGE_2;
        currentFocus = FOCUS_PAGE2_PIXEL_WARS;
        drawAppsPage2(tft);
        drawFocusHighlight(tft, currentFocus, true);
        bool currentWiFiConnected = (WiFi.status() == WL_CONNECTED);
        updateWiFiIconPage2(tft, currentWiFiConnected);
      }
    } else if (currentFocus == FOCUS_AI) {
      if (dir == DIR_UP) {
        drawFocusHighlight(tft, currentFocus, false);
        currentFocus = FOCUS_QUOTE_CARD;
        drawFocusHighlight(tft, currentFocus, true);
      } else if (dir == DIR_LEFT) {
        drawFocusHighlight(tft, currentFocus, false);
        currentFocus = FOCUS_PIXEL_WARS;
        drawFocusHighlight(tft, currentFocus, true);
      } else if (dir == DIR_RIGHT) {
        drawFocusHighlight(tft, currentFocus, false);
        currentFocus = FOCUS_LOST_CROWN;
        drawFocusHighlight(tft, currentFocus, true);
      } else if (dir == DIR_DOWN) {
        // ON LAST ROW + DOWN AGAIN -> SWITCH TO PAGE 2!
        currentAppsPage = APPS_PAGE_2;
        currentFocus = FOCUS_PAGE2_AI;
        drawAppsPage2(tft);
        drawFocusHighlight(tft, currentFocus, true);
        bool currentWiFiConnected = (WiFi.status() == WL_CONNECTED);
        updateWiFiIconPage2(tft, currentWiFiConnected);
      }
    } else if (currentFocus == FOCUS_LOST_CROWN) {
      if (dir == DIR_UP) {
        drawFocusHighlight(tft, currentFocus, false);
        currentFocus = FOCUS_QUOTE_CARD;
        drawFocusHighlight(tft, currentFocus, true);
      } else if (dir == DIR_LEFT) {
        drawFocusHighlight(tft, currentFocus, false);
        currentFocus = FOCUS_AI;
        drawFocusHighlight(tft, currentFocus, true);
      } else if (dir == DIR_DOWN) {
        // ON LAST ROW + DOWN AGAIN -> SWITCH TO PAGE 2!
        currentAppsPage = APPS_PAGE_2;
        currentFocus = FOCUS_PAGE2_LOST_CROWN;
        drawAppsPage2(tft);
        drawFocusHighlight(tft, currentFocus, true);
        bool currentWiFiConnected = (WiFi.status() == WL_CONNECTED);
        updateWiFiIconPage2(tft, currentWiFiConnected);
      }
    }

  } else if (currentAppsPage == APPS_PAGE_2) {
    // -------------------------------------------------------------
    // PAGE 2 NAVIGATION (Apps Grid - Image 2)
    // -------------------------------------------------------------
    // Row 0: FOCUS_PAGE2_PIXEL_WARS (col 0), FOCUS_PAGE2_AI (col 1), FOCUS_PAGE2_LOST_CROWN (col 2)
    // Row 1: FOCUS_PAGE2_SPOTIFY (col 0), FOCUS_PAGE2_CALCULATOR (col 1)

    if (currentFocus == FOCUS_PAGE2_PIXEL_WARS) {
      if (dir == DIR_UP) {
        // UP FROM PAGE 2 ROW 0 -> RETURN TO PAGE 1!
        currentAppsPage = APPS_PAGE_1;
        currentFocus = FOCUS_PIXEL_WARS;
        drawHomeScreen(tft);
        drawFocusHighlight(tft, currentFocus, true);
        bool currentWiFiConnected = (WiFi.status() == WL_CONNECTED);
        updateWiFiIcon(tft, currentWiFiConnected);
      } else if (dir == DIR_RIGHT) {
        drawFocusHighlight(tft, currentFocus, false);
        currentFocus = FOCUS_PAGE2_AI;
        drawFocusHighlight(tft, currentFocus, true);
      } else if (dir == DIR_DOWN) {
        drawFocusHighlight(tft, currentFocus, false);
        currentFocus = FOCUS_PAGE2_SPOTIFY;
        drawFocusHighlight(tft, currentFocus, true);
      }
    } else if (currentFocus == FOCUS_PAGE2_AI) {
      if (dir == DIR_UP) {
        // UP FROM PAGE 2 ROW 0 -> RETURN TO PAGE 1!
        currentAppsPage = APPS_PAGE_1;
        currentFocus = FOCUS_AI;
        drawHomeScreen(tft);
        drawFocusHighlight(tft, currentFocus, true);
        bool currentWiFiConnected = (WiFi.status() == WL_CONNECTED);
        updateWiFiIcon(tft, currentWiFiConnected);
      } else if (dir == DIR_LEFT) {
        drawFocusHighlight(tft, currentFocus, false);
        currentFocus = FOCUS_PAGE2_PIXEL_WARS;
        drawFocusHighlight(tft, currentFocus, true);
      } else if (dir == DIR_RIGHT) {
        drawFocusHighlight(tft, currentFocus, false);
        currentFocus = FOCUS_PAGE2_LOST_CROWN;
        drawFocusHighlight(tft, currentFocus, true);
      } else if (dir == DIR_DOWN) {
        drawFocusHighlight(tft, currentFocus, false);
        currentFocus = FOCUS_PAGE2_CALCULATOR;
        drawFocusHighlight(tft, currentFocus, true);
      }
    } else if (currentFocus == FOCUS_PAGE2_LOST_CROWN) {
      if (dir == DIR_UP) {
        // UP FROM PAGE 2 ROW 0 -> RETURN TO PAGE 1!
        currentAppsPage = APPS_PAGE_1;
        currentFocus = FOCUS_LOST_CROWN;
        drawHomeScreen(tft);
        drawFocusHighlight(tft, currentFocus, true);
        bool currentWiFiConnected = (WiFi.status() == WL_CONNECTED);
        updateWiFiIcon(tft, currentWiFiConnected);
      } else if (dir == DIR_LEFT) {
        drawFocusHighlight(tft, currentFocus, false);
        currentFocus = FOCUS_PAGE2_AI;
        drawFocusHighlight(tft, currentFocus, true);
      } else if (dir == DIR_DOWN) {
        // Col 2 on Row 1 is empty, move to nearest valid item: Calculator (Col 1)
        drawFocusHighlight(tft, currentFocus, false);
        currentFocus = FOCUS_PAGE2_CALCULATOR;
        drawFocusHighlight(tft, currentFocus, true);
      }
    } else if (currentFocus == FOCUS_PAGE2_SPOTIFY) {
      if (dir == DIR_UP) {
        drawFocusHighlight(tft, currentFocus, false);
        currentFocus = FOCUS_PAGE2_PIXEL_WARS;
        drawFocusHighlight(tft, currentFocus, true);
      } else if (dir == DIR_RIGHT) {
        drawFocusHighlight(tft, currentFocus, false);
        currentFocus = FOCUS_PAGE2_CALCULATOR;
        drawFocusHighlight(tft, currentFocus, true);
      }
    } else if (currentFocus == FOCUS_PAGE2_CALCULATOR) {
      if (dir == DIR_UP) {
        drawFocusHighlight(tft, currentFocus, false);
        currentFocus = FOCUS_PAGE2_AI;
        drawFocusHighlight(tft, currentFocus, true);
      } else if (dir == DIR_LEFT) {
        drawFocusHighlight(tft, currentFocus, false);
        currentFocus = FOCUS_PAGE2_SPOTIFY;
        drawFocusHighlight(tft, currentFocus, true);
      }
    }
  }
}

// Shows the Shayari/quote screen (using the current quote)
void enterMaanKiBaat() {
  clearButtonEvents();
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
  clearButtonEvents();
  currentScreen = STATE_HOME;

  // Select a new random quote!
  selectRandomQuote();
  lastQuoteChangeTime = millis();

  // Restore active page layout
  redrawCurrentAppsPage();
}

// Launches Pixel Wars app cleanly
void enterPixelWars() {
  clearButtonEvents();
  currentScreen = STATE_PIXEL_WARS_LOADING;
  pixelWarsLoadStartTime = millis();
  lastProgress = -1;
  drawPixelWarsLoadingScreen(0);
}

// Exits Pixel Wars back to Home Screen
void exitPixelWars() {
  clearButtonEvents();
  currentScreen = STATE_HOME;

  // Select a new random quote!
  selectRandomQuote();
  lastQuoteChangeTime = millis();

  // Restore active page layout
  redrawCurrentAppsPage();
}

void enterAI() {
  clearButtonEvents();
  currentScreen = STATE_AI;
  AIApp::init(tft);
}

void exitAI() {
  clearButtonEvents();
  currentScreen = STATE_HOME;

  // Select a new random quote!
  selectRandomQuote();
  lastQuoteChangeTime = millis();

  // Restore active page layout
  redrawCurrentAppsPage();
}

void enterLostCrownPlaceholder() {
  clearButtonEvents();
  currentScreen = STATE_LOST_CROWN_LOADING;
  lostCrownLaunchSignalled = false;
  LostCrownLoading::begin(tft);
}

void exitLostCrownLoading() {
  clearButtonEvents();
  currentScreen = STATE_HOME;
  redrawCurrentAppsPage();
}

void launchLostCrownGame() {
  // This preserves the existing Lost Crown launch hook. A gameplay module has
  // not yet been added to this project, so no second game implementation is created here.
  Serial.println("Lost Crown loading complete: game launch hook reached");
}

void enterSpotify() {
  clearButtonEvents();
  currentScreen = STATE_SPOTIFY;
  SpotifyApp::init(tft);
}

void exitSpotify() {
  clearButtonEvents();
  currentScreen = STATE_HOME;

  // Select a new random quote!
  selectRandomQuote();
  lastQuoteChangeTime = millis();

  // Restore active page layout
  redrawCurrentAppsPage();
}

void enterCalculatorPlaceholder() {
  Serial.println("Placeholder Action: Enter Calculator App");
}

// Dispatches action based on the current focused menu item
void handleCurrentSelection() {
  if (currentScreen == STATE_HOME) {
    switch (currentFocus) {
      case FOCUS_QUOTE_CARD:
        enterMaanKiBaat();
        break;
      case FOCUS_PIXEL_WARS:
      case FOCUS_PAGE2_PIXEL_WARS:
        enterPixelWars();
        break;
      case FOCUS_AI:
      case FOCUS_PAGE2_AI:
        enterAI();
        break;
      case FOCUS_LOST_CROWN:
      case FOCUS_PAGE2_LOST_CROWN:
        enterLostCrownPlaceholder();
        break;
      case FOCUS_PAGE2_SPOTIFY:
        enterSpotify();
        break;
      case FOCUS_PAGE2_CALCULATOR:
        enterCalculatorPlaceholder();
        break;
    }
  } else if (currentScreen == STATE_QUOTE) {
    exitMaanKiBaat();
  }
}

void setup() {
  Serial.begin(115200);

  // 1. Pull display reset LOW immediately to blank the panel and hide previous RAM contents
  pinMode(TFT_RST, OUTPUT);
  digitalWrite(TFT_RST, LOW);

  // 2. Initialize display hardware and command it OFF to blank it cleanly
  tft.initR(INITR_BLACKTAB);
  tft.sendCommand(0x28);        // Display OFF
  tft.setRotation(0);
  tft.fillScreen(ST77XX_BLACK); // Clear display RAM to black while OFF

  // 3. Initialize joystick, button, random seed, and Pixel Wars preferences
  pinMode(JOY_X, INPUT);
  pinMode(JOY_Y, INPUT);
  pinMode(JOY_SW, INPUT_PULLUP);
  setupButton();
  randomSeed(analogRead(36));
  pixelWars.begin();

  // 4. Turn display back ON (now cleanly displaying a black screen)
  tft.sendCommand(0x29);

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
  } else if (currentScreen == STATE_AI) {
    AIApp::update(tft);
    if (AIApp::shouldExit()) {
      exitAI();
    }
  } else if (currentScreen == STATE_SPOTIFY) {
    SpotifyApp::update(tft);
    if (SpotifyApp::shouldExit()) {
      exitSpotify();
    }
  } else if (currentScreen == STATE_LOST_CROWN_LOADING) {
    if (isBackPressed()) {
      exitLostCrownLoading();
    } else if (!lostCrownLaunchSignalled && LostCrownLoading::update(tft)) {
      launchLostCrownGame();
      lostCrownLaunchSignalled = true;
    }
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
          if (currentAppsPage == APPS_PAGE_2) {
            updateWiFiIconPage2(tft, lastWiFiConnected);
          } else {
            updateWiFiIcon(tft, lastWiFiConnected);
          }
        }
      }

      // Process clock and auto-quote updates (Only visible in HOME state on Page 1)
      if (currentScreen == STATE_HOME && currentAppsPage == APPS_PAGE_1) {
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
