#include <Arduino.h>
#include "button.h"

#define BUTTON_PIN 13
#define DEBOUNCE_DELAY_MS 50 // Standard debounce delay

// Internal state variables for debouncing and edge detection
static int lastStableState = HIGH; // Idle state is HIGH since it's pulled up
static int lastRawState = HIGH;
static unsigned long lastDebounceTime = 0;
static bool enterPressedEvent = false;

void setupButton() {
  pinMode(BUTTON_PIN, INPUT_PULLUP);
}

void updateButton() {
  int reading = digitalRead(BUTTON_PIN);

  // If the raw input state changed (due to mechanical noise or pressing/releasing)
  if (reading != lastRawState) {
    lastDebounceTime = millis();
    lastRawState = reading;
  }

  // Once the raw input state is stable for at least DEBOUNCE_DELAY_MS
  if ((millis() - lastDebounceTime) > DEBOUNCE_DELAY_MS) {
    // Check if the stable state has changed
    if (reading != lastStableState) {
      lastStableState = reading;

      // Detect falling edge (HIGH -> LOW transition = press event)
      if (lastStableState == LOW) {
        enterPressedEvent = true;
      }
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
