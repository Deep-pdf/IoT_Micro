#pragma once
#include "PixelWarsShared.h"

void initStarfield();
void updateStarfield(uint32_t dt, float speedMultiplier);
void drawStar(const Star &s);
void eraseStar(const Star &s);
