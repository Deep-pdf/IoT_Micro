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

void drawPixelWarsLoadingScreen(int progress);
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
  tft.setRotation(2);

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
    if (elapsed >= 3000) { // 3 seconds loading duration
      progress = 100;
    } else {
      progress = (elapsed * 100) / 3000;
    }
    drawPixelWarsLoadingScreen(progress);
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

static void drawCenteredText(Adafruit_ST7735 &tft, const char *text, int16_t y, uint16_t color, uint8_t size) {
  tft.setFont(&Dream_Orphans_Bd6pt7b);
  tft.setTextColor(color);
  tft.setTextSize(size);
  int16_t x1, y1;
  uint16_t w, h;
  tft.getTextBounds(text, 0, y, &x1, &y1, &w, &h);
  int16_t x = (128 - w) / 2 - x1;
  tft.setCursor(x, y);
  tft.print(text);
}

void drawPixelWarsLoadingScreen(int progress) {
  uint16_t pwOrange = tft.color565(255, 122, 0);
  uint16_t accentRed = tft.color565(255, 50, 50);
  uint16_t darkGray = tft.color565(60, 60, 60);

  if (lastProgress == -1) {
    tft.fillScreen(ST77XX_BLACK);

    // 1. Draw Starfield background
    randomSeed(12345); // Fixed seed for consistent star placement
    for (int i = 0; i < 40; i++) {
      int sx = random(3, 125);
      int sy = random(3, 157);
      // Avoid drawing stars directly inside the main title card and progress container
      if (sy > 40 && sy < 145 && sx > 10 && sx < 118) continue;
      
      uint16_t starColor;
      int r = random(4);
      if (r == 0) starColor = tft.color565(200, 50, 50);      // Red star
      else if (r == 1) starColor = tft.color565(100, 100, 200); // Blue star
      else if (r == 2) starColor = tft.color565(120, 120, 120); // Gray star
      else starColor = ST77XX_WHITE;                           // White star
      tft.drawPixel(sx, sy, starColor);
    }

    // 2. Draw outer boundary frame (dark red with bright red corners)
    uint16_t frameColor = tft.color565(120, 20, 20);
    tft.drawRect(0, 0, 128, 160, frameColor);
    tft.drawRect(1, 1, 126, 158, frameColor);
    
    tft.drawFastHLine(0, 0, 8, accentRed);
    tft.drawFastVLine(0, 0, 8, accentRed);
    tft.drawFastHLine(120, 0, 8, accentRed);
    tft.drawFastVLine(127, 0, 8, accentRed);
    tft.drawFastHLine(0, 159, 8, accentRed);
    tft.drawFastVLine(0, 152, 8, accentRed);
    tft.drawFastHLine(120, 159, 8, accentRed);
    tft.drawFastVLine(127, 152, 8, accentRed);

    // 3. Draw Logo
    tft.drawRGBBitmap(48, 10, icon_pixelwars, 32, 32);

    // 4. Draw Title Card Container
    tft.drawRoundRect(14, 46, 100, 32, 4, darkGray);
    tft.fillRoundRect(15, 47, 98, 30, 4, tft.color565(15, 15, 15));

    // 5. Draw Stacked Title
    drawCenteredText(tft, "PIXEL", 58, ST77XX_WHITE, 1);
    drawCenteredText(tft, "WARS", 72, accentRed, 1);

    // 6. Draw Star separator
    drawCenteredText(tft, "*", 84, accentRed, 1);

    // 7. Draw LOADING...
    drawCenteredText(tft, "LOADING...", 95, ST77XX_WHITE, 1);

    // 8. Draw Progress Bar Container
    tft.drawRoundRect(14, 101, 100, 12, 3, tft.color565(80, 20, 20));

    // 9. Draw Status Capsule
    tft.drawRoundRect(10, 132, 108, 14, 4, darkGray);
    
    // 10. Draw Bottom Text
    drawCenteredText(tft, "|||| ESP32 SYSTEM ||||", 153, darkGray, 1);
  }

  // 11. Update Progress Bar Segments
  int segmentsToFill = progress / 10; // 0 to 10 segments
  static int lastFilledSegments = -1;

  if (segmentsToFill != lastFilledSegments || lastProgress == -1) {
    for (int i = 0; i < 10; i++) {
      uint16_t color = (i < segmentsToFill) ? accentRed : ST77XX_BLACK;
      tft.fillRect(19 + i * 9, 104, 8, 6, color);
    }
    lastFilledSegments = segmentsToFill;
  }

  // 12. Update Percentage text and status text
  if (progress != lastProgress) {
    // Clear old percentage area
    tft.fillRect(40, 116, 48, 14, ST77XX_BLACK);
    char pctBuf[16];
    snprintf(pctBuf, sizeof(pctBuf), "%d%%", progress);
    drawCenteredText(tft, pctBuf, 125, accentRed, 1);

    // Update Status text dynamically based on progress
    tft.fillRect(12, 134, 104, 10, ST77XX_BLACK);
    const char* statusStr;
    if (progress <= 25) statusStr = "LOADING SYSTEM...";
    else if (progress <= 50) statusStr = "ESTABLISHING COMMS";
    else if (progress <= 75) statusStr = "WARM-UP ENGINES";
    else if (progress <= 99) statusStr = "PREPARING BATTLE";
    else statusStr = "READY FOR BATTLE";
    
    drawCenteredText(tft, statusStr, 142, pwOrange, 1);

    lastProgress = progress;
  }
}
#endif