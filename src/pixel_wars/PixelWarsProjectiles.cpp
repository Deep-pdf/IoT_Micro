#include "PixelWarsProjectiles.h"
#include "PixelWarsPlayer.h"

void clearProjectiles() {
  for (int i = 0; i < MAX_PROJECTILES; i++) {
    projectiles[i].active = false;
  }
}

void spawnProjectile() {
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

void updateProjectiles(uint32_t dt) {
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

void spawnEnemyProjectile(float x, float y, float vx, float vy, uint8_t type) {
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

void drawEnemyProjectile(int16_t x, int16_t y, uint8_t type, bool erase) {
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

void updateEnemyProjectiles(uint32_t dt) {
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
      
      // Check collision with player ship (15x17 box at playerX, playerY)
      // Projectile bounding box: ~3x4
      if (enemyProjectiles[i].x < playerX + 15.0f && enemyProjectiles[i].x + 3.0f > playerX &&
          enemyProjectiles[i].y < playerY + 17.0f && enemyProjectiles[i].y + 4.0f > playerY) {
        enemyProjectiles[i].active = false;
        if (playerHealth > 0) {
          playerHealth--;
          if (playerHealth <= 0) {
            triggerDefeat();
          }
        }
        playerDamageBlinkActive = true;
        playerDamageBlinkStartTime = millis();
        continue;
      }
      
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
