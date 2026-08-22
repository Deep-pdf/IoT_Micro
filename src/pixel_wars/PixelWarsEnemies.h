#pragma once
#include "PixelWarsShared.h"

bool spawnEnemy(float x, float y, float vx, float vy, uint8_t type);
void updateEnemies(uint32_t dt);
void eraseEnemy(int16_t x, int16_t y, uint8_t type);
void drawEnemy(int16_t x, int16_t y, uint8_t type, bool flashRed);

// Type-specific draw functions
void drawEnemyType1(int16_t x, int16_t y);
void drawEnemyType2(int16_t x, int16_t y);
void drawEnemyType3(int16_t x, int16_t y, bool flashRed);
void drawEnemyType4(int16_t x, int16_t y, bool flashRed);
