/**
 * LostCrownGameplay.cpp
 *
 * Implementation of the Lost Crown Training Ground gameplay scene.
 *
 * Features:
 *  - 128x160 Static Training Ground background drawn once on begin().
 *  - Veera Idle and 6-frame running cycle (run1 -> run2 -> ... -> run6 -> repeat).
 *  - Clean non-blocking millis() animation timing (RUN_FRAME_TIME).
 *  - Analog joystick horizontal control with deadzone filtering.
 *  - Real-time horizontal mirroring when facing LEFT.
 *  - Guaranteed foot anchoring to the grass ground line.
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

static int16_t        prevRenderX          = -999;
static int16_t        prevRenderY          = -999;
static VeeraState     prevRenderState      = (VeeraState)255;
static VeeraDirection prevRenderDirection  = (VeeraDirection)255;
static uint8_t        prevRenderFrame      = 255;

static VeeraState     currentState         = VEERA_STATE_IDLE;
static VeeraDirection currentDirection     = VEERA_DIR_RIGHT;
static uint8_t        currentRunFrame      = 0;

static uint32_t       lastAnimTick         = 0;
static uint32_t       lastPhysicsTick      = 0;

// RAM buffer for zero-flicker composite rendering (36 x 34 = 1,224 words = 2,448 bytes)
static uint16_t       spriteCompositeBuffer[VEERA_SPRITE_W * VEERA_SPRITE_H];
static uint16_t       stripBuffer[VEERA_SPRITE_W * VEERA_SPRITE_H];

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

static void renderVeeraComposite(Adafruit_ST7735 &tft, int16_t currX, int16_t currY) {
  // Select active sprite frame
  const uint16_t *frameData = (currentState == VEERA_STATE_IDLE)
                                ? veera_idle_pixels
                                : veera_run_frames[currentRunFrame];

  // Composite sprite over the background in local RAM buffer
  for (int16_t r = 0; r < VEERA_SPRITE_H; r++) {
    int16_t py = currY + r;
    for (int16_t c = 0; c < VEERA_SPRITE_W; c++) {
      int16_t px = currX + c;

      // Sample background pixel
      uint16_t bgPixel = 0x0000;
      if (px >= 0 && px < LC_BG_W && py >= 0 && py < LC_BG_H) {
        bgPixel = pgm_read_word(&image_Traning_ground_pixels[py * LC_BG_W + px]);
      }

      // Sample sprite pixel with horizontal flip if facing left
      int16_t sc = (currentDirection == VEERA_DIR_LEFT) ? (VEERA_SPRITE_W - 1 - c) : c;
      uint16_t spritePixel = pgm_read_word(&frameData[r * VEERA_SPRITE_W + sc]);

      // Transparent keying
      if (spritePixel != VEERA_TRANSPARENT_COLOR) {
        spriteCompositeBuffer[r * VEERA_SPRITE_W + c] = spritePixel;
      } else {
        spriteCompositeBuffer[r * VEERA_SPRITE_W + c] = bgPixel;
      }
    }
  }

  // Blit the composited buffer in a single hardware transaction
  tft.drawRGBBitmap(currX, currY, spriteCompositeBuffer, VEERA_SPRITE_W, VEERA_SPRITE_H);

  // Update render cache
  prevRenderX         = currX;
  prevRenderY         = currY;
  prevRenderState     = currentState;
  prevRenderDirection = currentDirection;
  prevRenderFrame     = currentRunFrame;
}

static void drawVeera(Adafruit_ST7735 &tft) {
  int16_t currX = (int16_t)roundf(veeraX);
  int16_t currY = (int16_t)roundf(veeraY);

  if (prevRenderX != -999) {
    if (currY != prevRenderY) {
      // Y changed (for future jumps/falls): restore full previous box
      restoreBackgroundRect(tft, prevRenderX, prevRenderY, VEERA_SPRITE_W, VEERA_SPRITE_H);
    } else if (currX > prevRenderX) {
      // Moved right: restore only the uncovered sliver on the left
      int16_t uncoveredW = currX - prevRenderX;
      restoreBackgroundRect(tft, prevRenderX, prevRenderY, uncoveredW, VEERA_SPRITE_H);
    } else if (currX < prevRenderX) {
      // Moved left: restore only the uncovered sliver on the right
      int16_t uncoveredX = currX + VEERA_SPRITE_W;
      int16_t uncoveredW = prevRenderX - currX;
      restoreBackgroundRect(tft, uncoveredX, prevRenderY, uncoveredW, VEERA_SPRITE_H);
    }
  }

  renderVeeraComposite(tft, currX, currY);
}

// ─── Public API ──────────────────────────────────────────────────────────────

void begin(Adafruit_ST7735 &tft) {
  // 1. Draw static Training Ground background ONCE
  tft.drawRGBBitmap(0, 0, image_Traning_ground_pixels, LC_BG_W, LC_BG_H);

  // 2. Set Veera's starting position and state
  veeraX              = LC_VEERA_SPAWN_X;
  veeraY              = LC_VEERA_SPAWN_Y;
  currentState        = VEERA_STATE_IDLE;
  currentDirection    = VEERA_DIR_RIGHT;
  currentRunFrame     = 0;
  prevRenderX         = -999;
  prevRenderY         = -999;
  prevRenderState     = (VeeraState)255;
  prevRenderDirection = (VeeraDirection)255;
  prevRenderFrame     = 255;

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

  bool needsRedraw = false;
  VeeraState oldState = currentState;
  VeeraDirection oldDir = currentDirection;

  // 2. Process horizontal movement & dead zone
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

  // 3. Handle state transitions
  if (currentState != oldState) {
    if (currentState == VEERA_STATE_IDLE) {
      currentRunFrame = 0;
    } else {
      currentRunFrame = 0;
      lastAnimTick = now;
    }
    needsRedraw = true;
  }

  if (currentDirection != oldDir) {
    needsRedraw = true;
  }

  // 4. Update running animation timer (millis-based)
  if (currentState == VEERA_STATE_RUNNING) {
    if (now - lastAnimTick >= LC_RUN_FRAME_TIME_MS) {
      lastAnimTick = now;
      currentRunFrame = (currentRunFrame + 1) % VEERA_RUN_FRAME_COUNT;
      needsRedraw = true;
    }
  }

  // 5. Check if pixel position changed
  int16_t currPixelX = (int16_t)roundf(veeraX);
  int16_t currPixelY = (int16_t)roundf(veeraY);
  if (currPixelX != prevRenderX || currPixelY != prevRenderY) {
    needsRedraw = true;
  }

  // 6. Redraw only when needed
  if (needsRedraw) {
    drawVeera(tft);
  }
}

void reset() {
  veeraX           = LC_VEERA_SPAWN_X;
  veeraY           = LC_VEERA_SPAWN_Y;
  currentState     = VEERA_STATE_IDLE;
  currentDirection = VEERA_DIR_RIGHT;
  currentRunFrame  = 0;
  prevRenderX      = -999;
  prevRenderY      = -999;
}

VeeraState getState() {
  return currentState;
}

VeeraDirection getDirection() {
  return currentDirection;
}

float getPositionX() {
  return veeraX;
}

float getPositionY() {
  return veeraY;
}

} // namespace LostCrownGameplay
