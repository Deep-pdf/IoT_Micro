#pragma once
#include "PixelWarsShared.h"

void resetEnemySystem();
void updateExplosions();
void checkCollisions();
void updateSpawnManager();
void spawnFormation(int seqIndex);
void drawGameplayHUD();
void drawHeart(int16_t x, int16_t y, uint8_t state);
void drawScoreHUD(int scoreVal);
void updateBombs(uint32_t dt);
void updateHeartDrops(uint32_t dt);
void drawBomb(int16_t x, int16_t y, bool erase);
void drawHeartDrop(int16_t x, int16_t y, bool erase);
bool spawnBomb(float x, float y);
bool spawnHeartDrop(float x, float y);
void clearBombsAndHearts();
void updateGlobalPace(uint32_t dt);
void checkAndSaveHighScore();

// Complete gameplay update frame
void runGameplayUpdate();
