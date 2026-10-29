#ifndef STATUS_LED_H
#define STATUS_LED_H

#include <Arduino.h>

// ==============================================================================
// ── USER CONFIGURATION (MAIN 2 OPTIONS) ──────────────────────────────────────
// ==============================================================================

// ------------------------------------------------------------------------------
// [OPTION 1] MASTER POWER SWITCH:
// Choose whether you want the green light ON while running your project.
//   1 (or true)  : Turn ON the light when the device powers on / boots.
//   0 (or false) : Do NOT turn on the light on next boot (stays dark).
// ------------------------------------------------------------------------------
#define STATUS_LED_ENABLED          1

// ------------------------------------------------------------------------------
// [OPTION 2] LIGHT MODE (BREATHING / SMOOTH PULSE vs SOLID ON):
// Choose the light effect while powered on:
//   1 (or true)  : Light smoothly fades in and smoothly fades out continuously
//                  the entire time power is on (smooth breathing effect).
//   0 (or false) : Solid steady light (stays constantly ON without fading/blinking).
// ------------------------------------------------------------------------------
#define STATUS_LED_BREATHE_MODE     1

// ------------------------------------------------------------------------------
// ── TIMING & BRIGHTNESS VARIATIONS ───────────────────────────────────────────
// Customize the breathing speed and brightness curve variations here:
// ------------------------------------------------------------------------------
// Full cycle duration in milliseconds (fade-in + fade-out):
// Variations you can try:
//   - 1200 : Fast energetic pulse
//   - 2500 : Default gentle natural breathing
//   - 4000 : Calm, slow ambient glow
#define STATUS_LED_BREATHE_PERIOD_MS  4000

// Update resolution step in milliseconds (20ms = 50 updates/sec, ultra-fluid):
#define STATUS_LED_UPDATE_STEP_MS     20

// Minimum & Maximum brightness range (0 to 255):
#define STATUS_LED_MIN_BRIGHTNESS     0
#define STATUS_LED_MAX_BRIGHTNESS     55

// ------------------------------------------------------------------------------
// ── HARDWARE PIN ASSIGNMENT ───────────────────────────────────────────────────
// ------------------------------------------------------------------------------
#define STATUS_LED_PIN                26    // GPIO 26 connected to Green LED with resistor
#define STATUS_LED_PWM_CHANNEL        0     // ESP32 LEDC PWM channel (0 - 15)
#define STATUS_LED_PWM_FREQ           5000  // 5 kHz PWM frequency (flicker-free)
#define STATUS_LED_PWM_RES            8     // 8-bit resolution (0 - 255)

// ==============================================================================
// ── FUNCTION PROTOTYPES ──────────────────────────────────────────────────────
// ==============================================================================

// Call once in setup() to start the LED controller according to the options above
void initStatusLed();

// Optional runtime helpers (if you ever wish to dynamically adjust the LED from code)
void setStatusLedEnabled(bool enabled);
void setStatusLedBreathe(bool breathe);
void setStatusLedDuty(uint8_t duty);

#endif // STATUS_LED_H
