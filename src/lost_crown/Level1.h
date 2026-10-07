/**
 * Level1.h
 *
 * Lost Crown Level 1 Exploration & Platforming Scene.
 * Implements side-scrolling camera, multi-tier platforms, sloped stairs,
 * and full character movement integration matching assets/level1_ref.png.
 */
#pragma once

#include <Adafruit_ST7735.h>
#include "LostCrownConfig.h"
#include "Level1Collision.h"

namespace LostCrownLevel1 {

  // Initializes Level 1: resets player at left entrance, sets camera, draws initial frame
  void begin(Adafruit_ST7735 &tft);

  // Per-frame non-blocking update: physics, camera follow, rendering
  void update(Adafruit_ST7735 &tft);

  // Resets level state
  void reset();

  // Returns true if player held back button for 3s to return to Title
  bool shouldExit();

  // State inspection accessors
  float getPlayerWorldX();
  float getPlayerWorldY();
  float getCameraX();
  float getCameraY();
  bool isGrounded();

} // namespace LostCrownLevel1
