#pragma once
#include "PixelWarsShared.h"

void spawnProjectile();
void updateProjectiles(uint32_t dt);
void clearProjectiles();

void spawnEnemyProjectile(float x, float y, float vx, float vy, uint8_t type);
void updateEnemyProjectiles(uint32_t dt);
void drawEnemyProjectile(int16_t x, int16_t y, uint8_t type, bool erase);
