/**
 * LostCrownLoading.cpp
 *
 * Modular, editable, and animated loading screen for Lost Crown on the
 * 128x160 ST7735 TFT display.
 *
 * Architecture:
 *   - Static Layer: 128x160 high-fidelity background asset rendered once on begin()
 *   - Dynamic UI Layer:
 *       - Procedural golden amber loading bar with dirty-rect optimization (0..100%)
 *       - Customizable "LOADING..." text with auto-centering
 *   - Dynamic Animation Layer (non-blocking millis timers):
 *       - Torch / campfire flame flickering (3 frames)
 *       - Villain red eye glowing / breathing (3 frames)
 *       - Hero red cape tip fluttering (2 frames)
 *       - Sacred river water reflections shimmering
 */

#include "LostCrownLoading.h"
#include "LostCrownConfig.h"
#include "LostCrownAssets.h"
#include <Adafruit_GFX.h>

namespace LostCrownLoading {

// ─── State Tracking ──────────────────────────────────────────────────────────
static uint32_t startedAt           = 0;
static int      lastProgress        = -1;
static int16_t  currentFilledWidth  = 0;

static uint32_t lastFlameTime       = 0;
static uint8_t  flameFrame          = 0;

static uint32_t lastEyeTime         = 0;
static uint8_t  eyeFrame            = 0;

static uint32_t lastCapeTime        = 0;
static uint8_t  capeFrame           = 0;

static uint32_t lastWaterTime       = 0;
static bool     waterShimmerState   = false;

static bool     isInitialized       = false;

// ─── Helper Functions ────────────────────────────────────────────────────────

static void drawLoadingText(Adafruit_ST7735 &tft) {
  tft.setFont(NULL); // Ensure clean default 5x7 GFX font
  tft.setTextSize(LC_LOADING_TEXT_SIZE);
  tft.setTextColor(LC_LOADING_TEXT_COLOR, LC_LOADING_TEXT_BG);

  int16_t x = LC_LOADING_TEXT_X;
  if (x < 0) {
    // Auto-center text horizontally across the 128px screen
    size_t len = strlen(LC_LOADING_TEXT);
    int16_t textWidth = (int16_t)(len * 6);
    x = (128 - textWidth) / 2;
    if (x < 0) x = 0;
  }

  tft.setCursor(x, LC_LOADING_TEXT_Y);
  tft.print(LC_LOADING_TEXT);
}

static void drawTorchFlame(Adafruit_ST7735 &tft, uint8_t frame) {
  const uint16_t *frameData = lc_flame_frames[frame % 3];
  tft.drawRGBBitmap(LC_TORCH_X, LC_TORCH_Y, frameData, LC_TORCH_W, LC_TORCH_H);
}

static void drawVillainEye(Adafruit_ST7735 &tft, uint8_t frame) {
  const uint16_t *frameData = lc_eye_frames[frame % 3];
  tft.drawRGBBitmap(LC_EYE_X, LC_EYE_Y, frameData, LC_EYE_W, LC_EYE_H);
}

static void drawHeroCape(Adafruit_ST7735 &tft, uint8_t frame) {
  const uint16_t *frameData = lc_cape_frames[frame % 2];
  tft.drawRGBBitmap(LC_CAPE_X, LC_CAPE_Y, frameData, LC_CAPE_W, LC_CAPE_H);
}

static void drawWaterShimmer(Adafruit_ST7735 &tft, bool active) {
  for (uint8_t i = 0; i < 5; i++) {
    WaterShimmerPoint pt;
    memcpy_P(&pt, &lc_water_shimmers[i], sizeof(WaterShimmerPoint));
    uint16_t color = active ? pt.shimmerColor : pt.baseColor;
    tft.drawPixel(pt.x, pt.y, color);
    if (active) {
      tft.drawPixel(pt.x + 1, pt.y, pt.baseColor);
    }
  }
}

static void drawLoadingBar(Adafruit_ST7735 &tft, int progress) {
  if (progress < 0) progress = 0;
  if (progress > 100) progress = 100;

  int16_t targetFilled = (int16_t)((LC_LOADING_BAR_WIDTH * progress) / 100);

  if (targetFilled == 0 && currentFilledWidth > 0) {
    // Reset bar trough
    tft.fillRect(LC_LOADING_BAR_X, LC_LOADING_BAR_Y, LC_LOADING_BAR_WIDTH, LC_LOADING_BAR_HEIGHT, LC_BAR_TROUGH_COLOR);
    currentFilledWidth = 0;
    return;
  }

  if (targetFilled > currentFilledWidth) {
    // Restore previous sparkle position to gradient color
    if (currentFilledWidth > 0) {
      int16_t prevSparkleX = LC_LOADING_BAR_X + currentFilledWidth - 1;
      tft.drawPixel(prevSparkleX, LC_LOADING_BAR_Y + 1, LC_BAR_GRADIENT[1]);
    }

    // Draw newly filled vertical column slices
    for (int16_t col = currentFilledWidth; col < targetFilled; col++) {
      int16_t x = LC_LOADING_BAR_X + col;
      for (int16_t row = 0; row < LC_LOADING_BAR_HEIGHT; row++) {
        tft.drawPixel(x, LC_LOADING_BAR_Y + row, LC_BAR_GRADIENT[row]);
      }
    }

    // Draw leading edge sparkle pixel at row 1
    int16_t newSparkleX = LC_LOADING_BAR_X + targetFilled - 1;
    tft.drawPixel(newSparkleX, LC_LOADING_BAR_Y + 1, LC_BAR_SPARKLE_COLOR);

    currentFilledWidth = targetFilled;
  }
}

// ─── Public API ──────────────────────────────────────────────────────────────

void begin(Adafruit_ST7735 &tft) {
  reset();
  startedAt     = millis();
  lastFlameTime = startedAt;
  lastEyeTime   = startedAt;
  lastCapeTime  = startedAt;
  lastWaterTime = startedAt;
  isInitialized = true;

  // 1. Draw complete static background artwork
  tft.drawRGBBitmap(0, 0, image_LostCrown_background, 128, 160);

  // 2. Initialize the empty loading bar trough
  tft.fillRect(LC_LOADING_BAR_X, LC_LOADING_BAR_Y, LC_LOADING_BAR_WIDTH, LC_LOADING_BAR_HEIGHT, LC_BAR_TROUGH_COLOR);
  currentFilledWidth = 0;
  lastProgress       = 0;

  // 3. Render the editable loading text
  drawLoadingText(tft);

  // 4. Render initial frames of dynamic micro-sprites
  drawTorchFlame(tft, flameFrame);
  drawVillainEye(tft, eyeFrame);
  drawHeroCape(tft, capeFrame);
  drawWaterShimmer(tft, waterShimmerState);
}

bool update(Adafruit_ST7735 &tft) {
  if (!isInitialized) {
    begin(tft);
  }

  const uint32_t now     = millis();
  const uint32_t elapsed = now - startedAt;

  // ── 1. Torch Flame Animation ──
  if (now - lastFlameTime >= LC_FIRE_FRAME_MS) {
    lastFlameTime = now;
    flameFrame = (flameFrame + 1) % 3;
    drawTorchFlame(tft, flameFrame);
  }

  // ── 2. Villain Eye Pulse Animation ──
  if (now - lastEyeTime >= LC_EYE_FRAME_MS) {
    lastEyeTime = now;
    eyeFrame = (eyeFrame + 1) % 3;
    drawVillainEye(tft, eyeFrame);
  }

  // ── 3. Hero Cape Flutter Animation ──
  if (now - lastCapeTime >= LC_CAPE_FRAME_MS) {
    lastCapeTime = now;
    capeFrame = (capeFrame + 1) % 2;
    drawHeroCape(tft, capeFrame);
  }

  // ── 4. River Reflection Shimmer Animation ──
  if (now - lastWaterTime >= LC_WATER_FRAME_MS) {
    lastWaterTime = now;
    waterShimmerState = !waterShimmerState;
    drawWaterShimmer(tft, waterShimmerState);
  }

  // ── 5. Procedural Loading Bar Progress ──
  int progress;
  if (elapsed >= LC_LOADING_DURATION_MS) {
    progress = 100;
  } else {
    // Natural smooth cubic ease-in-out curve
    float ratio = (float)elapsed / (float)LC_LOADING_DURATION_MS;
    float f;
    if (ratio < 0.5f) {
      f = 4.0f * ratio * ratio * ratio;
    } else {
      float t = -2.0f * ratio + 2.0f;
      f = 1.0f - (t * t * t) / 2.0f;
    }
    progress = (int)(f * 100.0f);
  }

  if (progress != lastProgress) {
    drawLoadingBar(tft, progress);
    lastProgress = progress;
  }

  // Returns true once loading duration + hold time have completed
  return elapsed >= (LC_LOADING_DURATION_MS + LC_COMPLETE_HOLD_MS);
}

void reset() {
  startedAt          = 0;
  lastProgress       = -1;
  currentFilledWidth = 0;
  flameFrame         = 0;
  eyeFrame           = 0;
  capeFrame          = 0;
  waterShimmerState  = false;
  isInitialized      = false;
}

} // namespace LostCrownLoading
