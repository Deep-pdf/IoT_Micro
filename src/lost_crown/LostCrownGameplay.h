/**
 * LostCrownGameplay.h
 *
 * Training Ground gameplay scene for Lost Crown.
 * Handles the static 128x160 Training Ground environment and Veera's
 * idle and 6-frame running animations with horizontal mirroring and foot anchoring.
 */
#pragma once

#include <Adafruit_ST7735.h>
#include "LostCrownConfig.h"

namespace LostCrownGameplay {

  // Veera character animation/behavior states
  enum VeeraState {
    VEERA_STATE_IDLE = 0,
    VEERA_STATE_RUNNING,
    VEERA_STATE_JUMPING
  };

  // Horizontal facing direction
  enum VeeraDirection {
    VEERA_DIR_RIGHT = 0,
    VEERA_DIR_LEFT
  };

  // Initializes the gameplay scene: draws background once, places Veera at spawn in idle state
  void begin(Adafruit_ST7735 &tft);

  // Non-blocking per-frame update loop
  void update(Adafruit_ST7735 &tft);

  // Resets player position, velocity, and animation frame
  void reset();

  // Accessors for state inspection
  VeeraState getState();
  VeeraDirection getDirection();
  bool isGrounded();
  bool isAttacking();
  uint8_t getAttackFrame();
  bool isAttackHitboxActive();
  float getPositionX();
  float getPositionY();

} // namespace LostCrownGameplay
