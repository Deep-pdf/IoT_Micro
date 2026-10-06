/**
 * LostCrownConfig.h
 *
 * Centralized, developer-editable configuration for the Lost Crown loading
 * and main title screens. Adjust timings, dimensions, colors, text, and sprite
 * coordinates without modifying the rendering engine.
 */
#pragma once

#include <Arduino.h>

// =====================================================================
// LOADING SCREEN CONFIGURATION
// =====================================================================

// ===== 1. TIMINGS (in milliseconds) =====
// Total duration for progress bar to fill from 0% to 100%
static constexpr uint32_t LC_LOADING_DURATION_MS = 4000;

// Time to hold the complete 100% screen before signaling game launch
static constexpr uint32_t LC_COMPLETE_HOLD_MS    = 500;

// Animation frame intervals (non-blocking millis)
static constexpr uint32_t LC_FIRE_FRAME_MS       = 140; // Torch flame flicker rate
static constexpr uint32_t LC_EYE_FRAME_MS        = 500; // Villain eye pulse / breathe rate
static constexpr uint32_t LC_WATER_FRAME_MS      = 220; // River reflection shimmer interval
static constexpr uint32_t LC_CAPE_FRAME_MS       = 240; // Hero cape flutter rate
static constexpr uint32_t LC_MOON_PULSE_MS       = 800; // Moon subtle glow cycle

// ===== 2. LOADING BAR GEOMETRY =====
// Ornate outer frame position (reference: x=18..109, y=128..136)
static constexpr int16_t  LC_BAR_FRAME_X         = 18;
static constexpr int16_t  LC_BAR_FRAME_Y         = 128;
static constexpr int16_t  LC_BAR_FRAME_W         = 92;
static constexpr int16_t  LC_BAR_FRAME_H         = 9;

// Interior fillable bar trough (x=22..105, y=130..134)
static constexpr int16_t  LC_LOADING_BAR_X       = 22;
static constexpr int16_t  LC_LOADING_BAR_Y       = 130;
static constexpr int16_t  LC_LOADING_BAR_WIDTH   = 84;
static constexpr int16_t  LC_LOADING_BAR_HEIGHT  = 5;

// ===== 3. LOADING BAR COLORS (RGB565) =====
// Empty trough background color (dark shadow tone)
static constexpr uint16_t LC_BAR_TROUGH_COLOR    = 0x1082;

// 5-level vertical amber gradient matching the original Lopaka artwork
static constexpr uint16_t LC_BAR_GRADIENT[5] = {
  0xBD43, // Row 0 (y=130): Top subtle amber reflection
  0xFD65, // Row 1 (y=131): Bright golden highlight
  0xFCC3, // Row 2 (y=132): Rich amber
  0xEC01, // Row 3 (y=133): Warm deep orange
  0xC2A0  // Row 4 (y=134): Bottom shadow rim
};

// Sparkle pixel at the leading edge of the progress fill
static constexpr uint16_t LC_BAR_SPARKLE_COLOR   = 0xFFE0;

// ===== 4. EDITABLE LOADING TEXT =====
static const char* const  LC_LOADING_TEXT        = "LOADING...";
static constexpr int16_t  LC_LOADING_TEXT_X      = 44;
static constexpr int16_t  LC_LOADING_TEXT_Y      = 120;
static constexpr uint8_t  LC_LOADING_TEXT_SIZE   = 1;
static constexpr uint16_t LC_LOADING_TEXT_COLOR  = 0xE586; // Warm parchment gold
static constexpr uint16_t LC_LOADING_TEXT_BG     = 0x0821; // Surrounding rock background

// ===== 5. DYNAMIC SPRITE COORDINATES =====
// Torch flame (11x11 sprite)
static constexpr int16_t  LC_TORCH_X             = 112;
static constexpr int16_t  LC_TORCH_Y             = 93;
static constexpr int16_t  LC_TORCH_W             = 11;
static constexpr int16_t  LC_TORCH_H             = 11;

// Villain eye (4x3 sprite)
static constexpr int16_t  LC_EYE_X               = 85;
static constexpr int16_t  LC_EYE_Y               = 21;
static constexpr int16_t  LC_EYE_W               = 4;
static constexpr int16_t  LC_EYE_H               = 3;

// Hero cape tip (9x6 sprite)
static constexpr int16_t  LC_CAPE_X              = 6;
static constexpr int16_t  LC_CAPE_Y              = 93;
static constexpr int16_t  LC_CAPE_W              = 9;
static constexpr int16_t  LC_CAPE_H              = 6;

// Moon center & radius
static constexpr int16_t  LC_MOON_CX             = 68;
static constexpr int16_t  LC_MOON_CY             = 22;
static constexpr int16_t  LC_MOON_R              = 6;


// =====================================================================
// MAIN TITLE SCREEN CONFIGURATION
// =====================================================================

// ===== 6. TITLE SCREEN MENU OPTIONS =====
enum LostCrownMenuItem {
  LC_MENU_CONTINUE = 0,
  LC_MENU_NEW_GAME,
  LC_MENU_SETTINGS,
  LC_MENU_EXTRAS,
  LC_MENU_QUIT,
  LC_MENU_COUNT
};

static const char* const LC_MENU_LABELS[LC_MENU_COUNT] = {
  "CONTINUE",
  "NEW GAME",
  "SETTINGS",
  "EXTRAS",
  "QUIT"
};

// Menu typography & position
static constexpr int16_t  LC_TITLE_MENU_X        = 17; // Left-aligned text X
static constexpr int16_t  LC_TITLE_MENU_Y        = 53; // First item (CONTINUE) Y
static constexpr int16_t  LC_TITLE_MENU_SPACING  = 9;  // Vertical line pitch (padding between options)
static constexpr int16_t  LC_TITLE_ARROW_X       = 7;  // Selection arrow indicator X

// Menu colors (RGB565)
static constexpr uint16_t LC_COLOR_SELECTED      = 0xFEA0; // Bright golden amber (matching "LOST")
static constexpr uint16_t LC_COLOR_UNSELECTED    = 0xD6BA; // Cool silver/off-white (matching "CROWN")
static constexpr uint16_t LC_COLOR_SHADOW        = 0x0821; // Dark contrast outline shadow

// =====================================================================
// TRAINING GROUND GAMEPLAY CONFIGURATION
// =====================================================================
// Animation timing (millis per running frame)
static constexpr uint32_t LC_RUN_FRAME_TIME_MS      = 110;

// Character horizontal movement speed (pixels per millisecond)
static constexpr float    LC_VEERA_MOVE_SPEED       = 0.055f;

// Joystick deadzone thresholds (analog 0..4095)
static constexpr int      LC_JOYSTICK_DEADZONE_LOW  = 1400;
static constexpr int      LC_JOYSTICK_DEADZONE_HIGH = 2700;

// Playable boundaries for Veera on 128x160 screen (with 36px sprite)
static constexpr int16_t  LC_VEERA_MIN_X            = 0;
static constexpr int16_t  LC_VEERA_MAX_X            = 92; // 128 - 36
static constexpr int16_t  LC_VEERA_SPAWN_X          = 46;
static constexpr int16_t  LC_VEERA_SPAWN_Y          = 102; // Feet anchored to y=135

// ===== JUMPING CONFIGURATION =====
// Joystick UP threshold to trigger jump (analog 0..4095, typically < 1000 when pushed UP)
static constexpr int      LC_JOYSTICK_UP_THRESHOLD   = 1200;

// Jump physics parameters
static constexpr float    LC_JUMP_FORCE              = 0.20f;    // Initial upward velocity (pixels/ms)
static constexpr float    LC_GRAVITY                 = 0.00072f; // Downward acceleration (pixels/ms^2)
static constexpr float    LC_JUMP_HORIZONTAL_SPEED   = 0.055f;   // Horizontal speed during diagonal jump (pixels/ms)

// Animation timing for jump frames (millis per frame during jump)
static constexpr uint32_t LC_JUMP_FRAME_TIME_MS      = 140;

// ===== ATTACK CONFIGURATION =====
// Animation timing (millis per attack frame, ~240 ms total for 4-frame sword attack)
static constexpr uint32_t LC_ATTACK_FRAME_TIME_MS    = 60;

// ===== BOOMERANG THROW CONFIGURATION =====
// Animation timing (millis per throw animation frame)
static constexpr uint32_t LC_THROW_FRAME1_TIME_MS      = 120; // Veera_throw1: preparation windup
static constexpr uint32_t LC_THROW_FRAME2_TIME_MS      = 120; // Veera_throw2: release pose
static constexpr uint32_t LC_THROW_FRAME4_TIME_MS      = 160; // Veera_throw4: catch recovery

// Boomerang flight speed (pixels per millisecond)
static constexpr float    LC_BOOMERANG_SPEED            = 0.11f;

// Boomerang rotation spin interval (millis per 45-degree frame)
static constexpr uint32_t LC_BOOMERANG_ROT_INTERVAL_MS = 35;

// Boomerang max travel distance percent of available horizontal space toward boundary
static constexpr float    LC_BOOMERANG_MAX_DIST_PERCENT = 0.60f;

// Hand launch & catch offsets from Veera sprite position (veeraX, veeraY)
static constexpr int16_t  LC_BOOMERANG_OFFSET_X_RIGHT   = 24;
static constexpr int16_t  LC_BOOMERANG_OFFSET_X_LEFT    = -2;
static constexpr int16_t  LC_BOOMERANG_OFFSET_Y         = 10;

// Back button hold time to quit/return from gameplay (in milliseconds)
static constexpr uint32_t BACK_LONG_PRESS_TIME         = 3000;



