/**
 * LostCrownGameplay.cpp
 *
 * Implementation of the Lost Crown Training Ground gameplay scene.
 *
 * Features:
 *  - 128x160 Static Training Ground background drawn once on begin().
 *  - Veera Idle (Veera_IDLE) and 5-frame running cycle (run1 -> run2 -> ... -> run5 -> repeat).
 *  - Veera 3-frame jumping animation (jump1 -> jump2 -> jump3).
 *  - Full 2D platformer jump physics:
 *      - Vertical jump on UP
 *      - Diagonal jump on UP + LEFT / UP + RIGHT with horizontal momentum
 *      - Gravity and vertical velocity integration with smooth parabolic arc
 *      - Automatic landing and transition to IDLE or RUNNING
 *      - No double jump while airborne
 *  - Real-time horizontal mirroring when facing LEFT (runs and jumps).
 *  - Guaranteed foot anchoring to the grass ground line across all animations.
 *  - Zero-flicker dirty-rectangle background restoration and transparent sprite composition.
 */

#include "LostCrownGameplay.h"
#include "LostCrownGameAssets.h"
#include "LostCrownConfig.h"
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

static uint32_t       lastAnimTick         = 0;
static uint32_t       lastPhysicsTick      = 0;

// Maximum dimensions among all sprites (Jump is 38 x 42 = 1,596 words = 3,192 bytes)
static constexpr int16_t MAX_SPRITE_W = VEERA_JUMP_SPRITE_W;
static constexpr int16_t MAX_SPRITE_H = VEERA_JUMP_SPRITE_H;

static uint16_t       spriteCompositeBuffer[MAX_SPRITE_W * MAX_SPRITE_H];
static uint16_t       stripBuffer[MAX_SPRITE_W * MAX_SPRITE_H];

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

static void drawVeera(Adafruit_ST7735 &tft) {
  int16_t currX = (int16_t)roundf(veeraX);
  int16_t currY = (int16_t)roundf(veeraY);

  int16_t spriteW;
  int16_t spriteH;
  int16_t drawX;
  int16_t drawY;
  const uint16_t *frameData;

  if (currentState == VEERA_STATE_JUMPING) {
    spriteW   = VEERA_JUMP_SPRITE_W;
    spriteH   = VEERA_JUMP_SPRITE_H;
    drawX     = currX + VEERA_JUMP_OFFSET_X;
    drawY     = currY + VEERA_JUMP_OFFSET_Y;
    frameData = veera_jump_frames[currentJumpFrame];
  } else if (currentState == VEERA_STATE_RUNNING) {
    spriteW   = VEERA_SPRITE_W;
    spriteH   = VEERA_SPRITE_H;
    drawX     = currX;
    drawY     = currY;
    frameData = veera_run_frames[currentRunFrame];
  } else {
    // VEERA_STATE_IDLE
    spriteW   = VEERA_SPRITE_W;
    spriteH   = VEERA_SPRITE_H;
    drawX     = currX;
    drawY     = currY;
    frameData = veera_idle_pixels;
  }

  // 1. Clean previous frame area to eliminate trails and ghosting
  if (prevBoxX != -999) {
    if (drawY == prevBoxY && spriteW == prevBoxW && spriteH == prevBoxH) {
      // Ground running at same Y: single sliver dirty-rectangle optimization
      if (drawX > prevBoxX) {
        int16_t uncoveredW = drawX - prevBoxX;
        restoreBackgroundRect(tft, prevBoxX, prevBoxY, uncoveredW, prevBoxH);
      } else if (drawX < prevBoxX) {
        int16_t uncoveredX = drawX + spriteW;
        int16_t uncoveredW = prevBoxX - drawX;
        restoreBackgroundRect(tft, uncoveredX, prevBoxY, uncoveredW, prevBoxH);
      }
    } else {
      // Airborne jumping, landing, or sprite dimension shift: restore previous bounding box
      restoreBackgroundRect(tft, prevBoxX, prevBoxY, prevBoxW, prevBoxH);
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
      int16_t sc = (currentDirection == VEERA_DIR_LEFT) ? (spriteW - 1 - c) : c;
      uint16_t spritePixel = pgm_read_word(&frameData[r * spriteW + sc]);

      // Transparent keying
      if (spritePixel != VEERA_TRANSPARENT_COLOR) {
        spriteCompositeBuffer[r * spriteW + c] = spritePixel;
      } else {
        spriteCompositeBuffer[r * spriteW + c] = bgPixel;
      }
    }
  }

  // 3. Blit the composited buffer in a single hardware transaction
  tft.drawRGBBitmap(drawX, drawY, spriteCompositeBuffer, spriteW, spriteH);

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
  veeraX              = LC_VEERA_SPAWN_X;
  veeraY              = LC_VEERA_SPAWN_Y;
  velocityX           = 0.0f;
  velocityY           = 0.0f;
  isGroundedState     = true;
  currentState        = VEERA_STATE_IDLE;
  currentDirection    = VEERA_DIR_RIGHT;
  currentRunFrame     = 0;
  currentJumpFrame    = 0;
  prevBoxX            = -999;
  prevBoxY            = -999;
  prevBoxW            = 0;
  prevBoxH            = 0;

  uint32_t now        = millis();
  lastAnimTick        = now;
  lastPhysicsTick     = now;

  // 3. Render initial Veera IDLE sprite
  drawVeera(tft);
}

void update(Adafruit_ST7735 &tft) {
  uint32_t now = millis();
  uint32_t dt = now - lastPhysicsTick;
  if (dt > 100) dt = 100; // Cap large frame jumps
  lastPhysicsTick = now;

  // 1. Read joystick input
  int vrx = analogRead(JOY_X);
  int vry = analogRead(JOY_Y);

  bool needsRedraw = false;
  VeeraState oldState = currentState;
  VeeraDirection oldDir = currentDirection;
  uint8_t oldRunFrame = currentRunFrame;
  uint8_t oldJumpFrame = currentJumpFrame;
  int16_t oldPixelX = (int16_t)roundf(veeraX);
  int16_t oldPixelY = (int16_t)roundf(veeraY);

  // 2. Process Jump Trigger (Only when grounded)
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
      // UP + RIGHT: diagonal jump right
      velocityX = LC_JUMP_HORIZONTAL_SPEED;
      currentDirection = VEERA_DIR_RIGHT;
    } else if (vrx < LC_JOYSTICK_DEADZONE_LOW) {
      // UP + LEFT: diagonal jump left
      velocityX = -LC_JUMP_HORIZONTAL_SPEED;
      currentDirection = VEERA_DIR_LEFT;
    } else {
      // Straight UP: vertical jump
      velocityX = 0.0f;
    }
  }

  // 3. Update Physics & States
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
    // Ground horizontal movement & dead zone
    if (vrx > LC_JOYSTICK_DEADZONE_HIGH) {
      // Joystick RIGHT: Move right, running animation
      currentState = VEERA_STATE_RUNNING;
      currentDirection = VEERA_DIR_RIGHT;

      float dx = LC_VEERA_MOVE_SPEED * (float)dt;
      veeraX += dx;
      if (veeraX > (float)LC_VEERA_MAX_X) {
        veeraX = (float)LC_VEERA_MAX_X;
      }
    } else if (vrx < LC_JOYSTICK_DEADZONE_LOW) {
      // Joystick LEFT: Move left, mirrored running animation
      currentState = VEERA_STATE_RUNNING;
      currentDirection = VEERA_DIR_LEFT;

      float dx = LC_VEERA_MOVE_SPEED * (float)dt;
      veeraX -= dx;
      if (veeraX < (float)LC_VEERA_MIN_X) {
        veeraX = (float)LC_VEERA_MIN_X;
      }
    } else {
      // Joystick CENTER: Idle state, stop movement immediately
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

  // 4. State transition handling
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

  int16_t newPixelX = (int16_t)roundf(veeraX);
  int16_t newPixelY = (int16_t)roundf(veeraY);
  if (newPixelX != oldPixelX || newPixelY != oldPixelY) {
    needsRedraw = true;
  }

  // 5. Redraw only when visual state or position changed
  if (needsRedraw) {
    drawVeera(tft);
  }
}

void reset() {
  veeraX           = LC_VEERA_SPAWN_X;
  veeraY           = LC_VEERA_SPAWN_Y;
  velocityX        = 0.0f;
  velocityY        = 0.0f;
  isGroundedState  = true;
  currentState     = VEERA_STATE_IDLE;
  currentDirection = VEERA_DIR_RIGHT;
  currentRunFrame  = 0;
  currentJumpFrame = 0;
  prevBoxX         = -999;
  prevBoxY         = -999;
  prevBoxW         = 0;
  prevBoxH         = 0;
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

float getPositionX() {
  return veeraX;
}

float getPositionY() {
  return veeraY;
}

} // namespace LostCrownGameplay
