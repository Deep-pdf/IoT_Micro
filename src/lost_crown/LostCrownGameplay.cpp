/**
 * LostCrownGameplay.cpp
 *
 * Implementation of the Lost Crown Training Ground gameplay scene.
 *
 * Features:
 *  - 128x160 Static Training Ground background drawn once on begin().
 *  - Veera Idle (Veera_IDLE) and 5-frame running cycle (run1 -> run2 -> ... -> run5 -> repeat).
 *  - Veera 3-frame jumping animation (jump1 -> jump2 -> jump3), size-matched to Idle.
 *  - Full 2D platformer jump physics:
 *      - Vertical jump on UP
 *      - Diagonal jump on UP + LEFT / UP + RIGHT with horizontal momentum
 *      - Gravity and vertical velocity integration with smooth parabolic arc
 *      - Automatic landing and transition to IDLE or RUNNING
 *      - No double jump while airborne
 *  - Real-time horizontal mirroring when facing LEFT (runs and jumps).
 *  - Guaranteed foot anchoring to the grass ground line across all animations.
 *  - 100% flicker-free differential background restoration and transparent sprite composition.
 */

#include "LostCrownGameplay.h"
#include "LostCrownGameAssets.h"
#include "LostCrownConfig.h"
#include "button.h"
#include <Adafruit_GFX.h>

#define JOY_X 34
#define JOY_Y 35

namespace LostCrownGameplay {

// ─── Player & Animation State ────────────────────────────────────────────────
static float          veeraX               = LC_VEERA_SPAWN_X;
static float          veeraY               = LC_VEERA_SPAWN_Y;

static float          velocityX            = 0.0f;
static float          velocityY            = 0.0f;
static bool           isGroundedState      = true;

static int16_t        prevBoxX             = -999;
static int16_t        prevBoxY             = -999;
static int16_t        prevBoxW             = 0;
static int16_t        prevBoxH             = 0;

static VeeraState     currentState         = VEERA_STATE_IDLE;
static VeeraDirection currentDirection     = VEERA_DIR_RIGHT;
static uint8_t        currentRunFrame      = 0;
static uint8_t        currentJumpFrame     = 0;

// Attack Action State
static bool           isAttackingState     = false;
static uint8_t        currentAttackFrame   = 0;
static uint32_t       lastAttackTick       = 0;

// Boomerang Throw Action State
static bool           isThrowingState          = false;
static VeeraThrowState currentThrowState       = THROW_NONE;
static VeeraDirection throwFacingDir          = VEERA_DIR_RIGHT;
static uint32_t       lastThrowTick            = 0;

// Boomerang Flight & Rotation State
static BoomerangState boomerangState           = BOOMERANG_INACTIVE;
static float          boomerangX               = 0.0f;
static float          boomerangY               = 0.0f;
static float          boomerangTargetX         = 0.0f;
static uint8_t        currentBoomerangRotFrame = 0;
static uint32_t       lastBoomerangRotTick     = 0;

static int16_t        prevBoomerangX           = -999;
static int16_t        prevBoomerangY           = -999;
static bool           prevBoomerangActive      = false;

// Back Button Long Press & Exit Tracking
static bool           lastBackDownState        = false;
static uint32_t       backPressStartTick       = 0;
static bool           longPressTriggered       = false;
static bool           shouldExitGameplay       = false;

static uint32_t       lastAnimTick         = 0;
static uint32_t       lastPhysicsTick      = 0;

// RAM buffer for zero-flicker composite rendering (max dimensions: 58 x 46 = 2,668 words = 5,336 bytes)
static uint16_t       spriteCompositeBuffer[VEERA_ATTACK_SPRITE_W * VEERA_ATTACK_SPRITE_H];
static uint16_t       stripBuffer[VEERA_ATTACK_SPRITE_W * VEERA_ATTACK_SPRITE_H];
static uint16_t       boomerangCompositeBuffer[BOOMERANG_SPRITE_SIZE * BOOMERANG_SPRITE_SIZE];

// ─── Background & Sprite Rendering Helpers ───────────────────────────────────

static void restoreBackgroundRect(Adafruit_ST7735 &tft, int16_t rx, int16_t ry, int16_t rw, int16_t rh) {
  if (rw <= 0 || rh <= 0) return;
  if (rx >= LC_BG_W || ry >= LC_BG_H) return;

  int16_t x0 = (rx < 0) ? 0 : rx;
  int16_t y0 = (ry < 0) ? 0 : ry;
  int16_t x1 = rx + rw;
  int16_t y1 = ry + rh;
  if (x1 > LC_BG_W) x1 = LC_BG_W;
  if (y1 > LC_BG_H) y1 = LC_BG_H;

  int16_t w = x1 - x0;
  int16_t h = y1 - y0;
  if (w <= 0 || h <= 0) return;

  for (int16_t r = 0; r < h; r++) {
    int16_t py = y0 + r;
    for (int16_t c = 0; c < w; c++) {
      int16_t px = x0 + c;
      stripBuffer[r * w + c] = pgm_read_word(&image_Traning_ground_pixels[py * LC_BG_W + px]);
    }
  }

  tft.drawRGBBitmap(x0, y0, stripBuffer, w, h);
}

static void drawBoomerang(Adafruit_ST7735 &tft) {
  int16_t bx = (int16_t)roundf(boomerangX);
  int16_t by = (int16_t)roundf(boomerangY);

  if (bx < 0) bx = 0;
  if (by < 0) by = 0;
  if (bx > LC_BG_W - BOOMERANG_SPRITE_SIZE) bx = LC_BG_W - BOOMERANG_SPRITE_SIZE;
  if (by > LC_BG_H - BOOMERANG_SPRITE_SIZE) by = LC_BG_H - BOOMERANG_SPRITE_SIZE;

  const uint16_t *rotFrameData = boomerang_rot_frames[currentBoomerangRotFrame];

  for (int16_t r = 0; r < BOOMERANG_SPRITE_SIZE; r++) {
    int16_t py = by + r;
    for (int16_t c = 0; c < BOOMERANG_SPRITE_SIZE; c++) {
      int16_t px = bx + c;
      uint16_t bgPixel = 0x0000;

      // Sample background pixel, checking if overlapping Veera's active rendered sprite box
      if (prevBoxX != -999 && px >= prevBoxX && px < prevBoxX + prevBoxW && py >= prevBoxY && py < prevBoxY + prevBoxH) {
        bgPixel = spriteCompositeBuffer[(py - prevBoxY) * prevBoxW + (px - prevBoxX)];
      } else if (px >= 0 && px < LC_BG_W && py >= 0 && py < LC_BG_H) {
        bgPixel = pgm_read_word(&image_Traning_ground_pixels[py * LC_BG_W + px]);
      }

      uint16_t bPixel = pgm_read_word(&rotFrameData[r * BOOMERANG_SPRITE_SIZE + c]);
      if (bPixel != VEERA_TRANSPARENT_COLOR) {
        boomerangCompositeBuffer[r * BOOMERANG_SPRITE_SIZE + c] = bPixel;
      } else {
        boomerangCompositeBuffer[r * BOOMERANG_SPRITE_SIZE + c] = bgPixel;
      }
    }
  }

  tft.drawRGBBitmap(bx, by, boomerangCompositeBuffer, BOOMERANG_SPRITE_SIZE, BOOMERANG_SPRITE_SIZE);
  prevBoomerangX = bx;
  prevBoomerangY = by;
  prevBoomerangActive = true;
}

static void drawVeera(Adafruit_ST7735 &tft) {
  int16_t currX = (int16_t)roundf(veeraX);
  int16_t currY = (int16_t)roundf(veeraY);

  int16_t spriteW;
  int16_t spriteH;
  int16_t drawX;
  int16_t drawY;
  const uint16_t *frameData;

  // Visual Priority: Attack -> Throw -> Jump -> Run -> Idle
  if (isAttackingState) {
    spriteW = VEERA_ATTACK_SPRITE_W;
    spriteH = VEERA_ATTACK_SPRITE_H;
    if (currentDirection == VEERA_DIR_RIGHT) {
      drawX = currX - VEERA_ATTACK_OFFSET_X_RIGHT;
    } else {
      drawX = currX - VEERA_ATTACK_OFFSET_X_LEFT;
    }
    drawY = currY - VEERA_ATTACK_OFFSET_Y;
    frameData = veera_attack_frames[currentAttackFrame];
  } else if (isThrowingState) {
    spriteW = VEERA_SPRITE_W;
    spriteH = VEERA_SPRITE_H;
    drawX = currX;
    drawY = currY;
    uint8_t throwIdx = 0;
    if (currentThrowState == THROW_FRAME1) throwIdx = 0;
    else if (currentThrowState == THROW_FRAME2) throwIdx = 1;
    else if (currentThrowState == THROW_FRAME3) throwIdx = 2;
    else if (currentThrowState == THROW_FRAME4) throwIdx = 3;
    frameData = veera_throw_frames[throwIdx];
  } else if (currentState == VEERA_STATE_JUMPING) {
    spriteW = VEERA_SPRITE_W;
    spriteH = VEERA_SPRITE_H;
    drawX = currX;
    drawY = currY;
    frameData = veera_jump_frames[currentJumpFrame];
  } else if (currentState == VEERA_STATE_RUNNING) {
    spriteW = VEERA_SPRITE_W;
    spriteH = VEERA_SPRITE_H;
    drawX = currX;
    drawY = currY;
    frameData = veera_run_frames[currentRunFrame];
  } else {
    spriteW = VEERA_SPRITE_W;
    spriteH = VEERA_SPRITE_H;
    drawX = currX;
    drawY = currY;
    frameData = veera_idle_pixels;
  }

  // 1. Clean ONLY the vacated slivers of the previous frame (zero-flicker differential restoration)
  if (prevBoxX != -999) {
    if (spriteW != prevBoxW || spriteH != prevBoxH || abs(drawX - prevBoxX) >= prevBoxW || abs(drawY - prevBoxY) >= prevBoxH) {
      // Dimension shift (transition between attack & movement) or large displacement: restore entire previous box
      restoreBackgroundRect(tft, prevBoxX, prevBoxY, prevBoxW, prevBoxH);
    } else if (drawX != prevBoxX || drawY != prevBoxY) {
      int16_t dx = drawX - prevBoxX;
      int16_t dy = drawY - prevBoxY;

      // Vertical vacated strip
      if (dy > 0) {
        // Moved down: top of prevBox was vacated
        restoreBackgroundRect(tft, prevBoxX, prevBoxY, prevBoxW, dy);
      } else if (dy < 0) {
        // Moved up: bottom of prevBox was vacated
        restoreBackgroundRect(tft, prevBoxX, drawY + spriteH, prevBoxW, -dy);
      }

      // Horizontal vacated strip for the remaining Y overlap
      int16_t remY = (dy > 0) ? drawY : prevBoxY;
      int16_t remH = prevBoxH - abs(dy);

      if (dx > 0) {
        // Moved right: left of prevBox was vacated
        restoreBackgroundRect(tft, prevBoxX, remY, dx, remH);
      } else if (dx < 0) {
        // Moved left: right of prevBox was vacated
        restoreBackgroundRect(tft, drawX + spriteW, remY, -dx, remH);
      }
    }
  }

  // 2. Composite sprite over background in local RAM buffer
  for (int16_t r = 0; r < spriteH; r++) {
    int16_t py = drawY + r;
    for (int16_t c = 0; c < spriteW; c++) {
      int16_t px = drawX + c;

      // Sample background pixel
      uint16_t bgPixel = 0x0000;
      if (px >= 0 && px < LC_BG_W && py >= 0 && py < LC_BG_H) {
        bgPixel = pgm_read_word(&image_Traning_ground_pixels[py * LC_BG_W + px]);
      }

      // Sample sprite pixel with horizontal flip if facing left
      VeeraDirection facing = isThrowingState ? throwFacingDir : currentDirection;
      int16_t sc = (facing == VEERA_DIR_LEFT) ? (spriteW - 1 - c) : c;
      uint16_t spritePixel = pgm_read_word(&frameData[r * spriteW + sc]);

      // Transparent keying
      if (spritePixel != VEERA_TRANSPARENT_COLOR) {
        spriteCompositeBuffer[r * spriteW + c] = spritePixel;
      } else {
        spriteCompositeBuffer[r * spriteW + c] = bgPixel;
      }
    }
  }

  // 3. Clip-safe blit to hardware TFT (atomic character update, no flashing, no screen-edge wrap)
  int16_t x0 = (drawX < 0) ? 0 : drawX;
  int16_t y0 = (drawY < 0) ? 0 : drawY;
  int16_t x1 = drawX + spriteW;
  int16_t y1 = drawY + spriteH;
  if (x1 > LC_BG_W) x1 = LC_BG_W;
  if (y1 > LC_BG_H) y1 = LC_BG_H;

  int16_t blitW = x1 - x0;
  int16_t blitH = y1 - y0;

  if (blitW > 0 && blitH > 0) {
    if (blitW == spriteW && blitH == spriteH) {
      tft.drawRGBBitmap(drawX, drawY, spriteCompositeBuffer, spriteW, spriteH);
    } else {
      // Partial sub-rectangle when sprite crosses display boundary
      for (int16_t r = 0; r < blitH; r++) {
        int16_t srcR = (y0 - drawY) + r;
        int16_t srcC = (x0 - drawX);
        memcpy(&stripBuffer[r * blitW], &spriteCompositeBuffer[srcR * spriteW + srcC], blitW * sizeof(uint16_t));
      }
      tft.drawRGBBitmap(x0, y0, stripBuffer, blitW, blitH);
    }
  }

  // 4. Update previous render tracking
  prevBoxX = drawX;
  prevBoxY = drawY;
  prevBoxW = spriteW;
  prevBoxH = spriteH;
}

// ─── Public API ──────────────────────────────────────────────────────────────

void begin(Adafruit_ST7735 &tft) {
  // 1. Draw static Training Ground background ONCE
  tft.drawRGBBitmap(0, 0, image_Traning_ground_pixels, LC_BG_W, LC_BG_H);

  // 2. Set Veera's starting position and state
  reset();

  uint32_t now        = millis();
  lastAnimTick        = now;
  lastPhysicsTick     = now;
  lastAttackTick      = now;
  lastThrowTick       = now;
  lastBoomerangRotTick = now;
  lastBackDownState   = isBackDown();

  // 3. Render initial Veera IDLE sprite
  drawVeera(tft);
}

void update(Adafruit_ST7735 &tft) {
  uint32_t now = millis();
  uint32_t dt = now - lastPhysicsTick;
  if (dt > 100) dt = 100; // Cap large frame jumps
  lastPhysicsTick = now;

  // 1. Read joystick & button inputs
  int vrx = analogRead(JOY_X);
  int vry = analogRead(JOY_Y);
  bool enterHit = isEnterPressed();
  bool backDown = isBackDown();
  (void)isBackPressed(); // Clear raw button event if queued

  bool needsRedraw = false;
  VeeraState oldState = currentState;
  VeeraDirection oldDir = currentDirection;
  uint8_t oldRunFrame = currentRunFrame;
  uint8_t oldJumpFrame = currentJumpFrame;
  bool oldAttacking = isAttackingState;
  uint8_t oldAttackFrame = currentAttackFrame;
  bool oldThrowing = isThrowingState;
  VeeraThrowState oldThrowState = currentThrowState;
  uint8_t oldBoomerangRotFrame = currentBoomerangRotFrame;
  int16_t oldPixelX = (int16_t)roundf(veeraX);
  int16_t oldPixelY = (int16_t)roundf(veeraY);

  // 2. Process BACK Button (Short press -> Boomerang Throw, Hold 3s -> Quit)
  if (backDown && !lastBackDownState) {
    // Button pressed down: record start time
    backPressStartTick = now;
    longPressTriggered = false;
  } else if (backDown && lastBackDownState) {
    // Button continuously held: check 3000 ms long-press threshold
    if (!longPressTriggered && (now - backPressStartTick >= BACK_LONG_PRESS_TIME)) {
      longPressTriggered = true;
      shouldExitGameplay = true;
    }
  } else if (!backDown && lastBackDownState) {
    // Button released before 3s: valid SHORT PRESS triggers Boomerang Throw
    if (!longPressTriggered) {
      if (!isThrowingState && !isAttackingState) {
        isThrowingState = true;
        currentThrowState = THROW_FRAME1;
        throwFacingDir = currentDirection;
        lastThrowTick = now;
        needsRedraw = true;
      }
    }
  }
  lastBackDownState = backDown;

  // 3. Process Attack Trigger (ENTER button edge detection)
  if (enterHit && !isAttackingState && !isThrowingState) {
    isAttackingState = true;
    currentAttackFrame = 0;
    lastAttackTick = now;
    needsRedraw = true;
  }

  // 4. Process Attack Animation Progression (Non-blocking timing)
  if (isAttackingState) {
    if (now - lastAttackTick >= LC_ATTACK_FRAME_TIME_MS) {
      lastAttackTick = now;
      if (currentAttackFrame < VEERA_ATTACK_FRAME_COUNT - 1) {
        currentAttackFrame++;
        needsRedraw = true;
      } else {
        isAttackingState = false;
        currentAttackFrame = 0;
        needsRedraw = true;
      }
    }
  }

  // 5. Process Boomerang Throw Animation Progression (Non-blocking timing)
  if (isThrowingState) {
    switch (currentThrowState) {
      case THROW_FRAME1:
        if (now - lastThrowTick >= LC_THROW_FRAME1_TIME_MS) {
          currentThrowState = THROW_FRAME2;
          lastThrowTick = now;
          needsRedraw = true;
        }
        break;

      case THROW_FRAME2:
        if (now - lastThrowTick >= LC_THROW_FRAME2_TIME_MS) {
          // Release pose reached: LAUNCH BOOMERANG!
          currentThrowState = THROW_FRAME3;
          lastThrowTick = now;
          boomerangState = BOOMERANG_OUTBOUND;

          int16_t offX = (throwFacingDir == VEERA_DIR_RIGHT) ? LC_BOOMERANG_OFFSET_X_RIGHT : LC_BOOMERANG_OFFSET_X_LEFT;
          boomerangX = veeraX + (float)offX;
          boomerangY = veeraY + (float)LC_BOOMERANG_OFFSET_Y;

          // Target X: 60% of available horizontal travel distance toward facing direction boundary
          if (throwFacingDir == VEERA_DIR_RIGHT) {
            float maxBoundary = (float)(LC_BG_W - BOOMERANG_SPRITE_SIZE);
            float availableDist = maxBoundary - boomerangX;
            if (availableDist < 0.0f) availableDist = 0.0f;
            boomerangTargetX = boomerangX + availableDist * LC_BOOMERANG_MAX_DIST_PERCENT;
            if (boomerangTargetX > maxBoundary) boomerangTargetX = maxBoundary;
          } else {
            float minBoundary = 0.0f;
            float availableDist = boomerangX - minBoundary;
            if (availableDist < 0.0f) availableDist = 0.0f;
            boomerangTargetX = boomerangX - availableDist * LC_BOOMERANG_MAX_DIST_PERCENT;
            if (boomerangTargetX < minBoundary) boomerangTargetX = minBoundary;
          }

          currentBoomerangRotFrame = 0;
          lastBoomerangRotTick = now;
          needsRedraw = true;
        }
        break;

      case THROW_FRAME3:
        // Veera holds throw3 pose until boomerang returns to catch area
        break;

      case THROW_FRAME4:
        // Catch recovery frame
        if (now - lastThrowTick >= LC_THROW_FRAME4_TIME_MS) {
          currentThrowState = THROW_NONE;
          isThrowingState = false;
          needsRedraw = true;
        }
        break;

      default:
        break;
    }
  }

  // 6. Boomerang Flight Movement & Continuous Rotation
  if (boomerangState != BOOMERANG_INACTIVE) {
    needsRedraw = true;

    // Continuous rotation while airborne
    if (now - lastBoomerangRotTick >= LC_BOOMERANG_ROT_INTERVAL_MS) {
      lastBoomerangRotTick = now;
      currentBoomerangRotFrame = (currentBoomerangRotFrame + 1) % BOOMERANG_ROT_FRAME_COUNT;
    }

    float distStep = LC_BOOMERANG_SPEED * (float)dt;

    if (boomerangState == BOOMERANG_OUTBOUND) {
      if (throwFacingDir == VEERA_DIR_RIGHT) {
        boomerangX += distStep;
        if (boomerangX >= boomerangTargetX) {
          boomerangX = boomerangTargetX;
          boomerangState = BOOMERANG_RETURNING;
        }
      } else {
        boomerangX -= distStep;
        if (boomerangX <= boomerangTargetX) {
          boomerangX = boomerangTargetX;
          boomerangState = BOOMERANG_RETURNING;
        }
      }
    } else if (boomerangState == BOOMERANG_RETURNING) {
      // Dynamically return toward Veera's CURRENT catch position
      int16_t offX = (throwFacingDir == VEERA_DIR_RIGHT) ? LC_BOOMERANG_OFFSET_X_RIGHT : LC_BOOMERANG_OFFSET_X_LEFT;
      float catchX = veeraX + (float)offX;
      float catchY = veeraY + (float)LC_BOOMERANG_OFFSET_Y;

      float dx = catchX - boomerangX;
      float dy = catchY - boomerangY;

      if (fabsf(dx) <= distStep && fabsf(dy) <= 8.0f) {
        // CATCH REACHED! Boomerang caught by Veera
        boomerangState = BOOMERANG_INACTIVE;
        currentThrowState = THROW_FRAME4;
        lastThrowTick = now;
        needsRedraw = true;
      } else {
        if (dx > 0) boomerangX += distStep;
        else boomerangX -= distStep;

        // Dynamically align Y toward Veera's current height (if jumping / landing)
        if (fabsf(dy) > 1.0f) {
          float yStep = fminf(fabsf(dy), distStep * 0.75f);
          boomerangY += (dy > 0 ? 1.0f : -1.0f) * yStep;
        }
      }
    }
  }

  // 7. Process Jump Trigger (Only when grounded)
  bool isUpPushed = (vry < LC_JOYSTICK_UP_THRESHOLD);

  if (isGroundedState && isUpPushed) {
    // Jump initiates!
    isGroundedState = false;
    currentState = VEERA_STATE_JUMPING;
    velocityY = -LC_JUMP_FORCE;
    currentJumpFrame = 0;
    lastAnimTick = now;

    // Determine jump horizontal direction (vertical vs diagonal)
    if (vrx > LC_JOYSTICK_DEADZONE_HIGH) {
      velocityX = LC_JUMP_HORIZONTAL_SPEED;
      currentDirection = VEERA_DIR_RIGHT;
    } else if (vrx < LC_JOYSTICK_DEADZONE_LOW) {
      velocityX = -LC_JUMP_HORIZONTAL_SPEED;
      currentDirection = VEERA_DIR_LEFT;
    } else {
      velocityX = 0.0f;
    }
  }

  // 8. Update Movement Physics & States (Operates simultaneously with attack and throw)
  if (!isGroundedState) {
    // ─── AIRBORNE STATE ──────────────────────────────────────────────────────
    currentState = VEERA_STATE_JUMPING;

    // Apply horizontal velocity
    veeraX += velocityX * (float)dt;

    // Apply gravity to vertical velocity
    velocityY += LC_GRAVITY * (float)dt;
    veeraY += velocityY * (float)dt;

    // Jump animation frame mapping (takeoff -> apex/peak -> falling)
    if (velocityY < -0.05f) {
      currentJumpFrame = 0; // jump1: takeoff / rising
    } else if (velocityY < 0.05f) {
      currentJumpFrame = 1; // jump2: ascending peak / apex
    } else {
      currentJumpFrame = 2; // jump3: falling / preparing to land
    }

    // Clamp horizontal position within playable screen
    if (veeraX > (float)LC_VEERA_MAX_X) {
      veeraX = (float)LC_VEERA_MAX_X;
    }
    if (veeraX < (float)LC_VEERA_MIN_X) {
      veeraX = (float)LC_VEERA_MIN_X;
    }

    // Ground collision check
    if (veeraY >= (float)LC_VEERA_SPAWN_Y) {
      // LANDING ON GROUND!
      veeraY = (float)LC_VEERA_SPAWN_Y;
      velocityY = 0.0f;
      velocityX = 0.0f;
      isGroundedState = true;

      // Automatically transition to RUNNING or IDLE based on joystick
      if (vrx > LC_JOYSTICK_DEADZONE_HIGH) {
        currentState = VEERA_STATE_RUNNING;
        currentDirection = VEERA_DIR_RIGHT;
        currentRunFrame = 0;
      } else if (vrx < LC_JOYSTICK_DEADZONE_LOW) {
        currentState = VEERA_STATE_RUNNING;
        currentDirection = VEERA_DIR_LEFT;
        currentRunFrame = 0;
      } else {
        currentState = VEERA_STATE_IDLE;
        currentRunFrame = 0;
      }
    }
  } else {
    // ─── GROUNDED STATE ──────────────────────────────────────────────────────
    if (vrx > LC_JOYSTICK_DEADZONE_HIGH) {
      currentState = VEERA_STATE_RUNNING;
      currentDirection = VEERA_DIR_RIGHT;

      float dx = LC_VEERA_MOVE_SPEED * (float)dt;
      veeraX += dx;
      if (veeraX > (float)LC_VEERA_MAX_X) {
        veeraX = (float)LC_VEERA_MAX_X;
      }
    } else if (vrx < LC_JOYSTICK_DEADZONE_LOW) {
      currentState = VEERA_STATE_RUNNING;
      currentDirection = VEERA_DIR_LEFT;

      float dx = LC_VEERA_MOVE_SPEED * (float)dt;
      veeraX -= dx;
      if (veeraX < (float)LC_VEERA_MIN_X) {
        veeraX = (float)LC_VEERA_MIN_X;
      }
    } else {
      currentState = VEERA_STATE_IDLE;
    }

    // Running animation frame advancement
    if (currentState == VEERA_STATE_RUNNING) {
      if (now - lastAnimTick >= LC_RUN_FRAME_TIME_MS) {
        lastAnimTick = now;
        currentRunFrame = (currentRunFrame + 1) % VEERA_RUN_FRAME_COUNT;
      }
    }
  }

  // 9. State transition handling
  if (currentState != oldState) {
    needsRedraw = true;
    if (currentState == VEERA_STATE_IDLE) {
      currentRunFrame = 0;
    } else if (currentState == VEERA_STATE_RUNNING && oldState != VEERA_STATE_RUNNING) {
      currentRunFrame = 0;
      lastAnimTick = now;
    }
  }

  if (currentDirection != oldDir) {
    needsRedraw = true;
  }

  if (currentState == VEERA_STATE_RUNNING && currentRunFrame != oldRunFrame) {
    needsRedraw = true;
  }

  if (currentState == VEERA_STATE_JUMPING && currentJumpFrame != oldJumpFrame) {
    needsRedraw = true;
  }

  if (isAttackingState != oldAttacking) {
    needsRedraw = true;
  }

  if (isAttackingState && currentAttackFrame != oldAttackFrame) {
    needsRedraw = true;
  }

  if (isThrowingState != oldThrowing || currentThrowState != oldThrowState) {
    needsRedraw = true;
  }

  if (boomerangState != BOOMERANG_INACTIVE && currentBoomerangRotFrame != oldBoomerangRotFrame) {
    needsRedraw = true;
  }

  int16_t newPixelX = (int16_t)roundf(veeraX);
  int16_t newPixelY = (int16_t)roundf(veeraY);
  if (newPixelX != oldPixelX || newPixelY != oldPixelY) {
    needsRedraw = true;
  }

  // 10. Redraw when visual state, character position, or boomerang position changed
  if (needsRedraw || prevBoomerangActive || (boomerangState != BOOMERANG_INACTIVE)) {
    if (prevBoomerangActive && prevBoomerangX != -999) {
      restoreBackgroundRect(tft, prevBoomerangX, prevBoomerangY, BOOMERANG_SPRITE_SIZE, BOOMERANG_SPRITE_SIZE);
      prevBoomerangActive = false;
    }

    drawVeera(tft);

    if (boomerangState != BOOMERANG_INACTIVE) {
      drawBoomerang(tft);
    }
  }
}

void reset() {
  veeraX                   = LC_VEERA_SPAWN_X;
  veeraY                   = LC_VEERA_SPAWN_Y;
  velocityX                = 0.0f;
  velocityY                = 0.0f;
  isGroundedState          = true;
  currentState             = VEERA_STATE_IDLE;
  currentDirection         = VEERA_DIR_RIGHT;
  currentRunFrame          = 0;
  currentJumpFrame         = 0;
  isAttackingState         = false;
  currentAttackFrame       = 0;
  prevBoxX                 = -999;
  prevBoxY                 = -999;
  prevBoxW                 = 0;
  prevBoxH                 = 0;

  isThrowingState          = false;
  currentThrowState        = THROW_NONE;
  throwFacingDir           = VEERA_DIR_RIGHT;
  boomerangState           = BOOMERANG_INACTIVE;
  boomerangX               = 0.0f;
  boomerangY               = 0.0f;
  boomerangTargetX         = 0.0f;
  currentBoomerangRotFrame = 0;
  prevBoomerangX           = -999;
  prevBoomerangY           = -999;
  prevBoomerangActive      = false;

  lastBackDownState        = false;
  backPressStartTick       = 0;
  longPressTriggered       = false;
  shouldExitGameplay       = false;
}

VeeraState getState() {
  return currentState;
}

VeeraDirection getDirection() {
  return currentDirection;
}

bool isGrounded() {
  return isGroundedState;
}

bool isAttacking() {
  return isAttackingState;
}

uint8_t getAttackFrame() {
  return currentAttackFrame;
}

bool isAttackHitboxActive() {
  return (isAttackingState && currentAttackFrame == 2);
}

float getPositionX() {
  return veeraX;
}

float getPositionY() {
  return veeraY;
}

bool isThrowing() {
  return isThrowingState;
}

VeeraThrowState getThrowState() {
  return currentThrowState;
}

BoomerangState getBoomerangState() {
  return boomerangState;
}

bool isBoomerangActive() {
  return (boomerangState != BOOMERANG_INACTIVE);
}

float getBoomerangX() {
  return boomerangX;
}

float getBoomerangY() {
  return boomerangY;
}

bool shouldExit() {
  return shouldExitGameplay;
}

} // namespace LostCrownGameplay
