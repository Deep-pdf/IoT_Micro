#pragma once
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include "home_screen.h" // For ScreenState
#include <Arduino.h>

// Shared display reference
extern Adafruit_ST7735 tft;

// Constants
#define MAX_PROJECTILES 5
#define MAX_ENEMIES 20
#define MAX_ENEMY_PROJECTILES 12
#define MAX_EXPLOSIONS 8
#define MAX_BOMBS 5
#define MAX_HEART_DROPS 3

#define STAR_COUNT_FAR 35
#define STAR_COUNT_MID 25
#define STAR_COUNT_NEAR 12

#define HEART_FULL 0
#define HEART_HALF 1
#define HEART_EMPTY 2

#define STYLE_STRAIGHT 0
#define STYLE_DRIFT 1
#define STYLE_OSCILLATE 2
#define STYLE_ZIGZAG 3
#define STYLE_STEP 4

// Structs
struct Projectile {
  float x;
  float y;
  float prevX;
  float prevY;
  bool active;
};

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

struct Explosion {
  int16_t cx;
  int16_t cy;
  unsigned long startTime;
  uint8_t lastStage;
  bool active;
};

struct Bomb {
  float x;
  float y;
  float prevX;
  float prevY;
  float vx;
  float vy;
  bool active;
};

struct HeartDrop {
  float x;
  float y;
  float prevX;
  float prevY;
  float vx;
  float vy;
  bool active;
};

struct Star {
  float x;
  float y;
  float speed;
  uint16_t color;
  uint8_t type; // 0=1x1, 1=2x1, 2=2x2, 3=1x3 streak, 4=cross, 5=red particle
};

// Extern variables declarations
extern unsigned long pixelWarsLoadStartTime;
extern int lastProgress;
extern int pwMenuSelectedIndex;
extern int lastPwMenuSelectedIndex;

extern unsigned long countdownStartTime;
extern int lastCountdownNumber;
extern float playerX;
extern float playerY;
extern float prevPlayerX;
extern float prevPlayerY;
extern unsigned long lastGameplayFrameTime;

extern Projectile projectiles[MAX_PROJECTILES];
extern Enemy enemies[MAX_ENEMIES];
extern EnemyProjectile enemyProjectiles[MAX_ENEMY_PROJECTILES];
extern Explosion explosions[MAX_EXPLOSIONS];
extern Bomb bombs[MAX_BOMBS];
extern HeartDrop heartDrops[MAX_HEART_DROPS];

extern Star farStars[STAR_COUNT_FAR];
extern Star midStars[STAR_COUNT_MID];
extern Star nearStars[STAR_COUNT_NEAR];

extern float STARFIELD_SPEED_FAR;
extern float STARFIELD_SPEED_MID;
extern float STARFIELD_SPEED_NEAR;

extern float currentPaceMultiplier;
extern float targetPaceMultiplier;
extern float basePaceMultiplier;
extern float targetBasePaceMultiplier;
extern unsigned long pacePhaseStartTime;
extern unsigned long pacePhaseDuration;
extern uint8_t currentPacePhase;
extern unsigned long gameStartTime;

extern int playerHealth;
extern int score;
extern int lastDrawHealth;
extern int lastDrawScore;
extern int highScore;

extern bool playerHeartBlinkActive;
extern unsigned long playerHeartBlinkStartTime;
extern bool playerDamageBlinkActive;
extern unsigned long playerDamageBlinkStartTime;

extern bool playerDefeated;
extern unsigned long defeatTime;
extern bool playerGameOverDrawn;

extern unsigned long lastSpawnActionTime;
extern int spawnSequenceIndex;
extern int randomSpawnCount;

extern ScreenState currentScreen;

// Joystick pins reference for modules
#define JOY_X  34
#define JOY_Y  35
extern bool joystickCentered;
