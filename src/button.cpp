#include <Arduino.h>
#include "button.h"

#define BUTTON_PIN 13
#define DEBOUNCE_DELAY_MS 50 // Standard debounce delay

// Internal state variables for debouncing and edge detection
static int lastStableState = HIGH; // Idle state is HIGH since it's pulled up
static int lastRawState = HIGH;
static unsigned long lastDebounceTime = 0;
static bool enterPressedEvent = false;

static int lastBackStableState = HIGH;
static int lastBackRawState = HIGH;
static unsigned long lastBackDebounceTime = 0;
static unsigned long backPressStartTime = 0;
static bool backPressedEvent = false;
static bool backLongPressedEvent = false;
static bool backLongPressHandled = false;

void setupButton() {
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(25, INPUT_PULLUP); // BACK button
}

void updateButton() {
  unsigned long now = millis();

  // Update ENTER Button
  int reading = digitalRead(BUTTON_PIN);
  if (reading != lastRawState) {
    lastDebounceTime = now;
    lastRawState = reading;
  }
  if ((now - lastDebounceTime) > DEBOUNCE_DELAY_MS) {
    if (reading != lastStableState) {
      lastStableState = reading;
      if (lastStableState == LOW) {
        enterPressedEvent = true;
      }
    }
  }

  // Update BACK Button
  int backReading = digitalRead(25);
  if (backReading != lastBackRawState) {
    lastBackDebounceTime = now;
    lastBackRawState = backReading;
  }
  if ((now - lastBackDebounceTime) > DEBOUNCE_DELAY_MS) {
    if (backReading != lastBackStableState) {
      lastBackStableState = backReading;
      if (lastBackStableState == LOW) {
        // Transition to LOW -> Pressed down
        backPressStartTime = now;
        backLongPressHandled = false;
      } else {
        // Transition to HIGH -> Released
        if (!backLongPressHandled) {
          backPressedEvent = true; // Short press click
        }
      }
    }
  }

  // Check 5 second hold (5000 ms) while BACK button remains held LOW
  if (lastBackStableState == LOW && !backLongPressHandled) {
    if ((now - backPressStartTime) >= 5000) {
      backLongPressedEvent = true;
      backLongPressHandled = true;
    }
  }
}

bool isEnterPressed() {
  if (enterPressedEvent) {
    enterPressedEvent = false; // Consume the event
    return true;
  }
  return false;
}

bool isBackPressed() {
  if (backPressedEvent) {
    backPressedEvent = false; // Consume the event
    return true;
  }
  return false;
}

bool isBackLongPressed() {
  if (backLongPressedEvent) {
    backLongPressedEvent = false; // Consume the event
    return true;
  }
  return false;
}

void clearButtonEvents() {
  enterPressedEvent = false;
  backPressedEvent = false;
  backLongPressedEvent = false;
}

