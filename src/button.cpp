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
static bool backPressedEvent = false;

void setupButton() {
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(25, INPUT_PULLUP); // BACK button
}

void updateButton() {
  // Update ENTER Button
  int reading = digitalRead(BUTTON_PIN);
  if (reading != lastRawState) {
    lastDebounceTime = millis();
    lastRawState = reading;
  }
  if ((millis() - lastDebounceTime) > DEBOUNCE_DELAY_MS) {
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
    lastBackDebounceTime = millis();
    lastBackRawState = backReading;
  }
  if ((millis() - lastBackDebounceTime) > DEBOUNCE_DELAY_MS) {
    if (backReading != lastBackStableState) {
      lastBackStableState = backReading;
      if (lastBackStableState == LOW) {
        backPressedEvent = true;
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

bool isBackPressed() {
  if (backPressedEvent) {
    backPressedEvent = false; // Consume the event
    return true;
  }
  return false;
}
