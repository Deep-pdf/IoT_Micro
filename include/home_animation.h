#ifndef HOME_ANIMATION_H
#define HOME_ANIMATION_H

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>

// =============================================================================
// Home Screen Looping JPG Animation Configuration
// =============================================================================

// Configurable frame interval in milliseconds
// 66ms ≈ 15 FPS (smooth, low CPU overhead)
// 80ms = 12.5 FPS
// 100ms = 10 FPS
#define ANIM_FRAME_INTERVAL_MS 66

// Rendering mode:
// 0 = Reserved Right-Side Media Panel (1:2 hardware scale, 64x96 at X=63, Y=18) [Default & Recommended]
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

// Renders a specific frame (0-indexed: 0..29) with proper Z-ordering
void renderJoeFrame(Adafruit_ST7735 &tft, int frameIndex);

#endif // HOME_ANIMATION_H
