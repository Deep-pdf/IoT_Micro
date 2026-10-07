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

  // Boomerang projectile flight states
  enum BoomerangState {
    BOOMERANG_INACTIVE = 0,
    BOOMERANG_OUTBOUND,
    BOOMERANG_RETURNING
  };

  // Veera throw animation progression states
  enum VeeraThrowState {
    THROW_NONE = 0,
    THROW_FRAME1, // Veera_throw1 (preparation)
    THROW_FRAME2, // Veera_throw2 (release)
    THROW_FRAME3, // Veera_throw3 (airborne hold pose)
    THROW_FRAME4  // Veera_throw4 (catch & recovery)
  };

  bool isThrowing();
  VeeraThrowState getThrowState();
  BoomerangState getBoomerangState();
  bool isBoomerangActive();
  float getBoomerangX();
  float getBoomerangY();
  bool shouldExit();
  bool hasExitedRight();

} // namespace LostCrownGameplay
