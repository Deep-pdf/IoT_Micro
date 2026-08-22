#include "PixelWarsPlayer.h"
#include "PixelWarsGame.h"

void drawPlayerShip(int16_t px, int16_t py) {
  bool usePinkTint = false;
  if (playerHeartBlinkActive) {
    unsigned long elapsed = millis() - playerHeartBlinkStartTime;
    if (elapsed >= 400) {
      playerHeartBlinkActive = false;
    } else {
      if ((elapsed / 100) % 2 == 0) {
        usePinkTint = true;
      }
    }
  }

  bool useRedTint = false;
  if (playerDamageBlinkActive) {
    unsigned long elapsed = millis() - playerDamageBlinkStartTime;
    if (elapsed >= 400) {
      playerDamageBlinkActive = false;
    } else {
      if ((elapsed / 100) % 2 == 0) {
        useRedTint = true;
      }
    }
  }

  const uint16_t primaryRed = usePinkTint ? tft.color565(255, 120, 160) : (useRedTint ? tft.color565(255, 0, 0) : tft.color565(255, 32, 21));
  const uint16_t orangeGlow = usePinkTint ? tft.color565(255, 160, 180) : (useRedTint ? tft.color565(200, 50, 0) : tft.color565(255, 74, 31));
  const uint16_t yellowGlow = usePinkTint ? tft.color565(255, 200, 220) : (useRedTint ? tft.color565(150, 30, 0) : tft.color565(255, 180, 0));
  const uint16_t mainWhite = usePinkTint ? tft.color565(255, 180, 200) : (useRedTint ? tft.color565(200, 50, 50) : tft.color565(243, 237, 224));
  const uint16_t darkGray = usePinkTint ? tft.color565(120, 80, 100) : (useRedTint ? tft.color565(80, 20, 20) : tft.color565(36, 43, 49));
  const uint16_t shipBlue = usePinkTint ? tft.color565(200, 100, 150) : (useRedTint ? tft.color565(150, 0, 0) : tft.color565(23, 105, 168));

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

void triggerDefeat() {
  playerDefeated = true;
  defeatTime = millis();
  playerGameOverDrawn = false;
  checkAndSaveHighScore();
  tft.fillRect((int16_t)playerX, (int16_t)playerY, 15, 17, ST77XX_BLACK);
  
  // Spawn player explosion
  for (int x = 0; x < MAX_EXPLOSIONS; x++) {
    if (!explosions[x].active) {
      explosions[x].cx = (int16_t)playerX + 7;
      explosions[x].cy = (int16_t)playerY + 8;
      explosions[x].startTime = millis();
      explosions[x].lastStage = 0;
      explosions[x].active = true;
      break;
    }
  }
}
