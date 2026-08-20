#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <WiFi.h>
#include "mono.h"
#include "MikodacsPCS8pt7b.h"
#include "Dream_Orphans_Bd6pt7b.h"
#include "BLADRMF_4pt7b.h"
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

static unsigned long countdownStartTime = 0;
static int lastCountdownNumber = -1;
static float playerX = 56.0f;
static float playerY = 120.0f;
static float prevPlayerX = 56.0f;
static float prevPlayerY = 120.0f;
static unsigned long lastGameplayFrameTime = 0;

#define MAX_PROJECTILES 5
struct Projectile {
  float x;
  float y;
  float prevX;
  float prevY;
  bool active;
};
static Projectile projectiles[MAX_PROJECTILES];

struct Enemy {
  float x;
  float y;
  float prevX;
  float prevY;
  float vx;
  float vy;
  bool active;
  uint8_t type;
  uint8_t moveStyle;
  float movePhase;
  float baseX;
  int8_t driftDir;
  unsigned long nextAttackTime;
  unsigned long nextMoveChangeTime;
  bool bombWarningActive;
  unsigned long bombWarningStartTime;
};
#define MAX_ENEMIES 20
static Enemy enemies[MAX_ENEMIES];

struct EnemyProjectile {
  float x;
  float y;
  float prevX;
  float prevY;
  float vx;
  float vy;
  bool active;
  uint8_t type; // 1 = normal, 2 = angled, 3 = bomb
};
#define MAX_ENEMY_PROJECTILES 12
static EnemyProjectile enemyProjectiles[MAX_ENEMY_PROJECTILES];

// Pacing Variables
static float currentPaceMultiplier = 1.0f;
static float targetPaceMultiplier = 1.0f;
static unsigned long pacePhaseStartTime = 0;
static unsigned long pacePhaseDuration = 0;
static uint8_t currentPacePhase = 1; // 0 = SLOW, 1 = MEDIUM, 2 = FAST

// HUD Variables
static int playerHealth = 3;
static int score = 0;
static int lastDrawHealth = -1;
static int lastDrawScore = -1;

#define HEART_FULL 0
#define HEART_HALF 1
#define HEART_EMPTY 2

// Debug Random Seed Settings
#define PIXEL_WARS_DEBUG_RANDOM false

// Tuning Constants
const unsigned long MIN_ATTACK_DELAY_BASIC = 2500;
const unsigned long MAX_ATTACK_DELAY_BASIC = 6000;
const unsigned long MIN_ATTACK_DELAY_FAST = 1800;
const unsigned long MAX_ATTACK_DELAY_FAST = 4500;
const unsigned long MIN_ATTACK_DELAY_HEAVY = 4000;
const unsigned long MAX_ATTACK_DELAY_HEAVY = 8000;
const unsigned long MIN_ATTACK_DELAY_SPECIAL = 3000;
const unsigned long MAX_ATTACK_DELAY_SPECIAL = 7000;

#define STYLE_STRAIGHT 0
#define STYLE_DRIFT 1
#define STYLE_OSCILLATE 2
#define STYLE_ZIGZAG 3
#define STYLE_STEP 4

struct Explosion {
  int16_t cx;
  int16_t cy;
  unsigned long startTime;
  uint8_t lastStage;
  bool active;
};
#define MAX_EXPLOSIONS 8
static Explosion explosions[MAX_EXPLOSIONS];

// Spawn Manager variables
static unsigned long lastSpawnActionTime = 0;
static int spawnSequenceIndex = 0;
static int randomSpawnCount = 0;

#define STAR_COUNT_FAR 35
#define STAR_COUNT_MID 25
#define STAR_COUNT_NEAR 12

struct Star {
  float x;
  float y;
  float speed;
  uint16_t color;
  uint8_t type; // 0=1x1, 1=2x1, 2=2x2, 3=1x3 streak, 4=cross, 5=red particle
};

static Star farStars[STAR_COUNT_FAR];
static Star midStars[STAR_COUNT_MID];
static Star nearStars[STAR_COUNT_NEAR];

static float STARFIELD_SPEED_FAR = 0.02f;
static float STARFIELD_SPEED_MID = 0.05f;
static float STARFIELD_SPEED_NEAR = 0.12f;

void drawPixelWarsLoadingScreen(int progress);
void drawPixelWarsStartMenu();
void navigatePixelWarsMenu(int direction);
void handlePixelWarsMenuSelection();
static void drawPlayerShip(int16_t px, int16_t py);
static void drawCenteredPWText(Adafruit_ST7735 &tft, const char *text, int16_t y, uint16_t color, uint8_t size, bool bold);
static void spawnProjectile();
static void updateProjectiles(uint32_t dt);
static void clearProjectiles();
static void initStarfield();
static void updateStarfield(uint32_t dt, float speedMultiplier);
static void drawStar(const Star &s);
static void eraseStar(const Star &s);

static void resetEnemySystem();
static void updateEnemies(uint32_t dt);
static void updateExplosions();
static void checkCollisions();
static void updateSpawnManager();
static void spawnFormation(int seqIndex);
static void eraseEnemy(int16_t x, int16_t y, uint8_t type);
static void drawEnemy(int16_t x, int16_t y, uint8_t type, bool flashRed);
static void drawEnemyType1(int16_t x, int16_t y);
static void drawEnemyType2(int16_t x, int16_t y);
static void drawEnemyType3(int16_t x, int16_t y, bool flashRed);
static void drawEnemyType4(int16_t x, int16_t y, bool flashRed);
static void drawExplosionStage(int16_t cx, int16_t cy, uint8_t stage, bool erase);

static void updateGlobalPace(uint32_t dt);
static void spawnEnemyProjectile(float x, float y, float vx, float vy, uint8_t type);
static void updateEnemyProjectiles(uint32_t dt);
static void drawEnemyProjectile(int16_t x, int16_t y, uint8_t type, bool erase);

static void drawHeart(int16_t x, int16_t y, uint8_t state);
static void drawScoreHUD(int scoreVal);
static void drawGameplayHUD();
static void drawHUDSeparator();
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

  if (currentScreen == STATE_PIXEL_WARS_COUNTDOWN) {
    unsigned long elapsed = millis() - countdownStartTime;
    uint16_t primaryRed = tft.color565(255, 32, 21);
    uint16_t orangeGlow = tft.color565(255, 74, 31);
    
    unsigned long now = millis();
    uint32_t dt = now - lastGameplayFrameTime;
    if (dt > 100) dt = 100;
    lastGameplayFrameTime = now;

    // Scroll stars slowly during countdown
    updateStarfield(dt, 0.2f);
    
    if (elapsed < 800) {
      if (lastCountdownNumber != 3) {
        tft.fillRect(44, 68, 40, 24, ST77XX_BLACK);
        drawCenteredPWText(tft, "3", 68, orangeGlow, 3, true);
        lastCountdownNumber = 3;
      }
    } else if (elapsed < 1600) {
      if (lastCountdownNumber != 2) {
        tft.fillRect(44, 68, 40, 24, ST77XX_BLACK);
        drawCenteredPWText(tft, "2", 68, orangeGlow, 3, true);
        lastCountdownNumber = 2;
      }
    } else if (elapsed < 2400) {
      if (lastCountdownNumber != 1) {
        tft.fillRect(44, 68, 40, 24, ST77XX_BLACK);
        drawCenteredPWText(tft, "1", 68, orangeGlow, 3, true);
        lastCountdownNumber = 1;
      }
    } else if (elapsed < 3200) {
      if (lastCountdownNumber != 0) {
        tft.fillRect(34, 68, 60, 24, ST77XX_BLACK);
        drawCenteredPWText(tft, "GO!", 68, primaryRed, 3, true);
        lastCountdownNumber = 0;
      }
    } else {
      // Transition to Gameplay
      currentScreen = STATE_PIXEL_WARS_GAMEPLAY;
      playerX = 56.0f;
      playerY = 120.0f;
      prevPlayerX = 56.0f;
      prevPlayerY = 120.0f;
      lastGameplayFrameTime = millis();
      clearProjectiles(); // Clear active projectiles at startup
      
      // Reset HUD draw flags
      lastDrawHealth = -1;
      lastDrawScore = -1;

      // Erase countdown text box, draw HUD separator, draw HUD, and draw player ship
      tft.fillRect(34, 68, 60, 24, ST77XX_BLACK);
      drawHUDSeparator();
      drawGameplayHUD();
      drawPlayerShip((int16_t)playerX, (int16_t)playerY);
    }
    return;
  }

  if (currentScreen == STATE_PIXEL_WARS_GAMEPLAY) {
    if (isBackPressed()) {
      // Erase player, active projectiles, active enemies, active explosions, active enemy projectiles
      tft.fillRect((int16_t)playerX, (int16_t)playerY, 15, 17, ST77XX_BLACK);
      for (int i = 0; i < MAX_PROJECTILES; i++) {
        if (projectiles[i].active) {
          tft.fillRect((int16_t)projectiles[i].x, (int16_t)projectiles[i].y, 2, 6, ST77XX_BLACK);
        }
      }
      for (int i = 0; i < MAX_ENEMIES; i++) {
        if (enemies[i].active) {
          eraseEnemy((int16_t)enemies[i].x, (int16_t)enemies[i].y, enemies[i].type);
        }
      }
      for (int i = 0; i < MAX_EXPLOSIONS; i++) {
        if (explosions[i].active) {
          drawExplosionStage(explosions[i].cx, explosions[i].cy, explosions[i].lastStage, true);
        }
      }
      for (int i = 0; i < MAX_ENEMY_PROJECTILES; i++) {
        if (enemyProjectiles[i].active) {
          drawEnemyProjectile((int16_t)enemyProjectiles[i].x, (int16_t)enemyProjectiles[i].y, enemyProjectiles[i].type, true);
        }
      }

      resetEnemySystem();
      clearProjectiles();

      currentScreen = STATE_PIXEL_WARS_MENU;
      pwMenuSelectedIndex = 0;
      lastPwMenuSelectedIndex = -1;
      drawPixelWarsStartMenu();
      return;
    }

    unsigned long now = millis();
    unsigned long dt = now - lastGameplayFrameTime;
    if (dt > 100) dt = 100;
    lastGameplayFrameTime = now;

    // Check shoot input
    if (isEnterPressed()) {
      spawnProjectile();
    }

    // 1. Update global gameplay pace
    updateGlobalPace(dt);

    // 2. Update scrolling background (Reinforces pacing)
    updateStarfield(dt, 1.0f * currentPaceMultiplier);

    // 3 & 4. Spawn/update enemies and move them (Paced)
    updateEnemies(dt);

    // 5 & 6. Spawn/update enemy projectiles (Paced)
    updateEnemyProjectiles(dt);

    // 7. Update player movement
    float dx = 0;
    float dy = 0;
    float speed = 0.08f; // pixels per millisecond

    int vrx = analogRead(JOY_X);
    int vry = analogRead(JOY_Y);

    if (vrx < 1500) dx = -speed * dt;
    else if (vrx > 2700) dx = speed * dt;

    if (vry < 1500) dy = -speed * dt;
    else if (vry > 2700) dy = speed * dt;

    playerX += dx;
    playerY += dy;

    if (playerX < 0.0f) playerX = 0.0f;
    if (playerX > 113.0f) playerX = 113.0f;

    if (playerY < 16.0f) playerY = 16.0f;
    if (playerY > 143.0f) playerY = 143.0f;

    // 8. Update player projectiles
    updateProjectiles(dt);

    // 9. Check player projectile -> enemy collision
    checkCollisions();

    // 10. Update visual effects (explosions)
    updateExplosions();

    // 11. Check and trigger spawning patterns / individual enemies
    updateSpawnManager();

    int16_t ix = (int16_t)playerX;
    int16_t iy = (int16_t)playerY;
    int16_t pix = (int16_t)prevPlayerX;
    int16_t piy = (int16_t)prevPlayerY;

    // Redraw player ship if moved or if an active enemy overlaps its bounding box
    bool forcePlayerRedraw = false;
    for (int i = 0; i < MAX_ENEMIES; i++) {
      if (enemies[i].active) {
        float ex = enemies[i].x;
        float ey = enemies[i].y;
        float ew = 9.0f;
        float eh = 9.0f;
        if (enemies[i].type == 2) { ew = 7.0f; eh = 11.0f; }
        else if (enemies[i].type == 3) { ew = 13.0f; eh = 11.0f; }
        else if (enemies[i].type == 4) { ew = 9.0f; eh = 9.0f; }
        
        // Overlap test with player ship bounding box (ix, iy, 15, 17)
        if (ex < playerX + 15.0f && ex + ew > playerX && ey < playerY + 17.0f && ey + eh > playerY) {
          forcePlayerRedraw = true;
          break;
        }
      }
    }

    if (ix != pix || iy != piy || forcePlayerRedraw) {
      tft.fillRect(pix, piy, 15, 17, ST77XX_BLACK);
      drawPlayerShip(ix, iy);
      prevPlayerX = playerX;
      prevPlayerY = playerY;
    }

    drawGameplayHUD();
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

static void drawCenteredPWText(Adafruit_ST7735 &tft, const char *text, int16_t y, uint16_t color, uint8_t size, bool bold) {
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

static void drawPlayerShip(int16_t px, int16_t py) {
  const uint16_t primaryRed = tft.color565(255, 32, 21);
  const uint16_t orangeGlow = tft.color565(255, 74, 31);
  const uint16_t yellowGlow = tft.color565(255, 180, 0);
  const uint16_t mainWhite = tft.color565(243, 237, 224);
  const uint16_t darkGray = tft.color565(36, 43, 49);
  const uint16_t shipBlue = tft.color565(23, 105, 168);

  // 1. Nose tip and centerline fuselage
  tft.drawLine(px + 7, py, px + 7, py + 14, mainWhite);
  tft.drawLine(px + 6, py + 3, px + 6, py + 14, mainWhite);
  tft.drawLine(px + 8, py + 3, px + 8, py + 14, mainWhite);
  tft.drawPixel(px + 7, py, primaryRed); // Red nose tip
  tft.drawPixel(px + 7, py + 1, primaryRed);

  // 2. Cockpit window
  tft.fillRect(px + 6, py + 5, 3, 4, shipBlue);
  tft.drawPixel(px + 7, py + 6, ST77XX_WHITE);

  // 3. Swept Wings
  tft.drawLine(px + 5, py + 8, px, py + 13, mainWhite);
  tft.drawLine(px + 5, py + 9, px + 1, py + 13, darkGray);
  tft.drawPixel(px, py + 14, primaryRed);
  tft.drawPixel(px + 1, py + 14, primaryRed);

  tft.drawLine(px + 9, py + 8, px + 14, py + 13, mainWhite);
  tft.drawLine(px + 9, py + 9, px + 13, py + 13, darkGray);
  tft.drawPixel(px + 14, py + 14, primaryRed);
  tft.drawPixel(px + 13, py + 14, primaryRed);

  // 4. Engines
  tft.fillRect(px + 2, py + 11, 2, 3, darkGray);
  tft.fillRect(px + 11, py + 11, 2, 3, darkGray);
  tft.fillRect(px + 6, py + 14, 3, 2, darkGray);

  // 5. Thruster flames
  // Left flame
  tft.drawPixel(px + 2, py + 14, orangeGlow);
  tft.drawPixel(px + 3, py + 14, orangeGlow);
  tft.drawPixel(px + 2, py + 15, primaryRed);
  tft.drawPixel(px + 3, py + 15, primaryRed);
  // Right flame
  tft.drawPixel(px + 11, py + 14, orangeGlow);
  tft.drawPixel(px + 12, py + 14, orangeGlow);
  tft.drawPixel(px + 11, py + 15, primaryRed);
  tft.drawPixel(px + 12, py + 15, primaryRed);
  // Center flame
  tft.drawPixel(px + 7, py + 16, yellowGlow);
}

static void clearProjectiles() {
  for (int i = 0; i < MAX_PROJECTILES; i++) {
    projectiles[i].active = false;
  }
}

static void spawnProjectile() {
  const uint16_t primaryRed = tft.color565(255, 32, 21);
  for (int i = 0; i < MAX_PROJECTILES; i++) {
    if (!projectiles[i].active) {
      projectiles[i].x = playerX + 6.0f;
      projectiles[i].y = playerY - 6.0f;
      projectiles[i].prevX = projectiles[i].x;
      projectiles[i].prevY = projectiles[i].y;
      projectiles[i].active = true;
      
      // Draw immediately
      tft.fillRect((int16_t)projectiles[i].x, (int16_t)projectiles[i].y, 2, 6, primaryRed);
      break;
    }
  }
}

static void updateProjectiles(uint32_t dt) {
  const uint16_t primaryRed = tft.color565(255, 32, 21);
  float speed = 0.15f; // pixels per millisecond

  for (int i = 0; i < MAX_PROJECTILES; i++) {
    if (projectiles[i].active) {
      // 1. Erase at previous position
      tft.fillRect((int16_t)projectiles[i].prevX, (int16_t)projectiles[i].prevY, 2, 6, ST77XX_BLACK);
      
      // 2. Update Y position
      projectiles[i].y -= speed * dt;
      
      // 3. Check boundary
      if (projectiles[i].y < 14.0f) {
        projectiles[i].active = false;
      } else {
        // 4. Draw at new position
        tft.fillRect((int16_t)projectiles[i].x, (int16_t)projectiles[i].y, 2, 6, primaryRed);
        projectiles[i].prevX = projectiles[i].x;
        projectiles[i].prevY = projectiles[i].y;
      }
    }
  }
}

static void initStarfield() {
  randomSeed(millis() + analogRead(36));

  // Far layer (35 stars): tiny, dim, slow. Color: Dim cool-toned space palette
  for (int i = 0; i < STAR_COUNT_FAR; i++) {
    farStars[i].x = random(2, 126);
    farStars[i].y = random(0, 160);
    farStars[i].speed = STARFIELD_SPEED_FAR * (0.8f + (random(5) / 10.0f));
    farStars[i].type = 0;

    int c = random(4);
    if (c == 0) farStars[i].color = tft.color565(25, 12, 35);      // Dim Purple
    else if (c == 1) farStars[i].color = tft.color565(12, 25, 35); // Dim Blue
    else if (c == 2) farStars[i].color = tft.color565(12, 30, 25); // Dim Teal
    else farStars[i].color = tft.color565(30, 25, 12);             // Dim Gold
  }

  // Mid layer (25 stars): medium, slightly brighter. Color: Medium cool space palette
  for (int i = 0; i < STAR_COUNT_MID; i++) {
    midStars[i].x = random(2, 126);
    midStars[i].y = random(0, 160);
    midStars[i].speed = STARFIELD_SPEED_MID * (0.8f + (random(5) / 10.0f));
    // 0=1x1 pixel, 1=2x1 horizontal dash
    midStars[i].type = (random(4) == 0) ? 1 : 0; 

    int c = random(4);
    if (c == 0) midStars[i].color = tft.color565(55, 35, 75);
    else if (c == 1) midStars[i].color = tft.color565(35, 55, 75);
    else if (c == 2) midStars[i].color = tft.color565(35, 70, 55);
    else midStars[i].color = tft.color565(70, 55, 35);
  }

  // Near layer (12 stars): foreground, bright but cool, fast
  for (int i = 0; i < STAR_COUNT_NEAR; i++) {
    nearStars[i].x = random(3, 125);
    nearStars[i].y = random(0, 160);
    nearStars[i].speed = STARFIELD_SPEED_NEAR * (0.8f + (random(5) / 10.0f));
    
    int c = random(4);
    if (c == 0) nearStars[i].color = tft.color565(95, 65, 135);
    else if (c == 1) nearStars[i].color = tft.color565(65, 95, 135);
    else if (c == 2) nearStars[i].color = tft.color565(65, 125, 95);
    else nearStars[i].color = tft.color565(125, 95, 65);

    int r = random(4);
    if (r == 0) {
      nearStars[i].type = 2; // 2x2 soft block (simulates blur)
    } else if (r == 1) {
      nearStars[i].type = 3; // 1x3 vertical speed streak (simulates blur)
    } else {
      nearStars[i].type = 0; // 1x1 pixel
    }
  }
}

static void eraseStar(const Star &s) {
  int16_t px = (int16_t)playerX;
  int16_t py = (int16_t)playerY;
  int16_t ix = (int16_t)s.x;
  int16_t iy = (int16_t)s.y;

  if (iy < 14) {
    return;
  }

  // Skip erasing if it falls inside player ship bounding box to prevent trails
  if (ix >= px - 1 && ix < px + 16 && iy >= py - 1 && iy < py + 18) {
    return;
  }

  if (s.type == 0 || s.type == 5) {
    tft.drawPixel(ix, iy, ST77XX_BLACK);
  } else if (s.type == 1) {
    tft.drawFastHLine(ix, iy, 2, ST77XX_BLACK);
  } else if (s.type == 2) {
    tft.fillRect(ix, iy, 2, 2, ST77XX_BLACK);
  } else if (s.type == 3) {
    tft.drawFastVLine(ix, iy, 3, ST77XX_BLACK);
  } else if (s.type == 4) {
    tft.drawFastHLine(ix - 1, iy, 3, ST77XX_BLACK);
    tft.drawFastVLine(ix, iy - 1, 3, ST77XX_BLACK);
  }
}

static void drawStar(const Star &s) {
  int16_t px = (int16_t)playerX;
  int16_t py = (int16_t)playerY;
  int16_t ix = (int16_t)s.x;
  int16_t iy = (int16_t)s.y;

  if (iy < 14) {
    return;
  }

  // Skip drawing if it falls inside player ship bounding box
  if (ix >= px - 1 && ix < px + 16 && iy >= py - 1 && iy < py + 18) {
    return;
  }

  if (s.type == 0 || s.type == 5) {
    tft.drawPixel(ix, iy, s.color);
  } else if (s.type == 1) {
    tft.drawFastHLine(ix, iy, 2, s.color);
  } else if (s.type == 2) {
    tft.fillRect(ix, iy, 2, 2, s.color);
  } else if (s.type == 3) {
    tft.drawFastVLine(ix, iy, 3, s.color);
  } else if (s.type == 4) {
    tft.drawFastHLine(ix - 1, iy, 3, s.color);
    tft.drawFastVLine(ix, iy - 1, 3, s.color);
  }
}

static void updateStarfield(uint32_t dt, float speedMultiplier) {
  // Update Far Stars
  for (int i = 0; i < STAR_COUNT_FAR; i++) {
    eraseStar(farStars[i]);
    farStars[i].y += farStars[i].speed * dt * speedMultiplier;
    if (farStars[i].y >= 160.0f) {
      farStars[i].y = 0.0f;
      farStars[i].x = random(2, 126);
    }
    drawStar(farStars[i]);
  }

  // Update Mid Stars
  for (int i = 0; i < STAR_COUNT_MID; i++) {
    eraseStar(midStars[i]);
    midStars[i].y += midStars[i].speed * dt * speedMultiplier;
    if (midStars[i].y >= 160.0f) {
      midStars[i].y = 0.0f;
      midStars[i].x = random(2, 126);
      midStars[i].type = (random(4) == 0) ? 1 : 0;
    }
    drawStar(midStars[i]);
  }

  // Update Near Stars
  for (int i = 0; i < STAR_COUNT_NEAR; i++) {
    eraseStar(nearStars[i]);
    nearStars[i].y += nearStars[i].speed * dt * speedMultiplier;
    if (nearStars[i].y >= 160.0f) {
      nearStars[i].y = 0.0f;
      nearStars[i].x = random(3, 125);
      int r = random(4);
      if (r == 0) nearStars[i].type = 2;
      else if (r == 1) nearStars[i].type = 3;
      else nearStars[i].type = 0;
    }
    drawStar(nearStars[i]);
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
      resetEnemySystem(); // Reset enemy logic
      currentScreen = STATE_PIXEL_WARS_COUNTDOWN;
      countdownStartTime = millis();
      lastCountdownNumber = -1;
      lastGameplayFrameTime = millis(); // Initialize the timer for countdown scrolling
      clearProjectiles(); // Reset projectiles
      initStarfield(); // Initialize scrolling starfield
      
      // Clear screen and draw initial starfield
      tft.fillScreen(ST77XX_BLACK);
      for (int i = 0; i < STAR_COUNT_FAR; i++) drawStar(farStars[i]);
      for (int i = 0; i < STAR_COUNT_MID; i++) drawStar(midStars[i]);
      for (int i = 0; i < STAR_COUNT_NEAR; i++) drawStar(nearStars[i]);
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

static void resetEnemySystem() {
  if (PIXEL_WARS_DEBUG_RANDOM) {
    randomSeed(12345);
  }
  for (int i = 0; i < MAX_ENEMIES; i++) {
    enemies[i].active = false;
  }
  for (int i = 0; i < MAX_EXPLOSIONS; i++) {
    explosions[i].active = false;
  }
  for (int i = 0; i < MAX_ENEMY_PROJECTILES; i++) {
    enemyProjectiles[i].active = false;
  }
  
  lastSpawnActionTime = millis();
  spawnSequenceIndex = 0;
  randomSpawnCount = 0;
  
  // Reset pacing
  currentPaceMultiplier = 1.0f;
  targetPaceMultiplier = 1.0f;
  pacePhaseStartTime = millis();
  pacePhaseDuration = random(4000, 7001); // 4-7 seconds for first phase
  currentPacePhase = 1; // Start at MEDIUM

  // Reset HUD
  playerHealth = 3;
  score = 0;
  lastDrawHealth = -1;
  lastDrawScore = -1;
}

static void drawEnemyType1(int16_t x, int16_t y) {
  const uint16_t primaryRed = tft.color565(255, 32, 21);
  const uint16_t orangeGlow = tft.color565(255, 74, 31);
  const uint16_t darkGray = tft.color565(36, 43, 49);
  const uint16_t blueGray = tft.color565(82, 103, 121);
  
  tft.drawPixel(x + 1, y, orangeGlow);
  tft.drawPixel(x + 7, y, orangeGlow);
  tft.fillRect(x + 2, y + 1, 5, 2, darkGray);
  
  tft.drawLine(x, y + 3, x + 3, y + 3, blueGray);
  tft.drawLine(x + 5, y + 3, x + 8, y + 3, blueGray);
  tft.drawPixel(x, y + 4, primaryRed);
  tft.drawPixel(x + 8, y + 4, primaryRed);
  
  tft.fillRect(x + 3, y + 2, 3, 5, blueGray);
  tft.drawPixel(x + 4, y + 4, orangeGlow);
  tft.drawLine(x + 3, y + 7, x + 5, y + 7, darkGray);
  tft.drawPixel(x + 4, y + 8, primaryRed);
}

static void drawEnemyType2(int16_t x, int16_t y) {
  const uint16_t primaryRed = tft.color565(255, 32, 21);
  const uint16_t enemyPurple = tft.color565(140, 30, 160);
  const uint16_t darkGray = tft.color565(36, 43, 49);
  const uint16_t blueGray = tft.color565(82, 103, 121);
  
  tft.drawPixel(x + 2, y, primaryRed);
  tft.drawPixel(x + 4, y, primaryRed);
  
  tft.fillRect(x + 2, y + 1, 3, 3, darkGray);
  
  tft.drawLine(x + 1, y + 4, x, y + 5, blueGray);
  tft.drawLine(x + 5, y + 4, x + 6, y + 5, blueGray);
  tft.drawPixel(x, y + 6, enemyPurple);
  tft.drawPixel(x + 6, y + 6, enemyPurple);
  
  tft.fillRect(x + 2, y + 4, 3, 5, blueGray);
  tft.drawPixel(x + 3, y + 6, enemyPurple);
  tft.drawPixel(x + 3, y + 9, darkGray);
  tft.drawPixel(x + 3, y + 10, primaryRed);
}

static void drawEnemyType3(int16_t x, int16_t y, bool flashRed) {
  const uint16_t primaryRed = tft.color565(255, 32, 21);
  const uint16_t orangeGlow = flashRed ? primaryRed : tft.color565(255, 74, 31);
  const uint16_t darkGray = flashRed ? primaryRed : tft.color565(36, 43, 49);
  const uint16_t blueGray = flashRed ? primaryRed : tft.color565(82, 103, 121);
  
  tft.drawPixel(x + 3, y, orangeGlow);
  tft.drawPixel(x + 6, y, orangeGlow);
  tft.drawPixel(x + 9, y, orangeGlow);
  
  tft.fillRect(x + 2, y + 1, 9, 3, darkGray);
  
  tft.fillRect(x + 1, y + 4, 11, 2, blueGray);
  tft.drawPixel(x, y + 5, primaryRed);
  tft.drawPixel(x + 12, y + 5, primaryRed);
  
  tft.drawPixel(x + 3, y + 4, darkGray);
  tft.drawPixel(x + 9, y + 4, darkGray);
  
  tft.fillRect(x + 4, y + 5, 5, 4, darkGray);
  tft.fillRect(x + 5, y + 7, 3, 2, blueGray);
  tft.drawPixel(x + 6, y + 8, primaryRed);
  
  tft.drawPixel(x + 5, y + 10, primaryRed);
  tft.drawPixel(x + 7, y + 10, primaryRed);
}

static void drawEnemyType4(int16_t x, int16_t y, bool flashRed) {
  const uint16_t primaryRed = tft.color565(255, 32, 21);
  const uint16_t yellowAccent = flashRed ? primaryRed : tft.color565(255, 180, 0);
  const uint16_t bodyColor = flashRed ? primaryRed : tft.color565(36, 43, 49);
  const uint16_t detailColor = flashRed ? primaryRed : tft.color565(82, 103, 121);

  tft.drawPixel(x + 2, y, yellowAccent);
  tft.drawPixel(x + 6, y, yellowAccent);
  tft.fillRect(x + 3, y + 1, 3, 2, bodyColor);
  
  tft.drawPixel(x, y + 2, yellowAccent);
  tft.drawPixel(x + 8, y + 2, yellowAccent);
  tft.drawLine(x + 1, y + 3, x + 2, y + 4, detailColor);
  tft.drawLine(x + 7, y + 3, x + 6, y + 4, detailColor);
  
  tft.fillRect(x + 3, y + 3, 3, 4, bodyColor);
  tft.drawPixel(x + 4, y + 5, yellowAccent);
  
  tft.drawPixel(x + 4, y + 7, detailColor);
  tft.drawPixel(x + 4, y + 8, yellowAccent);
}

static void drawEnemy(int16_t x, int16_t y, uint8_t type, bool flashRed) {
  switch (type) {
    case 1:
      drawEnemyType1(x, y);
      break;
    case 2:
      drawEnemyType2(x, y);
      break;
    case 3:
      drawEnemyType3(x, y, flashRed);
      break;
    case 4:
      drawEnemyType4(x, y, flashRed);
      break;
  }
}

static void eraseEnemy(int16_t x, int16_t y, uint8_t type) {
  switch (type) {
    case 1:
      tft.fillRect(x, y, 9, 9, ST77XX_BLACK);
      break;
    case 2:
      tft.fillRect(x, y, 7, 11, ST77XX_BLACK);
      break;
    case 3:
      tft.fillRect(x, y, 13, 11, ST77XX_BLACK);
      break;
    case 4:
      tft.fillRect(x, y, 9, 9, ST77XX_BLACK);
      break;
  }
}

static void drawExplosionStage(int16_t cx, int16_t cy, uint8_t stage, bool erase) {
  uint16_t color1 = erase ? (uint16_t)ST77XX_BLACK : tft.color565(255, 180, 0);
  uint16_t color2 = erase ? (uint16_t)ST77XX_BLACK : tft.color565(255, 32, 21);

  if (stage == 1) {
    tft.drawPixel(cx, cy, color1);
    tft.drawPixel(cx - 1, cy, color2);
    tft.drawPixel(cx + 1, cy, color2);
    tft.drawPixel(cx, cy - 1, color2);
    tft.drawPixel(cx, cy + 1, color2);
  } else if (stage == 2) {
    tft.drawPixel(cx - 2, cy, color1);
    tft.drawPixel(cx + 2, cy, color1);
    tft.drawPixel(cx, cy - 2, color1);
    tft.drawPixel(cx, cy + 2, color1);
    
    tft.drawPixel(cx - 1, cy - 1, color2);
    tft.drawPixel(cx + 1, cy - 1, color2);
    tft.drawPixel(cx - 1, cy + 1, color2);
    tft.drawPixel(cx + 1, cy + 1, color2);
  } else if (stage == 3) {
    tft.drawPixel(cx - 3, cy, color2);
    tft.drawPixel(cx + 3, cy, color2);
    tft.drawPixel(cx, cy - 3, color2);
    tft.drawPixel(cx, cy + 3, color2);
  }
}

static void updateExplosions() {
  unsigned long now = millis();
  for (int i = 0; i < MAX_EXPLOSIONS; i++) {
    if (explosions[i].active) {
      unsigned long elapsed = now - explosions[i].startTime;
      uint8_t currentStage = 0;
      if (elapsed < 50) {
        currentStage = 1;
      } else if (elapsed < 100) {
        currentStage = 2;
      } else if (elapsed < 150) {
        currentStage = 3;
      } else {
        currentStage = 4;
      }

      if (currentStage != explosions[i].lastStage) {
        if (explosions[i].lastStage >= 1 && explosions[i].lastStage <= 3) {
          drawExplosionStage(explosions[i].cx, explosions[i].cy, explosions[i].lastStage, true);
        }
        
        if (currentStage >= 1 && currentStage <= 3) {
          drawExplosionStage(explosions[i].cx, explosions[i].cy, currentStage, false);
          explosions[i].lastStage = currentStage;
        } else {
          explosions[i].active = false;
        }
      }
    }
  }
}

static bool spawnEnemy(float x, float y, float vx, float vy, uint8_t type) {
  for (int i = 0; i < MAX_ENEMIES; i++) {
    if (!enemies[i].active) {
      enemies[i].x = x;
      enemies[i].y = y;
      enemies[i].prevX = x;
      enemies[i].prevY = y;
      enemies[i].vx = vx;
      enemies[i].vy = vy;
      enemies[i].active = true;
      enemies[i].type = type;
      
      // Randomly assign movement style and next attack delay
      if (type == 1) {
        enemies[i].moveStyle = (random(5) == 0) ? STYLE_DRIFT : STYLE_STRAIGHT;
        enemies[i].nextAttackTime = millis() + random(MIN_ATTACK_DELAY_BASIC, MAX_ATTACK_DELAY_BASIC);
      } else if (type == 2) {
        uint8_t r = random(3);
        enemies[i].moveStyle = (r == 0) ? STYLE_ZIGZAG : ((r == 1) ? STYLE_STEP : STYLE_DRIFT);
        enemies[i].nextAttackTime = millis() + random(MIN_ATTACK_DELAY_FAST, MAX_ATTACK_DELAY_FAST);
      } else if (type == 3) {
        enemies[i].moveStyle = (random(2) == 0) ? STYLE_STRAIGHT : STYLE_OSCILLATE;
        enemies[i].nextAttackTime = millis() + random(MIN_ATTACK_DELAY_HEAVY, MAX_ATTACK_DELAY_HEAVY);
      } else { // Special (Type 4)
        enemies[i].moveStyle = random(5);
        enemies[i].nextAttackTime = millis() + random(MIN_ATTACK_DELAY_SPECIAL, MAX_ATTACK_DELAY_SPECIAL);
      }
      
      enemies[i].movePhase = (random(100) / 10.0f);
      enemies[i].baseX = x;
      enemies[i].driftDir = (random(2) == 0) ? -1 : 1;
      enemies[i].nextMoveChangeTime = millis() + random(800, 2000);
      enemies[i].bombWarningActive = false;
      enemies[i].bombWarningStartTime = 0;
      
      return true;
    }
  }
  return false;
}

static void spawnGridFormation() {
  uint8_t typeFront = random(1, 3);
  uint8_t typeBack = random(2, 5); // 2, 3, or 4
  float vy = 0.025f;
  
  spawnEnemy(29.0f, -15.0f, 0.0f, vy, typeFront);
  spawnEnemy(64.0f, -15.0f, 0.0f, vy, typeFront);
  spawnEnemy(99.0f, -15.0f, 0.0f, vy, typeFront);
  
  spawnEnemy(29.0f, -35.0f, 0.0f, vy, typeBack);
  spawnEnemy(64.0f, -35.0f, 0.0f, vy, typeBack);
  spawnEnemy(99.0f, -35.0f, 0.0f, vy, typeBack);
}

static void spawnVFormation() {
  float vy = 0.028f;
  spawnEnemy(58.0f, -15.0f, 0.0f, vy, 3);
  spawnEnemy(40.0f, -30.0f, 0.0f, vy, 1);
  spawnEnemy(80.0f, -30.0f, 0.0f, vy, 1);
  spawnEnemy(21.0f, -45.0f, 0.0f, vy, 2);
  spawnEnemy(101.0f, -45.0f, 0.0f, vy, 2);
}

static void spawnRandomCluster() {
  float cx = random(35, 90);
  float cy = random(-40, -20);
  int count = random(4, 7);
  float vy = 0.032f;
  
  for (int i = 0; i < count; i++) {
    float rx = cx + random(-25, 25);
    float ry = cy + random(-20, 20);
    if (rx < 10) rx = 10;
    if (rx > 110) rx = 110;
    
    uint8_t type = random(1, 5); // types 1 to 4
    float vx = (random(60) - 30) / 1000.0f;
    spawnEnemy(rx, ry, vx, vy, type);
  }
}

static void spawnDiamondFormation() {
  float vy = 0.025f;
  spawnEnemy(60.0f, -55.0f, 0.0f, vy, 1);
  spawnEnemy(41.0f, -35.0f, 0.0f, vy, 2);
  spawnEnemy(81.0f, -35.0f, 0.0f, vy, 2);
  spawnEnemy(58.0f, -35.0f, 0.0f, vy, 3);
  spawnEnemy(60.0f, -15.0f, 0.0f, vy, 1);
}

static void spawnLineFormation() {
  float vy = 0.035f;
  spawnEnemy(13.0f, -15.0f, 0.0f, vy, 2);
  spawnEnemy(37.0f, -15.0f, 0.0f, vy, 2);
  spawnEnemy(61.0f, -15.0f, 0.0f, vy, 2);
  spawnEnemy(85.0f, -15.0f, 0.0f, vy, 2);
  spawnEnemy(109.0f, -15.0f, 0.0f, vy, 2);
}

static void spawnStaggeredFormation() {
  float vy = 0.026f;
  spawnEnemy(11.0f, -15.0f, 0.0f, vy, 1);
  spawnEnemy(51.0f, -15.0f, 0.0f, vy, 1);
  spawnEnemy(91.0f, -15.0f, 0.0f, vy, 1);
  
  spawnEnemy(32.0f, -35.0f, 0.0f, vy, 2);
  spawnEnemy(72.0f, -35.0f, 0.0f, vy, 2);
  spawnEnemy(112.0f, -35.0f, 0.0f, vy, 2);
}

static void spawnFormation(int seqIndex) {
  switch (seqIndex) {
    case 0:
      spawnGridFormation();
      break;
    case 2:
      spawnVFormation();
      break;
    case 3:
      spawnRandomCluster();
      break;
    case 4:
      spawnDiamondFormation();
      break;
    case 6:
      spawnLineFormation();
      break;
    case 7:
      spawnStaggeredFormation();
      break;
  }
}

static void updateSpawnManager() {
  unsigned long now = millis();
  
  int activeCount = 0;
  for (int i = 0; i < MAX_ENEMIES; i++) {
    if (enemies[i].active) activeCount++;
  }

  if (spawnSequenceIndex == 1 || spawnSequenceIndex == 5) {
    if (randomSpawnCount < 3) {
      if (now - lastSpawnActionTime >= 1800) {
        float rx = random(10, 110);
        float ry = -15.0f;
        float rvx = (random(100) - 50) / 1000.0f;
        float rvy = 0.03f + (random(20) / 1000.0f);
        uint8_t rtype = random(1, 5); // 1 to 4
        
        spawnEnemy(rx, ry, rvx, rvy, rtype);
        randomSpawnCount++;
        lastSpawnActionTime = now;
      }
    } else {
      if (now - lastSpawnActionTime >= 5000 && activeCount == 0) {
        spawnSequenceIndex = (spawnSequenceIndex + 1) % 8;
        randomSpawnCount = 0;
        lastSpawnActionTime = now - 5000;
      } else if (now - lastSpawnActionTime >= 8000) {
        spawnSequenceIndex = (spawnSequenceIndex + 1) % 8;
        randomSpawnCount = 0;
        lastSpawnActionTime = now;
      }
    }
    return;
  }

  unsigned long waitTime = 7000;
  if (activeCount == 0 && (now - lastSpawnActionTime >= 3000)) {
    waitTime = 3000;
  }

  if (now - lastSpawnActionTime >= waitTime) {
    spawnFormation(spawnSequenceIndex);
    lastSpawnActionTime = now;
    spawnSequenceIndex = (spawnSequenceIndex + 1) % 8;
    randomSpawnCount = 0;
  }
}

static void updateEnemies(uint32_t dt) {
  unsigned long now = millis();
  for (int i = 0; i < MAX_ENEMIES; i++) {
    if (enemies[i].active) {
      if (enemies[i].prevY >= 14.0f) {
        eraseEnemy((int16_t)enemies[i].prevX, (int16_t)enemies[i].prevY, enemies[i].type);
      }
      
      float maxW = 9.0f;
      float maxH = 9.0f;
      if (enemies[i].type == 2) { maxW = 7.0f; maxH = 11.0f; }
      else if (enemies[i].type == 3) { maxW = 13.0f; maxH = 11.0f; }
      else if (enemies[i].type == 4) { maxW = 9.0f; maxH = 9.0f; }

      // Update baseX if horizontal velocity is present
      enemies[i].baseX += enemies[i].vx * currentPaceMultiplier * dt;

      if (enemies[i].moveStyle == STYLE_STRAIGHT) {
        enemies[i].y += enemies[i].vy * currentPaceMultiplier * dt;
      }
      else if (enemies[i].moveStyle == STYLE_DRIFT) {
        enemies[i].x += enemies[i].driftDir * 0.015f * currentPaceMultiplier * dt;
        enemies[i].y += enemies[i].vy * currentPaceMultiplier * dt;
        if (enemies[i].x <= 0.0f) { enemies[i].x = 0.0f; enemies[i].driftDir = 1; }
        if (enemies[i].x >= 128.0f - maxW) { enemies[i].x = 128.0f - maxW; enemies[i].driftDir = -1; }
      }
      else if (enemies[i].moveStyle == STYLE_OSCILLATE) {
        enemies[i].y += enemies[i].vy * currentPaceMultiplier * dt;
        enemies[i].x = enemies[i].baseX + 12.0f * sin((float)millis() * 0.003f + enemies[i].movePhase);
      }
      else if (enemies[i].moveStyle == STYLE_ZIGZAG) {
        if (now >= enemies[i].nextMoveChangeTime) {
          enemies[i].driftDir = -enemies[i].driftDir;
          enemies[i].nextMoveChangeTime = now + random(1000, 2001);
        }
        enemies[i].x += enemies[i].driftDir * 0.03f * currentPaceMultiplier * dt;
        enemies[i].y += enemies[i].vy * currentPaceMultiplier * dt;
      }
      else if (enemies[i].moveStyle == STYLE_STEP) {
        if (now >= enemies[i].nextMoveChangeTime) {
          uint8_t currentPhase = ((int)enemies[i].movePhase) % 2;
          currentPhase = 1 - currentPhase;
          enemies[i].movePhase = currentPhase;
          if (currentPhase == 1) {
            enemies[i].driftDir = (random(2) == 0) ? -1 : 1;
          }
          enemies[i].nextMoveChangeTime = now + random(600, 1001);
        }
        uint8_t currentPhase = ((int)enemies[i].movePhase) % 2;
        if (currentPhase == 0) {
          enemies[i].y += enemies[i].vy * currentPaceMultiplier * dt;
        } else {
          enemies[i].x += enemies[i].driftDir * 0.035f * currentPaceMultiplier * dt;
        }
      }
      
      if (enemies[i].x < 0.0f) enemies[i].x = 0.0f;
      if (enemies[i].x > 128.0f - maxW) enemies[i].x = 128.0f - maxW;

      if (enemies[i].y >= 160.0f) {
        enemies[i].active = false;
      } else {
        // Fire logic
        if (now >= enemies[i].nextAttackTime) {
          if (enemies[i].y < 0.0f) {
            enemies[i].nextAttackTime = now + 500;
          } else {
            uint8_t attackType = 1;
            if (enemies[i].type == 1) attackType = 1;
            else if (enemies[i].type == 2) attackType = (random(4) == 0) ? 2 : 1;
            else if (enemies[i].type == 3) attackType = 3;
            else {
              int r = random(100);
              if (r < 60) attackType = 1;
              else if (r < 85) attackType = 2;
              else attackType = 3;
            }

            if (attackType == 3) {
              if (!enemies[i].bombWarningActive) {
                enemies[i].bombWarningActive = true;
                enemies[i].bombWarningStartTime = now;
                enemies[i].nextAttackTime = now + 200;
              } else {
                enemies[i].bombWarningActive = false;
                float bx = enemies[i].x + ((enemies[i].type == 3) ? 5.0f : 3.0f);
                float by = enemies[i].y + 11.0f;
                spawnEnemyProjectile(bx, by, 0.0f, 0.035f, 3);
                
                unsigned long delay = random(MIN_ATTACK_DELAY_HEAVY, MAX_ATTACK_DELAY_HEAVY);
                if (enemies[i].type == 4) delay = random(MIN_ATTACK_DELAY_SPECIAL, MAX_ATTACK_DELAY_SPECIAL);
                enemies[i].nextAttackTime = now + delay;
              }
            } else {
              float sx = enemies[i].x + ((enemies[i].type == 2) ? 2.5f : 3.5f);
              float sy = enemies[i].y + ((enemies[i].type == 2) ? 11.0f : 9.0f);
              float svx = 0.0f;
              float svy = (enemies[i].type == 2) ? 0.08f : 0.055f;
              
              if (attackType == 2) {
                float dx = (playerX + 6.0f) - sx;
                float dy = (playerY + 8.0f) - sy;
                float dist = sqrt(dx * dx + dy * dy);
                if (dist > 0.0f) {
                  float shotSpeed = (enemies[i].type == 2) ? 0.08f : 0.055f;
                  svx = (dx / dist) * shotSpeed;
                  svy = (dy / dist) * shotSpeed;
                }
              }
              spawnEnemyProjectile(sx, sy, svx, svy, attackType);
              
              unsigned long delay = 0;
              if (enemies[i].type == 1) delay = random(MIN_ATTACK_DELAY_BASIC, MAX_ATTACK_DELAY_BASIC);
              else if (enemies[i].type == 2) delay = random(MIN_ATTACK_DELAY_FAST, MAX_ATTACK_DELAY_FAST);
              else delay = random(MIN_ATTACK_DELAY_SPECIAL, MAX_ATTACK_DELAY_SPECIAL);
              enemies[i].nextAttackTime = now + delay;
            }
          }
        }

        bool flash = false;
        if (enemies[i].bombWarningActive) {
          flash = ((millis() / 50) % 2 == 0);
        }
        
        if (enemies[i].y >= 14.0f) {
          drawEnemy((int16_t)enemies[i].x, (int16_t)enemies[i].y, enemies[i].type, flash);
        }
        
        enemies[i].prevX = enemies[i].x;
        enemies[i].prevY = enemies[i].y;
      }
    }
  }
}

static void checkCollisions() {
  for (int p = 0; p < MAX_PROJECTILES; p++) {
    if (projectiles[p].active) {
      float px = projectiles[p].x;
      float py = projectiles[p].y;
      float pw = 2.0f;
      float ph = 6.0f;
      
      for (int e = 0; e < MAX_ENEMIES; e++) {
        if (enemies[e].active) {
          float ex = enemies[e].x;
          float ey = enemies[e].y;
          float ew = 9.0f;
          float eh = 9.0f;
          if (enemies[e].type == 2) {
            ew = 7.0f;
            eh = 11.0f;
          } else if (enemies[e].type == 3) {
            ew = 13.0f;
            eh = 11.0f;
          } else if (enemies[e].type == 4) {
            ew = 9.0f;
            eh = 9.0f;
          }
          
          if (ey + eh > 0.0f) {
            if (px < ex + ew && px + pw > ex && py < ey + eh && py + ph > ey) {
              int16_t cx = (int16_t)(ex + ew / 2.0f);
              int16_t cy = (int16_t)(ey + eh / 2.0f);
              
              for (int x = 0; x < MAX_EXPLOSIONS; x++) {
                if (!explosions[x].active) {
                  explosions[x].cx = cx;
                  explosions[x].cy = cy;
                  explosions[x].startTime = millis();
                  explosions[x].lastStage = 0;
                  explosions[x].active = true;
                  break;
                }
              }
              
              eraseEnemy((int16_t)enemies[e].x, (int16_t)enemies[e].y, enemies[e].type);
              enemies[e].active = false;
              
              tft.fillRect((int16_t)projectiles[p].x, (int16_t)projectiles[p].y, 2, 6, ST77XX_BLACK);
              projectiles[p].active = false;
              break;
            }
          }
        }
      }
    }
  }
}

static void updateGlobalPace(uint32_t dt) {
  unsigned long now = millis();
  currentPaceMultiplier += (targetPaceMultiplier - currentPaceMultiplier) * 0.0005f * dt;
  if (currentPaceMultiplier < 0.5f) currentPaceMultiplier = 0.5f;
  if (currentPaceMultiplier > 2.0f) currentPaceMultiplier = 2.0f;

  if (now - pacePhaseStartTime >= pacePhaseDuration) {
    uint8_t nextPhase = random(3);
    while (nextPhase == currentPacePhase) {
      nextPhase = random(3);
    }
    currentPacePhase = nextPhase;
    
    if (currentPacePhase == 0) {
      targetPaceMultiplier = 0.75f;
      pacePhaseDuration = random(3000, 6001);
    } else if (currentPacePhase == 1) {
      targetPaceMultiplier = 1.0f;
      pacePhaseDuration = random(4000, 8001);
    } else {
      targetPaceMultiplier = 1.30f;
      pacePhaseDuration = random(2000, 5001);
    }
    pacePhaseStartTime = now;
  }
}

static void spawnEnemyProjectile(float x, float y, float vx, float vy, uint8_t type) {
  for (int i = 0; i < MAX_ENEMY_PROJECTILES; i++) {
    if (!enemyProjectiles[i].active) {
      enemyProjectiles[i].x = x;
      enemyProjectiles[i].y = y;
      enemyProjectiles[i].prevX = x;
      enemyProjectiles[i].prevY = y;
      enemyProjectiles[i].vx = vx;
      enemyProjectiles[i].vy = vy;
      enemyProjectiles[i].active = true;
      enemyProjectiles[i].type = type;
      
      drawEnemyProjectile((int16_t)x, (int16_t)y, type, false);
      break;
    }
  }
}

static void drawEnemyProjectile(int16_t x, int16_t y, uint8_t type, bool erase) {
  uint16_t color = ST77XX_BLACK;
  if (!erase) {
    if (type == 1) {
      color = tft.color565(255, 32, 21);
    } else if (type == 2) {
      color = tft.color565(255, 120, 0);
    } else if (type == 3) {
      if ((millis() / 150) % 2 == 0) {
        color = tft.color565(255, 32, 21);
      } else {
        color = tft.color565(255, 180, 0);
      }
    }
  }
  
  if (type == 1 || type == 2) {
    tft.fillRect(x, y, 2, 4, color);
  } else if (type == 3) {
    tft.drawPixel(x + 1, y, color);
    tft.drawPixel(x, y + 1, color);
    tft.drawPixel(x + 1, y + 1, color);
    tft.drawPixel(x + 2, y + 1, color);
    tft.drawPixel(x + 1, y + 2, color);
  }
}

static void updateEnemyProjectiles(uint32_t dt) {
  for (int i = 0; i < MAX_ENEMY_PROJECTILES; i++) {
    if (enemyProjectiles[i].active) {
      if (enemyProjectiles[i].prevY >= 14.0f) {
        drawEnemyProjectile((int16_t)enemyProjectiles[i].prevX, (int16_t)enemyProjectiles[i].prevY, enemyProjectiles[i].type, true);
      }
      
      if (enemyProjectiles[i].type == 3) {
        enemyProjectiles[i].vx = 0.015f * sin((float)millis() * 0.004f);
      }
      
      enemyProjectiles[i].x += enemyProjectiles[i].vx * currentPaceMultiplier * dt;
      enemyProjectiles[i].y += enemyProjectiles[i].vy * currentPaceMultiplier * dt;
      
      if (enemyProjectiles[i].y >= 160.0f || enemyProjectiles[i].y < 14.0f || enemyProjectiles[i].x < -10.0f || enemyProjectiles[i].x >= 138.0f) {
        enemyProjectiles[i].active = false;
      } else {
        drawEnemyProjectile((int16_t)enemyProjectiles[i].x, (int16_t)enemyProjectiles[i].y, enemyProjectiles[i].type, false);
        enemyProjectiles[i].prevX = enemyProjectiles[i].x;
        enemyProjectiles[i].prevY = enemyProjectiles[i].y;
      }
    }
  }
}

static void drawHeart(int16_t x, int16_t y, uint8_t state) {
  const uint16_t borderRed = tft.color565(150, 20, 20); // Darker red border
  const uint16_t fillRed = tft.color565(255, 32, 21);   // Bright red fill
  const uint16_t black = ST77XX_BLACK;

  // Clear bounding box (7x7) to black
  tft.fillRect(x, y, 7, 7, black);

  // Draw Border (always present)
  tft.drawPixel(x + 1, y, borderRed);
  tft.drawPixel(x + 2, y, borderRed);
  tft.drawPixel(x + 4, y, borderRed);
  tft.drawPixel(x + 5, y, borderRed);

  tft.drawPixel(x, y + 1, borderRed);
  tft.drawPixel(x + 3, y + 1, borderRed);
  tft.drawPixel(x + 6, y + 1, borderRed);

  tft.drawPixel(x, y + 2, borderRed);
  tft.drawPixel(x + 6, y + 2, borderRed);

  tft.drawPixel(x, y + 3, borderRed);
  tft.drawPixel(x + 6, y + 3, borderRed);

  tft.drawPixel(x + 1, y + 4, borderRed);
  tft.drawPixel(x + 5, y + 4, borderRed);

  tft.drawPixel(x + 2, y + 5, borderRed);
  tft.drawPixel(x + 4, y + 5, borderRed);

  tft.drawPixel(x + 3, y + 6, borderRed);

  // Draw Fill
  if (state == HEART_FULL) {
    tft.drawPixel(x + 1, y + 1, fillRed);
    tft.drawPixel(x + 2, y + 1, fillRed);
    tft.drawPixel(x + 4, y + 1, fillRed);
    tft.drawPixel(x + 5, y + 1, fillRed);

    tft.fillRect(x + 1, y + 2, 5, 1, fillRed);
    tft.fillRect(x + 1, y + 3, 5, 1, fillRed);
    tft.fillRect(x + 2, y + 4, 3, 1, fillRed);
    tft.drawPixel(x + 3, y + 5, fillRed);
  } else if (state == HEART_HALF) {
    tft.drawPixel(x + 1, y + 1, fillRed);
    tft.drawPixel(x + 2, y + 1, fillRed);

    tft.fillRect(x + 1, y + 2, 3, 1, fillRed);
    tft.fillRect(x + 1, y + 3, 3, 1, fillRed);
    tft.fillRect(x + 2, y + 4, 2, 1, fillRed);
    tft.drawPixel(x + 3, y + 5, fillRed);
  }
}

static void drawScoreHUD(int scoreVal) {
  tft.setFont(&BLADRMF_4pt7b);
  tft.setTextSize(1);
  tft.setTextColor(tft.color565(200, 200, 200)); // Off-white/light gray
  
  // Format score: SCORE 00000
  char scoreBuf[20];
  snprintf(scoreBuf, sizeof(scoreBuf), "SCORE %05d", scoreVal);
  
  // Measure text width
  int16_t x1, y1;
  uint16_t w, h;
  tft.getTextBounds(scoreBuf, 0, 0, &x1, &y1, &w, &h);
  
  int16_t sx = 128 - w - 4; // 4 pixels margin from right
  int16_t sy = 9;           // baseline Y (fits in Y=3..9)
  
  // Clear only the score text box to black to prevent flicker
  tft.fillRect(sx, 1, w, 11, ST77XX_BLACK);
  
  tft.setCursor(sx, sy);
  tft.print(scoreBuf);
}

static void drawGameplayHUD() {
  if (playerHealth != lastDrawHealth) {
    drawHeart(4, 3, (playerHealth >= 1) ? HEART_FULL : HEART_EMPTY);
    drawHeart(14, 3, (playerHealth >= 2) ? HEART_FULL : HEART_EMPTY);
    drawHeart(24, 3, (playerHealth >= 3) ? HEART_FULL : HEART_EMPTY);
    lastDrawHealth = playerHealth;
  }
  
  if (score != lastDrawScore) {
    drawScoreHUD(score);
    lastDrawScore = score;
  }
}

static void drawHUDSeparator() {
  const uint16_t lineCol = tft.color565(82, 103, 121); // Blue-gray
  tft.drawFastHLine(4, 13, 30, lineCol);
  tft.drawFastHLine(44, 13, 40, lineCol);
  tft.drawFastHLine(94, 13, 30, lineCol);
}
#endif