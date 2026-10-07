/**
 * Level1.cpp
 *
 * Implementation of Lost Crown Level 1 Exploration & Platforming Scene.
 *
 * Features:
 *  - 512x240 World coordinate system rendered to 128x160 TFT camera viewport.
 *  - Authoritative visual environment generated from assets/level1_ref.png.
 *  - Smooth horizontal & vertical side-scrolling camera with follow dead-zone.
 *  - Real platforming physics & collision: ground, multi-tier platforms, and sloped stairs.
 *  - Veera character controller with running, jumping, attacking, and boomerang.
 *  - 100% flicker-free single-buffer compositing and blitting.
 */

#include "Level1.h"
#include "Level1Assets.h"
#include "Level1Collision.h"
#include "LostCrownGameAssets.h"
#include "LostCrownConfig.h"
#include "LostCrownGameplay.h"
#include "button.h"
#include <Adafruit_GFX.h>

#define JOY_X 34
#define JOY_Y 35

namespace LostCrownLevel1 {

// ─── Display & World Constants ───────────────────────────────────────────────
static constexpr int16_t SCREEN_W = 128;
static constexpr int16_t SCREEN_H = 160;

// Camera follow dead-zone thresholds (screen-space)
static constexpr float DEADZONE_LEFT   = 24.0f; // Left follow border
static constexpr float DEADZONE_RIGHT  = 68.0f; // Right follow border (128 - 24 - 36)
static constexpr float DEADZONE_TOP    = 45.0f; // Top follow border
static constexpr float DEADZONE_BOTTOM = 105.0f; // Bottom follow border

// ─── Camera State ────────────────────────────────────────────────────────────
static float camX = 0.0f;
static float camY = 80.0f; // Clamped to bottom at spawn to show ground floor

// ─── Player State ────────────────────────────────────────────────────────────
static float veeraWorldX = -20.0f; // Enters from the left
static float veeraWorldY = LEVEL1_DEFAULT_GROUND_Y - (float)VEERA_SPRITE_H; // 180.0f

static float velocityX = 0.0f;
static float velocityY = 0.0f;
static bool  isGroundedState = true;

static LostCrownGameplay::VeeraState     currentState     = LostCrownGameplay::VEERA_STATE_RUNNING;
static LostCrownGameplay::VeeraDirection currentDirection = LostCrownGameplay::VEERA_DIR_RIGHT;
static uint8_t currentRunFrame  = 0;
static uint8_t currentJumpFrame = 0;

// Sword attack state
static bool     isAttackingState   = false;
static uint8_t  currentAttackFrame = 0;
static uint32_t lastAttackTick     = 0;

// Boomerang throw action state
static bool                              isThrowingState   = false;
static LostCrownGameplay::VeeraThrowState currentThrowState = LostCrownGameplay::THROW_NONE;
static LostCrownGameplay::VeeraDirection throwFacingDir    = LostCrownGameplay::VEERA_DIR_RIGHT;
static uint32_t                          lastThrowTick     = 0;

// Boomerang flight & rotation state
static LostCrownGameplay::BoomerangState boomerangState           = LostCrownGameplay::BOOMERANG_INACTIVE;
static float                             boomerangWorldX          = 0.0f;
static float                             boomerangWorldY          = 0.0f;
static float                             boomerangTargetX         = 0.0f;
static uint8_t                           currentBoomerangRotFrame = 0;
static uint32_t                          lastBoomerangRotTick     = 0;

// Back button long-press detection
static bool     lastBackDownState  = false;
static uint32_t backPressStartTick = 0;
static bool     longPressTriggered = false;
static bool     shouldExitLevel    = false;

// Time tracking
static uint32_t lastAnimTick     = 0;
static uint32_t lastPhysicsTick  = 0;

// RAM backbuffer for 100% flicker-free atomic rendering (128 x 160 x 2 = 40,960 bytes)
static uint16_t screenBuffer[SCREEN_W * SCREEN_H];

// ─── Internal Rendering Routine ──────────────────────────────────────────────
static void renderFrame(Adafruit_ST7735 &tft) {
  int16_t icamX = (int16_t)roundf(camX);
  int16_t icamY = (int16_t)roundf(camY);

  if (icamX < 0) icamX = 0;
  if (icamX > LEVEL1_WORLD_W - SCREEN_W) icamX = LEVEL1_WORLD_W - SCREEN_W;
  if (icamY < 0) icamY = 0;
  if (icamY > LEVEL1_WORLD_H - SCREEN_H) icamY = LEVEL1_WORLD_H - SCREEN_H;

  // 1. Copy visible 128x160 viewport slice from PROGMEM world image into backbuffer
  for (int16_t r = 0; r < SCREEN_H; r++) {
    int32_t bgOffset = (int32_t)(icamY + r) * LEVEL1_WORLD_W + icamX;
    memcpy_P(&screenBuffer[r * SCREEN_W], &level1_bg_pixels[bgOffset], SCREEN_W * sizeof(uint16_t));
  }

  // 2. Composite Veera sprite into backbuffer
  int16_t spriteW = VEERA_SPRITE_W;
  int16_t spriteH = VEERA_SPRITE_H;
  int16_t drawScreenX = (int16_t)roundf(veeraWorldX) - icamX;
  int16_t drawScreenY = (int16_t)roundf(veeraWorldY) - icamY;
  const uint16_t *frameData = veera_idle_pixels;

  if (isAttackingState) {
    spriteW = VEERA_ATTACK_SPRITE_W;
    spriteH = VEERA_ATTACK_SPRITE_H;
    if (currentDirection == LostCrownGameplay::VEERA_DIR_RIGHT) {
      drawScreenX -= VEERA_ATTACK_OFFSET_X_RIGHT;
    } else {
      drawScreenX -= VEERA_ATTACK_OFFSET_X_LEFT;
    }
    drawScreenY -= VEERA_ATTACK_OFFSET_Y;
    frameData = veera_attack_frames[currentAttackFrame];
  } else if (isThrowingState) {
    uint8_t throwIdx = 0;
    if (currentThrowState == LostCrownGameplay::THROW_FRAME1) throwIdx = 0;
    else if (currentThrowState == LostCrownGameplay::THROW_FRAME2) throwIdx = 1;
    else if (currentThrowState == LostCrownGameplay::THROW_FRAME3) throwIdx = 2;
    else if (currentThrowState == LostCrownGameplay::THROW_FRAME4) throwIdx = 3;
    frameData = veera_throw_frames[throwIdx];
  } else if (currentState == LostCrownGameplay::VEERA_STATE_JUMPING) {
    frameData = veera_jump_frames[currentJumpFrame];
  } else if (currentState == LostCrownGameplay::VEERA_STATE_RUNNING) {
    frameData = veera_run_frames[currentRunFrame];
  }

  LostCrownGameplay::VeeraDirection facing = isThrowingState ? throwFacingDir : currentDirection;

  for (int16_t r = 0; r < spriteH; r++) {
    int16_t sy = drawScreenY + r;
    if (sy < 0 || sy >= SCREEN_H) continue;

    for (int16_t c = 0; c < spriteW; c++) {
      int16_t sx = drawScreenX + c;
      if (sx < 0 || sx >= SCREEN_W) continue;

      int16_t sc = (facing == LostCrownGameplay::VEERA_DIR_LEFT) ? (spriteW - 1 - c) : c;
      uint16_t pixel = pgm_read_word(&frameData[r * spriteW + sc]);

      if (pixel != VEERA_TRANSPARENT_COLOR) {
        screenBuffer[sy * SCREEN_W + sx] = pixel;
      }
    }
  }

  // 3. Composite Boomerang sprite into backbuffer if active
  if (boomerangState != LostCrownGameplay::BOOMERANG_INACTIVE) {
    int16_t bScreenX = (int16_t)roundf(boomerangWorldX) - icamX;
    int16_t bScreenY = (int16_t)roundf(boomerangWorldY) - icamY;
    const uint16_t *rotFrameData = boomerang_rot_frames[currentBoomerangRotFrame];

    for (int16_t r = 0; r < BOOMERANG_SPRITE_SIZE; r++) {
      int16_t sy = bScreenY + r;
      if (sy < 0 || sy >= SCREEN_H) continue;

      for (int16_t c = 0; c < BOOMERANG_SPRITE_SIZE; c++) {
        int16_t sx = bScreenX + c;
        if (sx < 0 || sx >= SCREEN_W) continue;

        uint16_t bPixel = pgm_read_word(&rotFrameData[r * BOOMERANG_SPRITE_SIZE + c]);
        if (bPixel != VEERA_TRANSPARENT_COLOR) {
          screenBuffer[sy * SCREEN_W + sx] = bPixel;
        }
      }
    }
  }

  // 4. Atomic hardware blit to ST7735 TFT display (zero tearing, zero flicker)
  tft.drawRGBBitmap(0, 0, screenBuffer, SCREEN_W, SCREEN_H);
}

// ─── Public API ──────────────────────────────────────────────────────────────

void begin(Adafruit_ST7735 &tft) {
  reset();

  uint32_t now = millis();
  lastAnimTick         = now;
  lastPhysicsTick      = now;
  lastAttackTick       = now;
  lastThrowTick        = now;
  lastBoomerangRotTick = now;
  lastBackDownState    = isBackDown();

  renderFrame(tft);
}

void reset() {
  veeraWorldX      = -20.0f; // Enters from the left edge
  veeraWorldY      = LEVEL1_DEFAULT_GROUND_Y - (float)VEERA_SPRITE_H; // 180.0f
  velocityX        = 0.0f;
  velocityY        = 0.0f;
  isGroundedState  = true;
  currentState     = LostCrownGameplay::VEERA_STATE_RUNNING;
  currentDirection = LostCrownGameplay::VEERA_DIR_RIGHT;
  currentRunFrame  = 0;
  currentJumpFrame = 0;

  isAttackingState   = false;
  currentAttackFrame = 0;

  isThrowingState          = false;
  currentThrowState        = LostCrownGameplay::THROW_NONE;
  throwFacingDir           = LostCrownGameplay::VEERA_DIR_RIGHT;
  boomerangState           = LostCrownGameplay::BOOMERANG_INACTIVE;
  boomerangWorldX          = 0.0f;
  boomerangWorldY          = 0.0f;
  boomerangTargetX         = 0.0f;
  currentBoomerangRotFrame = 0;

  camX = 0.0f;
  camY = 80.0f;

  lastBackDownState  = false;
  backPressStartTick = 0;
  longPressTriggered = false;
  shouldExitLevel    = false;
}

void update(Adafruit_ST7735 &tft) {
  uint32_t now = millis();
  uint32_t dt = now - lastPhysicsTick;
  if (dt > 100) dt = 100; // Cap large frame jumps
  lastPhysicsTick = now;

  // 1. Read joystick & buttons
  int vrx = analogRead(JOY_X);
  int vry = analogRead(JOY_Y);
  bool enterHit = isEnterPressed();
  bool backDown = isBackDown();
  (void)isBackPressed(); // Clear raw button queue

  // 2. Process BACK button (Short press -> Boomerang, Hold 3s -> Quit to Title)
  if (backDown && !lastBackDownState) {
    backPressStartTick = now;
    longPressTriggered = false;
  } else if (backDown && lastBackDownState) {
    if (!longPressTriggered && (now - backPressStartTick >= BACK_LONG_PRESS_TIME)) {
      longPressTriggered = true;
      shouldExitLevel = true;
    }
  } else if (!backDown && lastBackDownState) {
    if (!longPressTriggered) {
      if (!isThrowingState && !isAttackingState) {
        isThrowingState = true;
        currentThrowState = LostCrownGameplay::THROW_FRAME1;
        throwFacingDir = currentDirection;
        lastThrowTick = now;
      }
    }
  }
  lastBackDownState = backDown;

  // 3. Process ENTER button (Sword Attack)
  if (enterHit && !isAttackingState && !isThrowingState) {
    isAttackingState = true;
    currentAttackFrame = 0;
    lastAttackTick = now;
  }

  // 4. Attack progression
  if (isAttackingState) {
    if (now - lastAttackTick >= LC_ATTACK_FRAME_TIME_MS) {
      lastAttackTick = now;
      if (currentAttackFrame < VEERA_ATTACK_FRAME_COUNT - 1) {
        currentAttackFrame++;
      } else {
        isAttackingState = false;
        currentAttackFrame = 0;
      }
    }
  }

  // 5. Boomerang throw progression
  if (isThrowingState) {
    switch (currentThrowState) {
      case LostCrownGameplay::THROW_FRAME1:
        if (now - lastThrowTick >= LC_THROW_FRAME1_TIME_MS) {
          currentThrowState = LostCrownGameplay::THROW_FRAME2;
          lastThrowTick = now;
        }
        break;

      case LostCrownGameplay::THROW_FRAME2:
        if (now - lastThrowTick >= LC_THROW_FRAME2_TIME_MS) {
          currentThrowState = LostCrownGameplay::THROW_FRAME3;
          lastThrowTick = now;
          boomerangState = LostCrownGameplay::BOOMERANG_OUTBOUND;

          int16_t offX = (throwFacingDir == LostCrownGameplay::VEERA_DIR_RIGHT) ? LC_BOOMERANG_OFFSET_X_RIGHT : LC_BOOMERANG_OFFSET_X_LEFT;
          boomerangWorldX = veeraWorldX + (float)offX;
          boomerangWorldY = veeraWorldY + (float)LC_BOOMERANG_OFFSET_Y;

          // Target X: 75% of available horizontal screen distance
          float maxTravel = (float)SCREEN_W * 0.75f;
          if (throwFacingDir == LostCrownGameplay::VEERA_DIR_RIGHT) {
            boomerangTargetX = boomerangWorldX + maxTravel;
            if (boomerangTargetX > LEVEL1_MAX_PLAYER_X + 30.0f) boomerangTargetX = LEVEL1_MAX_PLAYER_X + 30.0f;
          } else {
            boomerangTargetX = boomerangWorldX - maxTravel;
            if (boomerangTargetX < 0.0f) boomerangTargetX = 0.0f;
          }

          currentBoomerangRotFrame = 0;
          lastBoomerangRotTick = now;
        }
        break;

      case LostCrownGameplay::THROW_FRAME3:
        // Holds pose until caught
        break;

      case LostCrownGameplay::THROW_FRAME4:
        if (now - lastThrowTick >= LC_THROW_FRAME4_TIME_MS) {
          currentThrowState = LostCrownGameplay::THROW_NONE;
          isThrowingState = false;
        }
        break;

      default:
        break;
    }
  }

  // 6. Boomerang airborne flight
  if (boomerangState != LostCrownGameplay::BOOMERANG_INACTIVE) {
    if (now - lastBoomerangRotTick >= LC_BOOMERANG_ROT_INTERVAL_MS) {
      lastBoomerangRotTick = now;
      currentBoomerangRotFrame = (currentBoomerangRotFrame + 1) % BOOMERANG_ROT_FRAME_COUNT;
    }

    float distStep = LC_BOOMERANG_SPEED * (float)dt;

    if (boomerangState == LostCrownGameplay::BOOMERANG_OUTBOUND) {
      if (throwFacingDir == LostCrownGameplay::VEERA_DIR_RIGHT) {
        boomerangWorldX += distStep;
        if (boomerangWorldX >= boomerangTargetX) {
          boomerangWorldX = boomerangTargetX;
          boomerangState = LostCrownGameplay::BOOMERANG_RETURNING;
        }
      } else {
        boomerangWorldX -= distStep;
        if (boomerangWorldX <= boomerangTargetX) {
          boomerangWorldX = boomerangTargetX;
          boomerangState = LostCrownGameplay::BOOMERANG_RETURNING;
        }
      }
    } else if (boomerangState == LostCrownGameplay::BOOMERANG_RETURNING) {
      int16_t offX = (throwFacingDir == LostCrownGameplay::VEERA_DIR_RIGHT) ? LC_BOOMERANG_OFFSET_X_RIGHT : LC_BOOMERANG_OFFSET_X_LEFT;
      float catchX = veeraWorldX + (float)offX;
      float catchY = veeraWorldY + (float)LC_BOOMERANG_OFFSET_Y;

      float dx = catchX - boomerangWorldX;
      float dy = catchY - boomerangWorldY;

      if (fabsf(dx) <= distStep && fabsf(dy) <= 8.0f) {
        boomerangState = LostCrownGameplay::BOOMERANG_INACTIVE;
        currentThrowState = LostCrownGameplay::THROW_FRAME4;
        lastThrowTick = now;
      } else {
        if (dx > 0) boomerangWorldX += distStep;
        else boomerangWorldX -= distStep;

        if (fabsf(dy) > 1.0f) {
          float yStep = fminf(fabsf(dy), distStep * 0.75f);
          boomerangWorldY += (dy > 0 ? 1.0f : -1.0f) * yStep;
        }
      }
    }
  }

  // 7. Jump Trigger (Grounded only)
  bool isUpPushed = (vry < LC_JOYSTICK_UP_THRESHOLD);
  if (isGroundedState && isUpPushed) {
    isGroundedState = false;
    currentState = LostCrownGameplay::VEERA_STATE_JUMPING;
    velocityY = -LC_JUMP_FORCE;
    currentJumpFrame = 0;
    lastAnimTick = now;

    if (vrx > LC_JOYSTICK_DEADZONE_HIGH) {
      velocityX = LC_JUMP_HORIZONTAL_SPEED;
      currentDirection = LostCrownGameplay::VEERA_DIR_RIGHT;
    } else if (vrx < LC_JOYSTICK_DEADZONE_LOW) {
      velocityX = -LC_JUMP_HORIZONTAL_SPEED;
      currentDirection = LostCrownGameplay::VEERA_DIR_LEFT;
    } else {
      velocityX = 0.0f;
    }
  }

  // 8. Player Movement & Terrain Collision
  float footCenterX = veeraWorldX + (float)VEERA_SPRITE_W * 0.5f; // Center foot
  float feetY = veeraWorldY + (float)VEERA_SPRITE_H;

  if (!isGroundedState) {
    // ─── AIRBORNE ────────────────────────────────────────────────────────────
    currentState = LostCrownGameplay::VEERA_STATE_JUMPING;

    veeraWorldX += velocityX * (float)dt;
    if (veeraWorldX < LEVEL1_MIN_PLAYER_X) veeraWorldX = LEVEL1_MIN_PLAYER_X;
    if (veeraWorldX > LEVEL1_MAX_PLAYER_X) veeraWorldX = LEVEL1_MAX_PLAYER_X;

    velocityY += LC_GRAVITY * (float)dt;
    veeraWorldY += velocityY * (float)dt;
    feetY = veeraWorldY + (float)VEERA_SPRITE_H;
    footCenterX = veeraWorldX + (float)VEERA_SPRITE_W * 0.5f;

    // Jump frame mapping
    if (velocityY < -0.05f) {
      currentJumpFrame = 0; // Takeoff / rising
    } else if (velocityY < 0.05f) {
      currentJumpFrame = 1; // Apex
    } else {
      currentJumpFrame = 2; // Falling
    }

    // Landing check: only land when falling downward
    if (velocityY >= 0.0f) {
      float floorY = getFloorHeight(footCenterX, feetY);
      if (feetY >= floorY) {
        // Land on platform / stairs / ground!
        veeraWorldY = floorY - (float)VEERA_SPRITE_H;
        velocityY = 0.0f;
        velocityX = 0.0f;
        isGroundedState = true;

        if (vrx > LC_JOYSTICK_DEADZONE_HIGH) {
          currentState = LostCrownGameplay::VEERA_STATE_RUNNING;
          currentDirection = LostCrownGameplay::VEERA_DIR_RIGHT;
          currentRunFrame = 0;
        } else if (vrx < LC_JOYSTICK_DEADZONE_LOW) {
          currentState = LostCrownGameplay::VEERA_STATE_RUNNING;
          currentDirection = LostCrownGameplay::VEERA_DIR_LEFT;
          currentRunFrame = 0;
        } else {
          currentState = LostCrownGameplay::VEERA_STATE_IDLE;
          currentRunFrame = 0;
        }
      }
    }
  } else {
    // ─── GROUNDED ────────────────────────────────────────────────────────────
    if (vrx > LC_JOYSTICK_DEADZONE_HIGH) {
      currentState = LostCrownGameplay::VEERA_STATE_RUNNING;
      currentDirection = LostCrownGameplay::VEERA_DIR_RIGHT;

      float dx = LC_VEERA_MOVE_SPEED * (float)dt;
      veeraWorldX += dx;
      if (veeraWorldX > LEVEL1_MAX_PLAYER_X) veeraWorldX = LEVEL1_MAX_PLAYER_X;
    } else if (vrx < LC_JOYSTICK_DEADZONE_LOW) {
      currentState = LostCrownGameplay::VEERA_STATE_RUNNING;
      currentDirection = LostCrownGameplay::VEERA_DIR_LEFT;

      float dx = LC_VEERA_MOVE_SPEED * (float)dt;
      veeraWorldX -= dx;
      if (veeraWorldX < LEVEL1_MIN_PLAYER_X) veeraWorldX = LEVEL1_MIN_PLAYER_X;
    } else {
      currentState = LostCrownGameplay::VEERA_STATE_IDLE;
    }

    // Advance running animation
    if (currentState == LostCrownGameplay::VEERA_STATE_RUNNING) {
      if (now - lastAnimTick >= LC_RUN_FRAME_TIME_MS) {
        lastAnimTick = now;
        currentRunFrame = (currentRunFrame + 1) % VEERA_RUN_FRAME_COUNT;
      }
    }

    // Re-sample floor at new position (follows stairs and platforms)
    footCenterX = veeraWorldX + (float)VEERA_SPRITE_W * 0.5f;
    feetY = veeraWorldY + (float)VEERA_SPRITE_H;
    float floorY = getFloorHeight(footCenterX, feetY);

    if (floorY > feetY + 10.0f) {
      // Walked off an edge!
      isGroundedState = false;
      velocityY = 0.0f;
      currentState = LostCrownGameplay::VEERA_STATE_JUMPING;
      currentJumpFrame = 2; // Falling
    } else {
      // Smoothly stick feet to walkable platform or stair slope
      veeraWorldY = floorY - (float)VEERA_SPRITE_H;
    }
  }

  // 9. Camera Follow System with Dead-Zone (Classic 2D side-scrolling platformer feel)
  float screenX = veeraWorldX - camX;
  float screenY = veeraWorldY - camY;

  // Horizontal follow: Veera moves freely in central 55-65% follow zone
  float targetCamX = camX;
  if (screenX > DEADZONE_RIGHT) {
    targetCamX = veeraWorldX - DEADZONE_RIGHT;
  } else if (screenX < DEADZONE_LEFT) {
    targetCamX = veeraWorldX - DEADZONE_LEFT;
  }

  // Vertical follow: soft follow for multi-level exploration
  float targetCamY = camY;
  if (screenY < DEADZONE_TOP) {
    targetCamY = veeraWorldY - DEADZONE_TOP;
  } else if (screenY > DEADZONE_BOTTOM) {
    targetCamY = veeraWorldY - DEADZONE_BOTTOM;
  }

  // Clamp target camera within valid level boundaries
  float maxCamX = (float)(LEVEL1_WORLD_W - SCREEN_W); // 384.0f
  float maxCamY = (float)(LEVEL1_WORLD_H - SCREEN_H); // 80.0f
  if (targetCamX < 0.0f) targetCamX = 0.0f;
  if (targetCamX > maxCamX) targetCamX = maxCamX;
  if (targetCamY < 0.0f) targetCamY = 0.0f;
  if (targetCamY > maxCamY) targetCamY = maxCamY;

  // Smooth camera interpolation
  camX += (targetCamX - camX) * 0.20f;
  camY += (targetCamY - camY) * 0.15f;

  if (fabsf(targetCamX - camX) < 0.2f) camX = targetCamX;
  if (fabsf(targetCamY - camY) < 0.2f) camY = targetCamY;

  // 10. Render frame
  renderFrame(tft);
}

bool shouldExit() {
  return shouldExitLevel;
}

float getPlayerWorldX() {
  return veeraWorldX;
}

float getPlayerWorldY() {
  return veeraWorldY;
}

float getCameraX() {
  return camX;
}

float getCameraY() {
  return camY;
}

bool isGrounded() {
  return isGroundedState;
}

} // namespace LostCrownLevel1
