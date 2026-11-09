#ifndef HOME_ANIMATION_H
#define HOME_ANIMATION_H

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>

// =============================================================================
// Home Screen Looping JPG Animation Configuration
// =============================================================================

// Configurable frame interval in milliseconds
// Doubled from 66ms to 132ms for exactly 50% speed reduction (~7.5 FPS)
#define ANIM_FRAME_INTERVAL_MS 132

// Rendering mode:
// 0 = Reserved Right-Side Media Panel (1:2 hardware scale, 64x96 at X=64, Y=26, sticking to side & bottom bar) [Default & Recommended]
// 1 = Fullscreen background (1:1 scale, 128x192 clipped to Y=10..121, Z-order under text)
#ifndef ANIM_USE_FULLSCREEN_ZORDER
#define ANIM_USE_FULLSCREEN_ZORDER 0
#endif

// Starts or resets animation to frame 1 forward, displaying frame 1 immediately
void startHomeAnimation(Adafruit_ST7735 &tft);

// Steps the animation if elapsed time has passed (non-blocking, millis()-based)
void stepHomeAnimation(Adafruit_ST7735 &tft);

// Stops animation playback when leaving home screen
void stopHomeAnimation();

// Returns true if animation is currently active
bool isHomeAnimationActive();

// Returns the current animation frame index (0..JOE_FRAME_COUNT-1)
int getHomeAnimationFrame();

// Renders a specific frame (0-indexed: 0..29) with proper Z-ordering
void renderJoeFrame(Adafruit_ST7735 &tft, int frameIndex);

#endif // HOME_ANIMATION_H
