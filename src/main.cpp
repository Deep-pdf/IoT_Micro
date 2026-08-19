#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <WiFi.h>
#include "mono.h"
#include "MikodacsPCS8pt7b.h"
#include "Dream_Orphans_Bd6pt7b.h"
#include "home_screen.h"
#include "config.h"
#include "button.h"
#include "icons.h"

// ===== PIXEL WARS DEVELOPMENT MODE =====
#define PIXEL_WARS_DEV_MODE true

#if defined(PIXEL_WARS_DEV_MODE) && PIXEL_WARS_DEV_MODE == true
static unsigned long pixelWarsLoadStartTime = 0;
static int lastProgress = -1;
static int pwMenuSelectedIndex = 0;
static int lastPwMenuSelectedIndex = -1;

void drawPixelWarsLoadingScreen(int progress);
void drawPixelWarsStartMenu();
void navigatePixelWarsMenu(int direction);
void handlePixelWarsMenuSelection();
#endif


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

// Returns to the Home Screen
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

// Reusable placeholder actions for future apps/games
void enterPixelWarsPlaceholder() {
  Serial.println("Placeholder Action: Enter Pixel Wars Game");
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
        enterPixelWarsPlaceholder();
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

  tft.initR(INITR_BLACKTAB);
  tft.sendCommand(0x28);        // Turn display OFF immediately to hide previous frame buffer garbage
  tft.setRotation(2);
  tft.fillScreen(ST77XX_BLACK); // Clear display RAM to black while display is OFF
  tft.sendCommand(0x29);        // Turn display ON now that RAM is cleanly cleared

  // Asynchronously initialize WiFi & NTP timezone (IST = GMT+5:30 = 19800 seconds)
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  WiFi.setAutoReconnect(true);
  configTime(19800, 0, "pool.ntp.org", "time.nist.gov");

  // ===== PIXEL WARS DEVELOPMENT MODE =====
#if defined(PIXEL_WARS_DEV_MODE) && PIXEL_WARS_DEV_MODE == true
  // In development mode, we bypass the standard boot screen and delay
#else
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
#endif

  // Select a random quote from the library
  selectRandomQuote();
  lastQuoteChangeTime = millis();

  // ===== PIXEL WARS DEVELOPMENT MODE =====
#if defined(PIXEL_WARS_DEV_MODE) && PIXEL_WARS_DEV_MODE == true
  currentScreen = STATE_PIXEL_WARS_LOADING;
  pixelWarsLoadStartTime = millis();
  drawPixelWarsLoadingScreen(0);
#else
  // Draw the Initial Home Screen UI (Wi-Fi icon begins as White)
  drawHomeScreen(tft);
  
  // Draw initial highlight around "Maan ki Baat" card
  drawFocusHighlight(tft, currentFocus, true);

  // Cache initial connection status
  lastWiFiConnected = (WiFi.status() == WL_CONNECTED);
  updateWiFiIcon(tft, lastWiFiConnected);
#endif
}

void loop() {
  // Update enter button state
  updateButton();

  // ===== PIXEL WARS DEVELOPMENT MODE =====
#if defined(PIXEL_WARS_DEV_MODE) && PIXEL_WARS_DEV_MODE == true
  if (currentScreen == STATE_PIXEL_WARS_LOADING) {
    unsigned long elapsed = millis() - pixelWarsLoadStartTime;
    int progress = 0;
    if (elapsed >= 3000) {
      progress = 100;
      drawPixelWarsLoadingScreen(progress);
      
      // Hold the completed loading screen for 300ms
      if (elapsed >= 3300) {
        currentScreen = STATE_PIXEL_WARS_MENU;
        pwMenuSelectedIndex = 0;
        lastPwMenuSelectedIndex = -1;
        drawPixelWarsStartMenu();
      }
    } else {
      progress = (elapsed * 100) / 3000;
      drawPixelWarsLoadingScreen(progress);
    }
    return;
  }

  if (currentScreen == STATE_PIXEL_WARS_MENU) {
    // 1. Handle selection
    if (isEnterPressed()) {
      handlePixelWarsMenuSelection();
    }

    // 2. Handle navigation
    int vrx = analogRead(JOY_X);
    int vry = analogRead(JOY_Y);

    // Joystick deadzone filtering
    bool isCentered = (vrx > 1500 && vrx < 2700 && vry > 1500 && vry < 2700);

    if (isCentered) {
      joystickCentered = true;
    } else if (joystickCentered) {
      if (vry < 1000) {
        navigatePixelWarsMenu(-1); // UP
        joystickCentered = false;
      } else if (vry > 3000) {
        navigatePixelWarsMenu(1);  // DOWN
        joystickCentered = false;
      }
    }
    return;
  }
#else
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
#endif
}

// ===== PIXEL WARS DEVELOPMENT MODE =====
#if defined(PIXEL_WARS_DEV_MODE) && PIXEL_WARS_DEV_MODE == true

static void drawCenteredPWText(Adafruit_ST7735 &tft, const char *text, int16_t y, uint16_t color, uint8_t size, bool bold = false) {
  tft.setFont(NULL); // Clear custom font to use default GFX font
  tft.setTextColor(color);
  tft.setTextSize(size);
  // Default GFX font width is 5px, plus 1px character spacing = 6px per character at size 1
  int16_t w = strlen(text) * 6 * size - 1;
  int16_t x = (128 - w) / 2;
  tft.setCursor(x, y);
  tft.print(text);
  if (bold) {
    tft.setCursor(x + 1, y);
    tft.print(text);
    tft.setCursor(x, y + 1);
    tft.print(text);
    tft.setCursor(x + 1, y + 1);
    tft.print(text);
  }
}

// Icon drawing helper primitives
static void drawTargetIcon(int16_t x, int16_t y, uint16_t color) {
  tft.drawRect(x + 2, y + 2, 5, 5, color);
  tft.drawPixel(x + 4, y + 4, color);
  tft.drawPixel(x + 4, y, color);
  tft.drawPixel(x + 4, y + 8, color);
  tft.drawPixel(x, y + 4, color);
  tft.drawPixel(x + 8, y + 4, color);
}

static void drawTrophyIcon(int16_t x, int16_t y, uint16_t color) {
  tft.fillRect(x + 2, y, 5, 4, color);
  tft.drawFastHLine(x + 3, y + 4, 3, color);
  tft.drawFastVLine(x + 4, y + 4, 3, color);
  tft.drawFastHLine(x + 2, y + 7, 5, color);
  // Handles
  tft.drawPixel(x + 1, y + 1, color);
  tft.drawPixel(x + 1, y + 2, color);
  tft.drawPixel(x + 7, y + 1, color);
  tft.drawPixel(x + 7, y + 2, color);
}

static void drawMiniShipIcon(int16_t x, int16_t y, uint16_t whiteColor, uint16_t redColor) {
  tft.drawPixel(x + 4, y, whiteColor);
  tft.fillRect(x + 3, y + 1, 3, 4, whiteColor);
  tft.fillRect(x + 1, y + 3, 7, 2, whiteColor);
  tft.drawPixel(x, y + 5, redColor);
  tft.drawPixel(x + 8, y + 5, redColor);
  tft.drawPixel(x + 4, y + 5, redColor); // engine
}

static void drawExitIcon(int16_t x, int16_t y, uint16_t color) {
  tft.drawRect(x, y, 5, 8, color);
  tft.fillRect(x + 2, y + 3, 5, 2, color);
  tft.drawPixel(x + 6, y + 2, color);
  tft.drawPixel(x + 6, y + 5, color);
  tft.drawPixel(x, y + 3, ST77XX_BLACK); // door opening
  tft.drawPixel(x, y + 4, ST77XX_BLACK);
}

static void drawMenuCursor(int16_t rowY, int16_t rowH, uint16_t color) {
  int16_t cy = rowY + rowH / 2;
  tft.drawPixel(5, cy - 2, color);
  tft.drawPixel(5, cy + 2, color);
  tft.drawPixel(6, cy - 1, color);
  tft.drawPixel(6, cy + 1, color);
  tft.drawPixel(7, cy, color);
}

static void drawPixelWarsShip() {
  const uint16_t primaryRed = tft.color565(255, 32, 21);
  const uint16_t secondaryRed = tft.color565(122, 21, 18);
  const uint16_t orangeGlow = tft.color565(255, 74, 31);
  const uint16_t yellowGlow = tft.color565(255, 180, 0);
  const uint16_t mainWhite = tft.color565(243, 237, 224);
  const uint16_t darkGray = tft.color565(36, 43, 49);
  const uint16_t blueGray = tft.color565(82, 103, 121);
  const uint16_t shipBlue = tft.color565(23, 105, 168);

  // 1. Engine Flames (trails extending down-left)
  // Left Engine Flame
  tft.drawLine(30, 138, 10, 153, primaryRed);
  tft.drawLine(31, 139, 11, 154, orangeGlow);
  tft.drawLine(32, 140, 12, 155, yellowGlow);

  // Right Engine Flame
  tft.drawLine(62, 145, 42, 160, primaryRed);
  tft.drawLine(63, 146, 43, 161, orangeGlow);
  tft.drawLine(64, 147, 44, 162, yellowGlow);
  
  // Center Engine Flame
  tft.drawLine(43, 141, 23, 156, primaryRed);
  tft.drawLine(44, 142, 24, 157, orangeGlow);

  // 2. Spaceship body (pointing top-right diagonally)
  // 3-pixel thick white core fuselage
  tft.drawLine(78, 114, 48, 140, mainWhite);
  tft.drawLine(79, 115, 49, 141, mainWhite);
  tft.drawLine(77, 113, 47, 139, mainWhite);

  // Red nose stripes
  tft.drawLine(81, 113, 83, 111, primaryRed);
  tft.drawLine(82, 114, 84, 112, primaryRed);

  // Blue Cockpit glass with a white highlight
  tft.fillRect(70, 120, 4, 4, shipBlue);
  tft.drawPixel(72, 121, ST77XX_WHITE);

  // Swept Wings
  // Left Wing (White body with dark gray accents and red tips)
  tft.drawLine(56, 126, 32, 136, mainWhite);
  tft.drawLine(55, 127, 31, 137, darkGray);
  tft.drawPixel(30, 136, primaryRed);
  tft.drawPixel(31, 137, primaryRed);

  // Left Engine cylinder and exhaust nozzle
  tft.fillRect(32, 136, 5, 5, darkGray);
  tft.fillRect(31, 137, 2, 3, secondaryRed);

  // Right Wing (White body with dark gray accents and red tips)
  tft.drawLine(66, 124, 74, 146, mainWhite);
  tft.drawLine(65, 125, 73, 147, darkGray);
  tft.drawPixel(75, 147, primaryRed);
  tft.drawPixel(76, 148, primaryRed);

  // Right Engine cylinder and exhaust nozzle
  tft.fillRect(64, 143, 5, 5, darkGray);
  tft.fillRect(63, 144, 3, 2, secondaryRed);

  // Center Engine cylinder
  tft.fillRect(44, 140, 3, 3, darkGray);
}

static void drawPixelWarsPlanet() {
  const uint16_t primaryRed = tft.color565(255, 32, 21);
  const uint16_t blueGray = tft.color565(82, 103, 121);
  const uint16_t darkBlue = tft.color565(16, 25, 35);
  const uint16_t veryDarkBlueGray = tft.color565(17, 24, 32);

  int pcx = 115;
  int pcy = 159;
  int pr = 40;
  for (int py = 123; py <= 159; py++) {
    for (int px = 70; px <= 127; px++) {
      int dx = px - pcx;
      int dy = py - pcy;
      if (dx*dx + dy*dy <= pr*pr) {
        int distSq = dx*dx + dy*dy;
        if (distSq >= (pr-2)*(pr-2)) {
          if (dx < 0 && dy < 0) tft.drawPixel(px, py, primaryRed); // Red rim light facing ship
          else tft.drawPixel(px, py, blueGray);
        } else if (distSq >= (pr-8)*(pr-8)) {
          tft.drawPixel(px, py, darkBlue);
        } else {
          tft.drawPixel(px, py, veryDarkBlueGray);
        }
      }
    }
  }
}

void drawPixelWarsLoadingScreen(int progress) {
  const uint16_t primaryRed = tft.color565(255, 32, 21);
  const uint16_t secondaryRed = tft.color565(122, 21, 18);
  const uint16_t orangeGlow = tft.color565(255, 74, 31);
  const uint16_t mainWhite = tft.color565(243, 237, 224);
  const uint16_t darkGray = tft.color565(36, 43, 49);
  const uint16_t blueGray = tft.color565(82, 103, 121);
  const uint16_t veryDarkBlueGray = tft.color565(17, 24, 32);

  if (lastProgress == -1) {
    tft.fillScreen(ST77XX_BLACK);

    // 1. Space Background (Starfield x=0..127, y=15..145)
    randomSeed(12345);
    for (int i = 0; i < 25; i++) {
      int sx = random(6, 122);
      int sy = random(15, 145);
      // Skip center area for logo/loading
      if (sx > 14 && sx < 114 && sy > 39 && sy < 122) continue;
      
      uint16_t color;
      int r = random(3);
      if (r == 0) color = primaryRed;
      else if (r == 1) color = blueGray;
      else color = mainWhite;
      tft.drawPixel(sx, sy, color);
    }
    // Cross-shaped bright stars
    tft.drawFastHLine(14, 25, 3, mainWhite);
    tft.drawFastVLine(15, 24, 3, mainWhite);
    tft.drawPixel(15, 25, orangeGlow);
    
    tft.drawFastHLine(109, 30, 3, mainWhite);
    tft.drawFastVLine(110, 29, 3, mainWhite);
    tft.drawPixel(110, 30, orangeGlow);

    tft.drawFastHLine(11, 130, 3, mainWhite);
    tft.drawFastVLine(12, 129, 3, mainWhite);
    tft.drawPixel(12, 130, orangeGlow);

    // 2. Planet (x = 91–118, y = 13–35)
    int cx = 115;
    int cy = 15;
    int r = 13;
    for (int py = 13; py <= 28; py++) {
      for (int px = 91; px <= 118; px++) {
        int dx = px - cx;
        int dy = py - cy;
        if (dx*dx + dy*dy <= r*r) {
          if (dx*dx + dy*dy < (r-3)*(r-3)) {
            if (dx < -2) tft.drawPixel(px, py, blueGray);
            else if (dx < 4) tft.drawPixel(px, py, veryDarkBlueGray);
            else tft.drawPixel(px, py, ST77XX_BLACK);
          } else {
            if (dx < 0) tft.drawPixel(px, py, tft.color565(150, 180, 200));
            else tft.drawPixel(px, py, veryDarkBlueGray);
          }
        }
      }
    }

    // 3. Outer Frame (left=4, right=123, top=3, bottom=157)
    // Corners
    tft.drawFastHLine(4, 3, 10, blueGray);
    tft.drawFastVLine(4, 3, 10, blueGray);
    tft.drawPixel(5, 4, primaryRed);

    tft.drawFastHLine(114, 3, 10, blueGray);
    tft.drawFastVLine(123, 3, 10, blueGray);
    tft.drawPixel(122, 4, primaryRed);

    tft.drawFastHLine(4, 157, 10, blueGray);
    tft.drawFastVLine(4, 148, 10, blueGray);
    tft.drawPixel(5, 156, primaryRed);

    tft.drawFastHLine(114, 157, 10, blueGray);
    tft.drawFastVLine(123, 148, 10, blueGray);
    tft.drawPixel(122, 156, primaryRed);

    // Broken sides
    tft.drawFastVLine(4, 40, 80, blueGray);
    tft.drawFastVLine(123, 40, 80, blueGray);
    tft.drawPixel(4, 25, primaryRed);
    tft.drawPixel(123, 25, primaryRed);
    tft.drawPixel(4, 135, primaryRed);
    tft.drawPixel(123, 135, primaryRed);

    // 4. Top Center Insignia (Y=7, center X=64)
    tft.drawFastHLine(46, 7, 10, secondaryRed);
    tft.drawFastHLine(72, 7, 10, secondaryRed);
    tft.drawPixel(64, 9, primaryRed);
    tft.drawPixel(63, 8, darkGray);
    tft.drawPixel(65, 8, darkGray);
    tft.drawPixel(62, 7, darkGray);
    tft.drawPixel(66, 7, darkGray);
    tft.drawFastHLine(58, 6, 4, secondaryRed);
    tft.drawFastHLine(66, 6, 4, secondaryRed);

    // 5. Main Spaceship (x = 38–90, y = 13–50)
    tft.fillRect(63, 13, 3, 3, mainWhite);
    tft.fillRect(62, 16, 5, 5, mainWhite);
    tft.fillRect(61, 21, 7, 10, mainWhite);
    tft.fillRect(60, 31, 9, 5, darkGray);
    tft.fillRect(63, 17, 3, 3, blueGray);
    tft.drawPixel(64, 18, tft.color565(150, 200, 255));
    
    tft.fillRect(52, 23, 10, 2, mainWhite);
    tft.fillRect(46, 25, 15, 2, mainWhite);
    tft.fillRect(40, 27, 21, 2, mainWhite);
    tft.fillRect(38, 29, 23, 2, mainWhite);
    tft.fillRect(38, 31, 10, 2, darkGray);
    tft.fillRect(39, 33, 5, 2, primaryRed);
    tft.fillRect(48, 27, 2, 4, secondaryRed);
    
    tft.fillRect(66, 23, 10, 2, mainWhite);
    tft.fillRect(67, 25, 15, 2, mainWhite);
    tft.fillRect(67, 27, 21, 2, mainWhite);
    tft.fillRect(67, 29, 23, 2, mainWhite);
    tft.fillRect(80, 31, 10, 2, darkGray);
    tft.fillRect(84, 33, 5, 2, primaryRed);
    tft.fillRect(78, 27, 2, 4, secondaryRed);

    tft.fillRect(62, 36, 5, 2, secondaryRed);
    tft.fillRect(63, 38, 3, 2, orangeGlow);
    tft.drawPixel(64, 40, primaryRed);
    tft.drawPixel(49, 31, orangeGlow);
    tft.drawPixel(78, 31, orangeGlow);

    // 6. PIXEL WARS Logo Card (Y = 39–70)
    // tft.drawRoundRect(14, 44, 100, 34, 4, darkGray);
    // tft.fillRoundRect(15, 45, 98, 32, 4, veryDarkBlueGray);
    drawCenteredPWText(tft, "PIXEL", 47, mainWhite, 2, true);
    drawCenteredPWText(tft, "WARS", 62, primaryRed, 3, true);

    // 7. Logo Decorations
    tft.drawFastHLine(6, 68, 8, primaryRed);
    tft.drawFastHLine(9, 70, 5, primaryRed);
    tft.drawFastHLine(114, 68, 8, primaryRed);
    tft.drawFastHLine(114, 70, 5, primaryRed);
    
    tft.drawPixel(64, 79, primaryRed);
    tft.drawPixel(63, 79, primaryRed);
    tft.drawPixel(65, 79, primaryRed);
    tft.drawPixel(64, 78, primaryRed);
    tft.drawPixel(64, 80, primaryRed);

    // 8. Separator / Subtitle
    drawCenteredPWText(tft, "-  *  -", 84, blueGray, 1, false);

    // 9. LOADING Text
    drawCenteredPWText(tft, "LOADING...", 93, mainWhite, 1, false);

    // 10. Progress Bar Container
    tft.drawRect(16, 103, 96, 9, blueGray);

    // 11. Status Message Capsule
    tft.drawRoundRect(8, 124, 112, 11, 3, darkGray);
    
    // 12. Futuristic Horizon & Perspective Grid
    tft.drawFastHLine(0, 137, 128, blueGray);
    tft.drawFastHLine(0, 139, 128, veryDarkBlueGray);
    tft.drawFastHLine(0, 142, 128, darkGray);
    tft.drawFastHLine(0, 146, 128, darkGray);
    tft.drawFastHLine(0, 151, 128, blueGray);
    
    tft.drawLine(64, 137, -10, 155, veryDarkBlueGray);
    tft.drawLine(64, 137, 15, 155, darkGray);
    tft.drawLine(64, 137, 40, 155, darkGray);
    tft.drawLine(64, 137, 55, 155, blueGray);
    tft.drawLine(64, 137, 73, 155, blueGray);
    tft.drawLine(64, 137, 88, 155, darkGray);
    tft.drawLine(64, 137, 113, 155, darkGray);
    tft.drawLine(64, 137, 138, 155, veryDarkBlueGray);
    
    tft.drawPixel(64, 137, orangeGlow);
    tft.drawPixel(63, 137, primaryRed);
    tft.drawPixel(65, 137, primaryRed);

    // 13. Bottom System Text
    drawCenteredPWText(tft, "||||  ESP32 SYSTEM  ||||", 153, blueGray, 1, false);
  }

  // 14. Update Progress Bar Segments (10 segments)
  int segmentsToFill = progress / 10;
  static int lastFilledSegments = -1;
  if (segmentsToFill != lastFilledSegments || lastProgress == -1) {
    for (int i = 0; i < 10; i++) {
      uint16_t color = (i < segmentsToFill) ? primaryRed : ST77XX_BLACK;
      tft.fillRect(20 + i * 8, 105, 7, 5, color);
    }
    lastFilledSegments = segmentsToFill;
  }

  // 15. Update Percentage and Status text
  if (progress != lastProgress) {
    // Clear old percentage area
    tft.fillRect(48, 114, 32, 8, ST77XX_BLACK);
    char pctBuf[16];
    snprintf(pctBuf, sizeof(pctBuf), "%d%%", progress);
    drawCenteredPWText(tft, pctBuf, 114, orangeGlow, 1, false);

    // Update status text
    tft.fillRect(10, 126, 108, 7, ST77XX_BLACK);
    const char* statusStr;
    if (progress <= 25) statusStr = "SYSTEM BOOT...";
    else if (progress <= 50) statusStr = "ESTABLISHING COMMS";
    else if (progress <= 75) statusStr = "WARMING ENGINES";
    else if (progress <= 99) statusStr = "WEAPONS READY";
    else statusStr = "READY FOR BATTLE";
    drawCenteredPWText(tft, statusStr, 126, orangeGlow, 1, false);

    lastProgress = progress;
  }
}

void drawPixelWarsStartMenu() {
  const uint16_t primaryRed = tft.color565(255, 32, 21);
  const uint16_t secondaryRed = tft.color565(122, 21, 18);
  const uint16_t orangeGlow = tft.color565(255, 74, 31);
  const uint16_t mainWhite = tft.color565(243, 237, 224);
  const uint16_t darkGray = tft.color565(36, 43, 49);
  const uint16_t blueGray = tft.color565(82, 103, 121);
  const uint16_t veryDarkBlueGray = tft.color565(17, 24, 32);

  const char* menuItems[] = {
    "START GAME",
    "HIGH SCORE",
    "CUSTOMIZE",
    "EXIT"
  };

  int rowYs[] = { 64, 78, 92, 106 };
  int rowHs[] = { 12, 12, 12, 12 };

  if (lastPwMenuSelectedIndex == -1) {
    tft.fillScreen(ST77XX_BLACK);
    
    // 1. Space Background (Starfield + Meteors)
    randomSeed(12345);
    for (int i = 0; i < 20; i++) {
      int sx = random(6, 122);
      int sy = random(15, 145);
      // Skip menu area
      if (sx > 8 && sx < 120 && sy > 58 && sy < 115) continue;
      
      uint16_t color;
      int r = random(3);
      if (r == 0) color = primaryRed;
      else if (r == 1) color = blueGray;
      else color = mainWhite;
      tft.drawPixel(sx, sy, color);
    }

    // Glowing cross-shaped stars
    // Star 1 (Top Left)
    tft.drawFastHLine(14, 25, 3, mainWhite);
    tft.drawFastVLine(15, 24, 3, mainWhite);
    tft.drawPixel(15, 25, orangeGlow);
    
    // Star 2 (Top Right)
    tft.drawFastHLine(109, 30, 3, mainWhite);
    tft.drawFastVLine(110, 29, 3, mainWhite);
    tft.drawPixel(110, 30, orangeGlow);

    // Star 3 (Bottom Left)
    tft.drawFastHLine(11, 120, 3, mainWhite);
    tft.drawFastVLine(12, 119, 3, mainWhite);
    tft.drawPixel(12, 120, orangeGlow);

    // Shooting meteors (diagonal streaks)
    tft.drawLine(18, 70, 22, 74, blueGray);
    tft.drawPixel(18, 70, mainWhite);

    tft.drawLine(102, 65, 106, 69, primaryRed);
    tft.drawPixel(102, 65, orangeGlow);

    // 2. Top Center Insignia (Y=8, center X=64, completed circle crosshair)
    tft.drawFastHLine(46, 8, 10, secondaryRed);
    tft.drawFastHLine(72, 8, 10, secondaryRed);
    tft.drawCircle(64, 8, 3, primaryRed);
    tft.drawFastHLine(58, 8, 3, primaryRed);
    tft.drawFastHLine(68, 8, 3, primaryRed);
    tft.drawFastVLine(64, 2, 3, primaryRed);
    tft.drawFastVLine(64, 12, 3, primaryRed);
    tft.drawPixel(64, 8, primaryRed);

    // 3. PIXEL WARS Title Card (Y = 16–42, borderless)
    drawCenteredPWText(tft, "PIXEL", 16, mainWhite, 2, true);
    drawCenteredPWText(tft, "WARS", 31, primaryRed, 3, true);

    // 4. Logo Separator (completed circle crosshair at Y=49)
    tft.drawFastHLine(14, 55, 43, darkGray);
    tft.drawFastHLine(71, 55, 43, darkGray);
    tft.drawCircle(64, 55, 3, primaryRed);
    tft.drawPixel(64, 55, primaryRed);
    tft.drawPixel(64, 51, primaryRed);
    tft.drawPixel(64, 59, primaryRed);
    tft.drawPixel(60, 55, primaryRed);
    tft.drawPixel(68, 55, primaryRed);

    // 5. Spaceship Scene (Planet + Ship + Trails)
    drawPixelWarsPlanet();
    drawPixelWarsShip();

    // 6. Version Text (Y=155)
    // drawCenteredPWText(tft, "-- v1.0.0 --", 155, primaryRed, 1, false);
  }

  // Draw/update menu rows dynamically (no flicker)
  for (int i = 0; i < 4; i++) {
    int16_t rowY = rowYs[i];
    int16_t rowH = rowHs[i];
    bool isSelected = (i == pwMenuSelectedIndex);
    
    // Clear only this row's box and the left cursor area to prevent ghosting
    tft.fillRect(4, rowY, 114, rowH, ST77XX_BLACK);
    
    // Border
    uint16_t borderColor = isSelected ? primaryRed : darkGray;
    tft.drawRoundRect(12, rowY, 104, rowH, 2, borderColor);
    
    // Left Icon
    int16_t iconY = rowY + (rowH - 8) / 2;
    if (i == 0) drawTargetIcon(17, iconY, isSelected ? primaryRed : mainWhite);
    else if (i == 1) drawTrophyIcon(17, iconY, isSelected ? primaryRed : mainWhite);
    else if (i == 2) drawMiniShipIcon(17, iconY, isSelected ? primaryRed : mainWhite, secondaryRed);
    else if (i == 3) drawExitIcon(17, iconY, isSelected ? primaryRed : mainWhite);

    // Menu Text (Using normal default GFX font)
    tft.setFont(NULL);
    tft.setTextSize(1);
    tft.setTextColor(isSelected ? mainWhite : blueGray);
    tft.setCursor(32, rowY + (rowH - 8) / 2);
    tft.print(menuItems[i]);

    // Right Arrow
    int16_t arrowY = rowY + rowH / 2;
    tft.drawPixel(108, arrowY, primaryRed);
    tft.drawPixel(107, arrowY - 1, primaryRed);
    tft.drawPixel(107, arrowY + 1, primaryRed);
    tft.drawPixel(106, arrowY - 2, primaryRed);
    tft.drawPixel(106, arrowY + 2, primaryRed);

    // Left Cursor if selected
    if (isSelected) {
      drawMenuCursor(rowY, rowH, primaryRed);
    }
  }

  lastPwMenuSelectedIndex = pwMenuSelectedIndex;
}

void navigatePixelWarsMenu(int direction) {
  pwMenuSelectedIndex += direction;
  if (pwMenuSelectedIndex < 0) pwMenuSelectedIndex = 3;
  if (pwMenuSelectedIndex > 3) pwMenuSelectedIndex = 0;
  drawPixelWarsStartMenu();
}

void handlePixelWarsMenuSelection() {
  Serial.print("Pixel Wars Option Selected: ");
  switch (pwMenuSelectedIndex) {
    case 0:
      Serial.println("START GAME");
      break;
    case 1:
      Serial.println("HIGH SCORE");
      break;
    case 2:
      Serial.println("CUSTOMIZE");
      break;
    case 3:
      Serial.println("EXIT");
      break;
  }
}
#endif