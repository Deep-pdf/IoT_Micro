#ifndef BUTTON_H
#define BUTTON_H

// Initialize the push button pin (GPIO 13) as INPUT_PULLUP
void setupButton();

// Polls the button state, performs non-blocking debouncing, and detects
// the transition from HIGH to LOW (falling edge). Call this in the main loop().
void updateButton();

// Returns true if the button was pressed since the last time this function was called.
// Consumes the press event (resets the flag to false).
bool isEnterPressed();
bool isBackPressed();

#endif // BUTTON_H
