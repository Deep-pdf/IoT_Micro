#include "PixelWars.h"
#include "PixelWarsShared.h"
#include "PixelWarsMenu.h"
#include "PixelWarsBackground.h"
#include "PixelWarsPlayer.h"
#include "PixelWarsProjectiles.h"
#include "PixelWarsEnemies.h"
#include "PixelWarsGame.h"
#include "button.h"
#include <Preferences.h>

// Definitions of all extern variables declared in PixelWarsShared.h
unsigned long pixelWarsLoadStartTime = 0;
int lastProgress = -1;
int pwMenuSelectedIndex = 0;
int lastPwMenuSelectedIndex = -1;

unsigned long countdownStartTime = 0;
int lastCountdownNumber = -1;
float playerX = 56.0f;
float playerY = 120.0f;
float prevPlayerX = 56.0f;
float prevPlayerY = 120.0f;
unsigned long lastGameplayFrameTime = 0;

Projectile projectiles[MAX_PROJECTILES];
Enemy enemies[MAX_ENEMIES];
EnemyProjectile enemyProjectiles[MAX_ENEMY_PROJECTILES];
Explosion explosions[MAX_EXPLOSIONS];
Bomb bombs[MAX_BOMBS];
HeartDrop heartDrops[MAX_HEART_DROPS];

Star farStars[STAR_COUNT_FAR];
Star midStars[STAR_COUNT_MID];
Star nearStars[STAR_COUNT_NEAR];

float STARFIELD_SPEED_FAR = 0.02f;
float STARFIELD_SPEED_MID = 0.05f;
float STARFIELD_SPEED_NEAR = 0.12f;

float currentPaceMultiplier = 1.0f;
float targetPaceMultiplier = 1.0f;
float basePaceMultiplier = 1.0f;
float targetBasePaceMultiplier = 1.0f;
unsigned long pacePhaseStartTime = 0;
unsigned long pacePhaseDuration = 0;
uint8_t currentPacePhase = 1;
unsigned long gameStartTime = 0;

int playerHealth = 6;
int score = 0;
int lastDrawHealth = -1;
int lastDrawScore = -1;
int highScore = 0;
static Preferences prefs;

bool playerHeartBlinkActive = false;
unsigned long playerHeartBlinkStartTime = 0;
bool playerDamageBlinkActive = false;
unsigned long playerDamageBlinkStartTime = 0;

bool playerDefeated = false;
unsigned long defeatTime = 0;
bool playerGameOverDrawn = false;

unsigned long lastSpawnActionTime = 0;
int spawnSequenceIndex = 0;
int randomSpawnCount = 0;


PixelWars pixelWars;

void PixelWars::begin() {
  // Load High Score from Preferences
  prefs.begin("pixelwars", false);
  highScore = prefs.getInt("highscore", 0);
  prefs.end();
}

void PixelWars::update() {
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

  if (currentScreen == STATE_PIXEL_WARS_HIGH_SCORE) {
    if (isBackPressed() || isEnterPressed()) {
      currentScreen = STATE_PIXEL_WARS_MENU;
      pwMenuSelectedIndex = 1; // Stay on high score menu item
      lastPwMenuSelectedIndex = -1;
      drawPixelWarsStartMenu();
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
      
      // Reset pacing/time variables on start
      gameStartTime = millis();
      basePaceMultiplier = 1.0f;
      targetBasePaceMultiplier = 1.0f;
      currentPaceMultiplier = 0.5f;
      targetPaceMultiplier = 0.5f;
      playerHeartBlinkActive = false;
      playerDamageBlinkActive = false;
      playerHealth = 6;
      playerDefeated = false;
      playerGameOverDrawn = false;
      
      // Reset HUD draw flags
      lastDrawHealth = -1;
      lastDrawScore = -1;

      // Erase countdown text box, draw HUD, and draw player ship
      tft.fillRect(34, 68, 60, 24, ST77XX_BLACK);
      drawGameplayHUD();
      drawPlayerShip((int16_t)playerX, (int16_t)playerY);
    }
    return;
  }

  if (currentScreen == STATE_PIXEL_WARS_GAMEPLAY) {
    runGameplayUpdate();
  }
}
