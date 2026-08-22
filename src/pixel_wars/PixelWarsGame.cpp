#include "PixelWarsGame.h"
#include "PixelWarsPlayer.h"
#include "PixelWarsEnemies.h"
#include "PixelWarsProjectiles.h"
#include "PixelWarsBackground.h"
#include "PixelWarsMenu.h"
#include "PixelWars.h"
#include "button.h"
#include <Preferences.h>
#include "Dream_Orphans_Bd6pt7b.h"

static Preferences prefsLocal;

void resetEnemySystem() {
  if (random(2) == 999) { // dummy to suppress unused warning or keep debug seed choice
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
  
  clearBombsAndHearts();
  
  lastSpawnActionTime = millis();
  spawnSequenceIndex = 0;
  randomSpawnCount = 0;
  
  // Reset pacing
  basePaceMultiplier = 1.0f;
  targetBasePaceMultiplier = 1.0f;
  currentPaceMultiplier = 0.5f;
  targetPaceMultiplier = 0.5f;
  pacePhaseStartTime = millis();
  pacePhaseDuration = random(4000, 7001); // 4-7 seconds for first phase
  currentPacePhase = 1; // Start at MEDIUM
  gameStartTime = millis();
  playerHeartBlinkActive = false;
  playerDamageBlinkActive = false;

  // Reset HUD
  playerHealth = 6;
  playerDefeated = false;
  score = 0;
  lastDrawHealth = -1;
  lastDrawScore = -1;
}

void drawExplosionStage(int16_t cx, int16_t cy, uint8_t stage, bool erase) {
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

void updateExplosions() {
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

void checkAndSaveHighScore() {
  if (score > highScore) {
    highScore = score;
    prefsLocal.begin("pixelwars", false);
    prefsLocal.putInt("highscore", highScore);
    prefsLocal.end();
  }
}

void checkCollisions() {
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
              
              // Increment score based on enemy type
              if (enemies[e].type == 1) score += 10;
              else if (enemies[e].type == 2) score += 15;
              else if (enemies[e].type == 3) score += 20;
              else if (enemies[e].type == 4) score += 25;
              if (score > 99999) score = 99999; // HUD max display clamp
              
              // Heart drop opportunity: 8-15% chance (12% selected)
              if (random(100) < 12) {
                float hx = ex + ew / 2.0f;
                float hy = ey + eh / 2.0f;
                spawnHeartDrop(hx, hy);
              }
              
              tft.fillRect((int16_t)projectiles[p].x, (int16_t)projectiles[p].y, 2, 6, ST77XX_BLACK);
              projectiles[p].active = false;
              break;
            }
          }
        }
      }
    }
  }

  // Check collision: Player Ship vs Enemy Planes
  for (int e = 0; e < MAX_ENEMIES; e++) {
    if (enemies[e].active) {
      float ex = enemies[e].x;
      float ey = enemies[e].y;
      float ew = 9.0f;
      float eh = 9.0f;
      if (enemies[e].type == 2) { ew = 7.0f; eh = 11.0f; }
      else if (enemies[e].type == 3) { ew = 13.0f; eh = 11.0f; }
      else if (enemies[e].type == 4) { ew = 9.0f; eh = 9.0f; }
      
      if (ex < playerX + 15.0f && ex + ew > playerX && ey < playerY + 17.0f && ey + eh > playerY) {
        // Destroy the enemy plane
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
        
        // Subtract half life (1 half heart = subtract 1)
        if (playerHealth > 0) {
          playerHealth--;
          if (playerHealth <= 0) {
            triggerDefeat();
          }
        }
        
        // Trigger damage blink
        playerDamageBlinkActive = true;
        playerDamageBlinkStartTime = millis();
      }
    }
  }
}

void spawnGridFormation() {
  uint8_t typeFront = random(1, 3);
  uint8_t typeBack = random(2, 5); // 2, 3, or 4
  float vy = 0.025f;
  
  spawnEnemy(29.0f, -15.0f, 0.0f, vy, typeFront);
  spawnEnemy(64.0f, -15.0f, 0.0f, vy, typeFront);
  spawnEnemy(99.0f, -15.0f, 0.0f, vy, typeFront);
  
  if (score >= 100) {
    spawnEnemy(29.0f, -35.0f, 0.0f, vy, typeBack);
    if (score >= 200) {
      spawnEnemy(64.0f, -35.0f, 0.0f, vy, typeBack);
    }
    spawnEnemy(99.0f, -35.0f, 0.0f, vy, typeBack);
  }
}

void spawnVFormation() {
  float vy = 0.028f;
  spawnEnemy(58.0f, -15.0f, 0.0f, vy, 3);
  spawnEnemy(40.0f, -30.0f, 0.0f, vy, 1);
  spawnEnemy(80.0f, -30.0f, 0.0f, vy, 1);
  if (score >= 100) {
    spawnEnemy(21.0f, -45.0f, 0.0f, vy, 2);
    if (score >= 200) {
      spawnEnemy(101.0f, -45.0f, 0.0f, vy, 2);
    }
  }
}

void spawnRandomCluster() {
  float cx = random(35, 90);
  float cy = random(-40, -20);
  int count = random(4, 7);
  if (score < 100) {
    count = random(2, 4);
  } else if (score < 200) {
    count = random(3, 5);
  }
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

void spawnDiamondFormation() {
  float vy = 0.025f;
  spawnEnemy(60.0f, -55.0f, 0.0f, vy, 1);
  spawnEnemy(41.0f, -35.0f, 0.0f, vy, 2);
  spawnEnemy(81.0f, -35.0f, 0.0f, vy, 2);
  if (score >= 100) {
    spawnEnemy(58.0f, -35.0f, 0.0f, vy, 3);
    if (score >= 200) {
      spawnEnemy(60.0f, -15.0f, 0.0f, vy, 1);
    }
  }
}

void spawnLineFormation() {
  float vy = 0.035f;
  spawnEnemy(13.0f, -15.0f, 0.0f, vy, 2);
  spawnEnemy(61.0f, -15.0f, 0.0f, vy, 2);
  spawnEnemy(109.0f, -15.0f, 0.0f, vy, 2);
  if (score >= 100) {
    spawnEnemy(37.0f, -15.0f, 0.0f, vy, 2);
    if (score >= 200) {
      spawnEnemy(85.0f, -15.0f, 0.0f, vy, 2);
    }
  }
}

void spawnStaggeredFormation() {
  float vy = 0.026f;
  spawnEnemy(11.0f, -15.0f, 0.0f, vy, 1);
  spawnEnemy(51.0f, -15.0f, 0.0f, vy, 1);
  spawnEnemy(91.0f, -15.0f, 0.0f, vy, 1);
  if (score >= 100) {
    spawnEnemy(32.0f, -35.0f, 0.0f, vy, 2);
    if (score >= 200) {
      spawnEnemy(72.0f, -35.0f, 0.0f, vy, 2);
      spawnEnemy(112.0f, -35.0f, 0.0f, vy, 2);
    }
  }
}

void spawnFormation(int seqIndex) {
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

void updateSpawnManager() {
  unsigned long now = millis();
  
  int activeCount = 0;
  for (int i = 0; i < MAX_ENEMIES; i++) {
    if (enemies[i].active) activeCount++;
  }

  int maxRandomSpawn = 3;
  if (score < 100) maxRandomSpawn = 1;
  else if (score < 200) maxRandomSpawn = 2;

  if (spawnSequenceIndex == 1 || spawnSequenceIndex == 5) {
    if (randomSpawnCount < maxRandomSpawn) {
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

void drawHeart(int16_t x, int16_t y, uint8_t state) {
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

void drawScoreHUD(int scoreVal) {
  tft.setFont(NULL); // Use default readable font
  tft.setTextSize(1);
  tft.setTextColor(tft.color565(200, 200, 200)); // Off-white/light gray
  
  // Format score: SCORE 00000
  char scoreBuf[20];
  snprintf(scoreBuf, sizeof(scoreBuf), "SCORE %05d", scoreVal);
  
  // Measure text width for default font
  int16_t w = strlen(scoreBuf) * 6 - 1;
  int16_t sx = 128 - w - 4; // 4 pixels margin from right
  int16_t sy = 3;           // Top margin 3 pixels (aligned with hearts)
  
  // Clear only the score text box to black to prevent flicker
  tft.fillRect(sx, sy, w, 8, ST77XX_BLACK);
  
  tft.setCursor(sx, sy);
  tft.print(scoreBuf);
}

void drawGameplayHUD() {
  if (playerHealth != lastDrawHealth) {
    drawHeart(4, 3, (playerHealth >= 2) ? HEART_FULL : ((playerHealth >= 1) ? HEART_HALF : HEART_EMPTY));
    drawHeart(14, 3, (playerHealth >= 4) ? HEART_FULL : ((playerHealth >= 3) ? HEART_HALF : HEART_EMPTY));
    drawHeart(24, 3, (playerHealth >= 6) ? HEART_FULL : ((playerHealth >= 5) ? HEART_HALF : HEART_EMPTY));
    lastDrawHealth = playerHealth;
  }
  
  if (score != lastDrawScore) {
    drawScoreHUD(score);
    lastDrawScore = score;
  }
}

bool spawnBomb(float x, float y) {
  for (int i = 0; i < MAX_BOMBS; i++) {
    if (!bombs[i].active) {
      bombs[i].x = x + (random(7) - 3); // random starting offset
      if (bombs[i].x < 0.0f) bombs[i].x = 0.0f;
      if (bombs[i].x > 120.0f) bombs[i].x = 120.0f;
      bombs[i].y = y;
      bombs[i].prevX = bombs[i].x;
      bombs[i].prevY = bombs[i].y;
      
      // Randomize drift: left, right, almost straight
      int driftChoice = random(3);
      if (driftChoice == 0) {
        bombs[i].vx = -0.012f - (random(10) / 1000.0f); // left
      } else if (driftChoice == 1) {
        bombs[i].vx = 0.012f + (random(10) / 1000.0f);  // right
      } else {
        bombs[i].vx = (random(6) - 3) / 1000.0f;        // almost straight
      }
      
      // Randomize speed: slightly slow, normal, slightly faster
      int speedChoice = random(3);
      if (speedChoice == 0) {
        bombs[i].vy = 0.025f + (random(8) / 1000.0f);   // slow
      } else if (speedChoice == 1) {
        bombs[i].vy = 0.04f + (random(8) / 1000.0f);    // normal
      } else {
        bombs[i].vy = 0.055f + (random(12) / 1000.0f);  // faster
      }
      
      bombs[i].active = true;
      return true;
    }
  }
  return false;
}

bool spawnHeartDrop(float x, float y) {
  for (int i = 0; i < MAX_HEART_DROPS; i++) {
    if (!heartDrops[i].active) {
      heartDrops[i].x = x + (random(7) - 3); // initial X offset
      if (heartDrops[i].x < 0.0f) heartDrops[i].x = 0.0f;
      if (heartDrops[i].x > 121.0f) heartDrops[i].x = 121.0f;
      heartDrops[i].y = y;
      heartDrops[i].prevX = heartDrops[i].x;
      heartDrops[i].prevY = heartDrops[i].y;
      
      // Randomize initial x offset / horizontal drift
      int driftChoice = random(3);
      if (driftChoice == 0) {
        heartDrops[i].vx = -0.01f - (random(8) / 1000.0f);
      } else if (driftChoice == 1) {
        heartDrops[i].vx = 0.01f + (random(8) / 1000.0f);
      } else {
        heartDrops[i].vx = (random(4) - 2) / 1000.0f;
      }
      
      // Randomize speed: slightly slow, normal, slightly faster
      int speedChoice = random(3);
      if (speedChoice == 0) {
        heartDrops[i].vy = 0.02f + (random(8) / 1000.0f);
      } else if (speedChoice == 1) {
        heartDrops[i].vy = 0.035f + (random(8) / 1000.0f);
      } else {
        heartDrops[i].vy = 0.05f + (random(10) / 1000.0f);
      }
      
      heartDrops[i].active = true;
      return true;
    }
  }
  return false;
}

void updateBombs(uint32_t dt) {
  for (int i = 0; i < MAX_BOMBS; i++) {
    if (bombs[i].active) {
      if (bombs[i].prevY >= 14.0f) {
        drawBomb((int16_t)bombs[i].prevX, (int16_t)bombs[i].prevY, true);
      }
      
      bombs[i].x += bombs[i].vx * currentPaceMultiplier * dt;
      bombs[i].y += bombs[i].vy * currentPaceMultiplier * dt;
      
      // Check collision/proximity with player ship
      float bx = bombs[i].x + 4.0f;
      float by = bombs[i].y + 4.0f;
      float px = playerX + 7.5f;
      float py = playerY + 8.5f;
      float distSq = (bx - px) * (bx - px) + (by - py) * (by - py);
      
      if (distSq < 132.0f) { // Proximity distance < ~11.5 pixels
        bombs[i].active = false;
        
        // Spawn explosion animation
        for (int x = 0; x < MAX_EXPLOSIONS; x++) {
          if (!explosions[x].active) {
            explosions[x].cx = (int16_t)bx;
            explosions[x].cy = (int16_t)by;
            explosions[x].startTime = millis();
            explosions[x].lastStage = 0;
            explosions[x].active = true;
            break;
          }
        }
        
        // Decrease player health by 2 (full heart)
        if (playerHealth > 0) {
          playerHealth -= 2;
          if (playerHealth < 0) playerHealth = 0;
          if (playerHealth <= 0) {
            triggerDefeat();
          }
        }
        
        // Trigger damage blink
        playerDamageBlinkActive = true;
        playerDamageBlinkStartTime = millis();
        continue;
      }
      
      // Clamp x to stay within screen boundaries
      if (bombs[i].x < 0.0f) {
        bombs[i].x = 0.0f;
        bombs[i].vx = -bombs[i].vx * 0.5f; // simple bounce
      } else if (bombs[i].x > 120.0f) {
        bombs[i].x = 120.0f;
        bombs[i].vx = -bombs[i].vx * 0.5f;
      }
      
      if (bombs[i].y >= 160.0f) {
        bombs[i].active = false;
      } else {
        if (bombs[i].y >= 14.0f) {
          drawBomb((int16_t)bombs[i].x, (int16_t)bombs[i].y, false);
        }
        bombs[i].prevX = bombs[i].x;
        bombs[i].prevY = bombs[i].y;
      }
    }
  }
}

void updateHeartDrops(uint32_t dt) {
  for (int i = 0; i < MAX_HEART_DROPS; i++) {
    if (heartDrops[i].active) {
      if (heartDrops[i].prevY >= 14.0f) {
        drawHeartDrop((int16_t)heartDrops[i].prevX, (int16_t)heartDrops[i].prevY, true);
      }
      
      heartDrops[i].x += heartDrops[i].vx * currentPaceMultiplier * dt;
      heartDrops[i].y += heartDrops[i].vy * currentPaceMultiplier * dt;
      
      // Check collision with player ship (15x17 box at playerX, playerY)
      // Heart drop bounding box is 7x7
      if (heartDrops[i].x < playerX + 15.0f && heartDrops[i].x + 7.0f > playerX &&
          heartDrops[i].y < playerY + 17.0f && heartDrops[i].y + 7.0f > playerY) {
        heartDrops[i].active = false;
        if (playerHealth < 6) {
          playerHealth += 2;
          if (playerHealth > 6) playerHealth = 6;
        }
        playerHeartBlinkActive = true;
        playerHeartBlinkStartTime = millis();
        continue;
      }
      
      // Clamp x to stay within screen boundaries
      if (heartDrops[i].x < 0.0f) {
        heartDrops[i].x = 0.0f;
        heartDrops[i].vx = -heartDrops[i].vx * 0.5f; // simple bounce
      } else if (heartDrops[i].x > 121.0f) {
        heartDrops[i].x = 121.0f;
        heartDrops[i].vx = -heartDrops[i].vx * 0.5f;
      }
      
      if (heartDrops[i].y >= 160.0f) {
        heartDrops[i].active = false;
      } else {
        if (heartDrops[i].y >= 14.0f) {
          drawHeartDrop((int16_t)heartDrops[i].x, (int16_t)heartDrops[i].y, false);
        }
        heartDrops[i].prevX = heartDrops[i].x;
        heartDrops[i].prevY = heartDrops[i].y;
      }
    }
  }
}

void clearBombsAndHearts() {
  for (int i = 0; i < MAX_BOMBS; i++) {
    bombs[i].active = false;
  }
  for (int i = 0; i < MAX_HEART_DROPS; i++) {
    heartDrops[i].active = false;
  }
}

void drawBomb(int16_t x, int16_t y, bool erase) {
  if (erase) {
    tft.fillRect(x, y, 8, 8, ST77XX_BLACK);
    return;
  }
  
  uint16_t bodyColor = tft.color565(90, 30, 20); // Dark red/brown
  uint16_t darkGray = tft.color565(60, 60, 60);  // Dark gray/iron
  uint16_t sparkColor = ((millis() / 120) % 2 == 0) ? tft.color565(255, 60, 0) : tft.color565(255, 180, 0); // Pulsing orange-red/yellow
  uint16_t fuseGray = tft.color565(120, 100, 80);

  // Row 0: Spark/Fuse
  tft.drawPixel(x + 4, y, sparkColor);
  
  // Row 1: Fuse line
  tft.drawPixel(x + 4, y + 1, fuseGray);

  // Row 2: Bomb collar / top
  tft.fillRect(x + 3, y + 2, 2, 1, darkGray);

  // Row 3: Bomb body
  tft.fillRect(x + 2, y + 3, 4, 1, bodyColor);
  
  // Row 4: Bomb body middle
  tft.fillRect(x + 1, y + 4, 6, 1, bodyColor);
  
  // Row 5: Bomb body middle
  tft.fillRect(x + 1, y + 5, 6, 1, bodyColor);

  // Row 6: Bomb body lower
  tft.fillRect(x + 2, y + 6, 4, 1, bodyColor);

  // Row 7: Bomb body bottom point
  tft.fillRect(x + 3, y + 7, 2, 1, bodyColor);
}

void drawHeartDrop(int16_t x, int16_t y, bool erase) {
  if (erase) {
    tft.fillRect(x, y, 7, 7, ST77XX_BLACK);
    return;
  }
  uint16_t borderRed = tft.color565(200, 0, 0); // Brighter outline
  uint16_t fillRed = tft.color565(255, 50, 50);    // Brighter red fill
  uint16_t highlight = ST77XX_WHITE;
  
  // Draw border/shape in borderRed
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

  // Fill in the middle
  tft.drawPixel(x + 1, y + 1, fillRed);
  tft.drawPixel(x + 2, y + 1, fillRed);
  tft.drawPixel(x + 4, y + 1, fillRed);
  tft.drawPixel(x + 5, y + 1, fillRed);

  tft.fillRect(x + 1, y + 2, 5, 1, fillRed);
  tft.fillRect(x + 1, y + 3, 5, 1, fillRed);
  tft.fillRect(x + 2, y + 4, 3, 1, fillRed);
  tft.drawPixel(x + 3, y + 5, fillRed);

  // Tiny blinking pixel / highlight
  bool blink = (millis() / 250) % 2 == 0;
  if (blink) {
    tft.drawPixel(x + 2, y + 2, highlight);
  } else {
    tft.drawPixel(x + 2, y + 2, fillRed);
  }
}

void updateGlobalPace(uint32_t dt) {
  unsigned long now = millis();
  basePaceMultiplier += (targetBasePaceMultiplier - basePaceMultiplier) * 0.0005f * dt;
  if (basePaceMultiplier < 0.5f) basePaceMultiplier = 0.5f;
  if (basePaceMultiplier > 2.0f) basePaceMultiplier = 2.0f;

  if (now - pacePhaseStartTime >= pacePhaseDuration) {
    uint8_t nextPhase = random(3);
    while (nextPhase == currentPacePhase) {
      nextPhase = random(3);
    }
    currentPacePhase = nextPhase;
    
    if (currentPacePhase == 0) {
      targetBasePaceMultiplier = 0.75f;
      pacePhaseDuration = random(3000, 6001);
    } else if (currentPacePhase == 1) {
      targetBasePaceMultiplier = 1.0f;
      pacePhaseDuration = random(4000, 8001);
    } else {
      targetBasePaceMultiplier = 1.30f;
      pacePhaseDuration = random(2000, 5001);
    }
    pacePhaseStartTime = now;
  }

  // Calculate gradual speed progression factor
  float elapsed = (float)(now - gameStartTime) / 60000.0f; // 60s ramp
  if (elapsed > 1.0f) elapsed = 1.0f;
  float scoreProg = (float)score / 500.0f;
  if (scoreProg > 1.0f) scoreProg = 1.0f;
  float progression = elapsed > scoreProg ? elapsed : scoreProg;
  
  // Progression scales from 0.5 to 1.0
  float factor = 0.5f + 0.5f * progression;
  currentPaceMultiplier = basePaceMultiplier * factor;
}

void runGameplayUpdate() {
  if (playerDefeated) {
    unsigned long now = millis();
    
    // Phase 1: Wait 800ms for player explosion animation to complete
    if (now - defeatTime < 800) {
      unsigned long dt = now - lastGameplayFrameTime;
      if (dt > 100) dt = 100;
      lastGameplayFrameTime = now;
      
      updateStarfield(dt, 0.2f);
      updateExplosions();
    } 
    // Phase 2: Draw the Game Over modal once, and wait for input
    else {
      if (!playerGameOverDrawn) {
        // Draw a beautiful Game Over modal box
        tft.fillRect(9, 36, 110, 88, ST77XX_BLACK);
        tft.drawRect(9, 36, 110, 88, tft.color565(255, 32, 21)); // Red border
        tft.drawRect(11, 38, 106, 84, tft.color565(120, 20, 20)); // Inner red border
        
        tft.setFont(&Dream_Orphans_Bd6pt7b);
        tft.setTextColor(tft.color565(255, 32, 21));
        tft.setTextSize(1);
        int16_t x1, y1;
        uint16_t w, h;
        tft.getTextBounds("GAME OVER", 0, 0, &x1, &y1, &w, &h);
        tft.setCursor((128 - w) / 2 - x1, 54);
        tft.print("GAME OVER");
        
        tft.setFont(NULL); // Reset to default clean font
        
        char scoreBuf[25];
        snprintf(scoreBuf, sizeof(scoreBuf), "SCORE: %05d", score);
        drawCenteredPWText(tft, scoreBuf, 72, tft.color565(243, 237, 224), 1, true);
        
        drawCenteredPWText(tft, "[ENT] RETRY", 90, tft.color565(50, 220, 80), 1, false);
        drawCenteredPWText(tft, "[BACK] EXIT", 104, tft.color565(255, 70, 70), 1, false);
        
        playerGameOverDrawn = true;
      }
      
      // Handle input
      if (isEnterPressed()) {
        // Retry Game
        playerDefeated = false;
        playerGameOverDrawn = false;
        resetEnemySystem();
        clearProjectiles();
        
        currentScreen = STATE_PIXEL_WARS_COUNTDOWN;
        countdownStartTime = millis();
        lastCountdownNumber = -1;
        lastGameplayFrameTime = millis();
        initStarfield();
        
        tft.fillScreen(ST77XX_BLACK);
        for (int i = 0; i < STAR_COUNT_FAR; i++) drawStar(farStars[i]);
        for (int i = 0; i < STAR_COUNT_MID; i++) drawStar(midStars[i]);
        for (int i = 0; i < STAR_COUNT_NEAR; i++) drawStar(nearStars[i]);
      } 
      else if (isBackPressed()) {
        // Exit to Menu
        playerDefeated = false;
        playerGameOverDrawn = false;
        resetEnemySystem();
        clearProjectiles();
        
        currentScreen = STATE_PIXEL_WARS_MENU;
        pwMenuSelectedIndex = 0;
        lastPwMenuSelectedIndex = -1;
        drawPixelWarsStartMenu();
      }
    }
    return;
  }

  if (isBackPressed()) {
    checkAndSaveHighScore();
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
    for (int i = 0; i < MAX_BOMBS; i++) {
      if (bombs[i].active) {
        drawBomb((int16_t)bombs[i].x, (int16_t)bombs[i].y, true);
      }
    }
    for (int i = 0; i < MAX_HEART_DROPS; i++) {
      if (heartDrops[i].active) {
        drawHeartDrop((int16_t)heartDrops[i].x, (int16_t)heartDrops[i].y, true);
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

  // 1. Update global pacing
  updateGlobalPace(dt);

  // 2. Update scrolling background
  updateStarfield(dt, 1.0f * currentPaceMultiplier);

  // 3 & 4. Spawn/update enemies and move them
  updateEnemies(dt);

  // Update bombs
  updateBombs(dt);

  // 5 & 6. Spawn/update enemy projectiles
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

  // Update heart drops
  updateHeartDrops(dt);

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

  // Redraw player ship if moved or if an active enemy overlaps its bounding box, or if blinking
  bool forcePlayerRedraw = false;
  if (playerHeartBlinkActive || playerDamageBlinkActive) {
    forcePlayerRedraw = true;
  }
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
}
