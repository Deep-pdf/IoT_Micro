#ifndef AI_APP_H
#define AI_APP_H

#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>

// ── Internal Chat AI state machine ───────────────────────────
enum AIChatState {
    CHAT_KEYBOARD,   // On-screen keyboard — user types a question
    CHAT_THINKING,   // Waiting for the Gemini / Flask response
    CHAT_RESPONSE    // Displaying the Gemini response
};

class AIApp {
public:
    // Called once when the user enters the Chat AI app.
    static void init(Adafruit_ST7735 &tft);

    // Called every loop() iteration while STATE_AI is active.
    // Handles keyboard input, state transitions, and rendering.
    static void update(Adafruit_ST7735 &tft);

    // Returns true (and resets the flag) when the app wants to
    // exit back to the home screen.  main.cpp calls exitAI() on true.
    static bool shouldExit();
};

#endif // AI_APP_H
