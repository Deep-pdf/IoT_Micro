#include "PixelWarsBackground.h"

void initStarfield() {
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

void eraseStar(const Star &s) {
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

void drawStar(const Star &s) {
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

void updateStarfield(uint32_t dt, float speedMultiplier) {
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
