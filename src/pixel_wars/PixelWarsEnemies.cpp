#include "PixelWarsEnemies.h"
#include "PixelWarsGame.h"
#include "PixelWarsProjectiles.h"

// Tuning Constants
const unsigned long MIN_ATTACK_DELAY_BASIC = 2500;
const unsigned long MAX_ATTACK_DELAY_BASIC = 6000;
const unsigned long MIN_ATTACK_DELAY_FAST = 1800;
const unsigned long MAX_ATTACK_DELAY_FAST = 4500;
const unsigned long MIN_ATTACK_DELAY_HEAVY = 4000;
const unsigned long MAX_ATTACK_DELAY_HEAVY = 8000;
const unsigned long MIN_ATTACK_DELAY_SPECIAL = 3000;
const unsigned long MAX_ATTACK_DELAY_SPECIAL = 7000;

bool spawnEnemy(float x, float y, float vx, float vy, uint8_t type) {
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

void drawEnemyType1(int16_t x, int16_t y) {
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

void drawEnemyType2(int16_t x, int16_t y) {
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

void drawEnemyType3(int16_t x, int16_t y, bool flashRed) {
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

void drawEnemyType4(int16_t x, int16_t y, bool flashRed) {
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

void drawEnemy(int16_t x, int16_t y, uint8_t type, bool flashRed) {
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

void eraseEnemy(int16_t x, int16_t y, uint8_t type) {
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

void updateEnemies(uint32_t dt) {
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
            // Bomb drop opportunity: 5-12% chance (10% selected)
            if (random(100) < 10) {
              float bx = enemies[i].x + ((enemies[i].type == 2) ? 3.5f : 4.5f);
              float by = enemies[i].y + ((enemies[i].type == 2) ? 11.0f : 9.0f);
              spawnBomb(bx, by);
            }
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
