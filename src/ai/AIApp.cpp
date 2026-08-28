#include "AIApp.h"
#include "AIKeyboard.h"
#include "AIConnection.h"
#include "BLADRMF_4pt7b.h"
#include "button.h"
#include <Arduino.h>
#include <string.h>

// ============================================================
//  HARDWARE PINS
// ============================================================
#define JOY_X_PIN   34
#define JOY_Y_PIN   35

// ============================================================
//  TIMING
// ============================================================
#define CURSOR_BLINK_MS   500   // cursor toggle period (ms)
#define JOY_DEAD_LO      1500   // joystick dead-zone lower bound (ADC counts)
#define JOY_DEAD_HI      2700   // joystick dead-zone upper bound
#define JOY_THRESH_LO    1000   // below this â†’ direction detected
#define JOY_THRESH_HI    3000   // above this â†’ direction detected
#define JOY_INIT_DELAY    200   // ms before first auto-repeat fires
#define JOY_REPEAT_RATE   120   // ms between subsequent auto-repeats

// ============================================================
//  LIMITS
// ============================================================
#define MAX_INPUT_LEN   120     // maximum typed question length

// ============================================================
//  SCREEN LAYOUT
// ============================================================
#define HDR_H        14         // Header height     (Y=0 .. 13)
#define INP_Y        14         // Input box Y-start (Y=14 .. 41)
#define INP_H        28         // Input box height
#define KB_Y         42         // Keyboard area Y-start
#define INP_CHARS    20         // chars per line  floor(122 px / 6 px)
#define INP_LINES     3         // text lines inside input box

// ============================================================
//  COLOURS  (RGB-565)
// ============================================================
#define COL_BG         0x0000   // Black
#define COL_HDR_BG     0x0008   // Very dark navy
#define COL_HDR_TXT    0xFFFF   // White
#define COL_HDR_ACC    0x07FF   // Cyan accent  (small indicators)
#define COL_INP_BDR    0xF81F   // Magenta / pink border (input box)
#define COL_INP_TXT    0xFFFF   // White (typed text)
#define COL_INP_PHLD   0x7BEF   // Gray  (placeholder)
#define COL_CURSOR     0xFFFF   // White cursor underscore
#define COL_THINK_TXT  0x07FF   // Cyan  "Thinkingâ€¦"
#define COL_RESP_TXT   0xFFFF   // White response
#define COL_HINT_TXT   0x8410   // Medium gray hint line

// ============================================================
//  STATIC STATE
// ============================================================
static AIChatState  aiState         = CHAT_KEYBOARD;
static String       inputText       = "";
static String       responseText    = "";

// Cursor blink
static bool          cursorVisible  = true;
static unsigned long lastBlink      = 0;
static bool          inputDirty     = true;

// Joystick auto-repeat
static bool          joyCentered    = true;
static bool          joyMoving      = false;
static unsigned long joyMoveStart   = 0;
static unsigned long lastRepeat     = 0;
static int           lastDx         = 0;
static int           lastDy         = 0;

// Exit signal for main.cpp
static bool          _wantsExit     = false;

// ============================================================
//  HELPER â€” drawHeader
// ============================================================
static void drawHeader(Adafruit_ST7735 &tft) {
    tft.fillRect(0, 0, 128, HDR_H, COL_HDR_BG);

    // "DEEPAI" centred using BLADRMF_4pt7b
    tft.setFont(&BLADRMF_4pt7b);
    tft.setTextSize(1);
    tft.setTextColor(COL_HDR_TXT, COL_HDR_BG);

    int16_t x1, y1;
    uint16_t tw, th;
    tft.getTextBounds("DEEPAI", 0, HDR_H - 2, &x1, &y1, &tw, &th);
    // x1 is the offset from cursor to left of bounding box
    tft.setCursor((128 - (int16_t)tw) / 2 - x1, HDR_H - 2);
    tft.print("DEEPAI");

    // Small accent indicators (default font, textSize=1)
    tft.setFont(NULL);
    tft.setTextSize(1);
    tft.setTextColor(COL_HDR_ACC, COL_HDR_BG);
    tft.setCursor(2,   3);  tft.print(">");   // left robot indicator
    tft.setCursor(118, 3);  tft.print("~");   // right Wi-Fi indicator
}

// ============================================================
//  HELPER â€” drawInputBox
// ============================================================
static void drawInputBox(Adafruit_ST7735 &tft) {
    // Clear entire input area
    tft.fillRect(0, INP_Y, 128, INP_H, COL_BG);

    // Magenta border
    tft.drawRect(1, INP_Y + 1, 126, INP_H - 2, COL_INP_BDR);

    tft.setFont(NULL);
    tft.setTextSize(1);

    // Placeholder when input is empty and cursor hidden
    if (inputText.length() == 0 && !cursorVisible) {
        tft.setTextColor(COL_INP_PHLD, COL_BG);
        tft.setCursor(4, INP_Y + 4);
        tft.print("Type question...");
        return;
    }

    // Build display string: typed text + optional blinking cursor
    String display = inputText;
    if (cursorVisible) display += '_';

    int totalLen = display.length();

    // Scroll to end: show last (INP_CHARS Ã— INP_LINES) characters
    int maxVisible = INP_CHARS * INP_LINES;  // 60 chars
    int startIdx   = (totalLen > maxVisible) ? (totalLen - maxVisible) : 0;

    int16_t baseX = 4;                    // Just inside left border
    int16_t baseY = INP_Y + 4;            // Just below top border
    int     line  = 0, col = 0;

    for (int i = startIdx; i < totalLen; i++) {
        char    c  = display[i];
        int16_t px = baseX + col * 6;
        int16_t py = baseY + line * 9;

        if (py + 8 > INP_Y + INP_H - 1) break; // Outside box

        tft.setTextColor(
            (c == '_' && i == totalLen - 1) ? COL_CURSOR : COL_INP_TXT,
            COL_BG
        );
        tft.setCursor(px, py);
        tft.print(c);

        col++;
        if (col >= INP_CHARS) { col = 0; line++; }
    }
}

// ============================================================
//  HELPER â€” clearKbArea
// ============================================================
static void clearKbArea(Adafruit_ST7735 &tft) {
    tft.fillRect(0, KB_Y, 128, 160 - KB_Y, COL_BG);
}

// ============================================================
//  HELPER â€” drawThinkingScreen
// ============================================================
static void drawThinkingScreen(Adafruit_ST7735 &tft) {
    clearKbArea(tft);

    tft.setFont(NULL);
    tft.setTextSize(1);

    // Short question preview
    tft.setTextColor(COL_INP_PHLD, COL_BG);
    tft.setCursor(3, KB_Y + 6);
    String qPreview = "Q: " + inputText;
    if (qPreview.length() > 20) qPreview = qPreview.substring(0, 19) + "~";
    tft.print(qPreview);

    // "Thinkingâ€¦" centred
    const char *msg  = "Thinking...";
    int16_t     msgW = (int16_t)(strlen(msg) * 6);
    tft.setTextColor(COL_THINK_TXT, COL_BG);
    tft.setCursor((128 - msgW) / 2, KB_Y + 50);
    tft.print(msg);
}

// ============================================================
//  HELPER â€” drawResponseBody
//  Word-wraps the response text across the keyboard area.
// ============================================================
static void drawResponseBody(Adafruit_ST7735 &tft, const String &text) {
    clearKbArea(tft);

    tft.setFont(NULL);
    tft.setTextSize(1);

    int16_t x     = 3;
    int16_t y     = KB_Y + 2;
    int16_t lineH = 10;
    int16_t maxY  = 150;  // Stop here, leave room for hint

    char lineBuf[22];
    int  lineLen   = 0;
    bool overflow  = false;

    // Inline flush helper
    auto flush = [&]() {
        if (lineLen == 0) return;
        lineBuf[lineLen] = '\0';
        tft.setTextColor(COL_RESP_TXT, COL_BG);
        tft.setCursor(x, y);
        tft.print(lineBuf);
        y += lineH;
        lineLen = 0;
        if (y + 8 > maxY) overflow = true;
    };

    const char *src = text.c_str();
    while (*src && !overflow) {
        if (*src == '\n') {
            flush();
            src++;
            continue;
        }
        // Skip leading spaces at the start of a line
        if (*src == ' ' && lineLen == 0) { src++; continue; }

        // Collect one word
        const char *ws = src;
        while (*src && *src != ' ' && *src != '\n') src++;
        int wlen = (int)(src - ws);

        int needed = lineLen + (lineLen ? 1 : 0) + wlen;
        if (needed <= 20) {
            if (lineLen) lineBuf[lineLen++] = ' ';
            memcpy(lineBuf + lineLen, ws, wlen);
            lineLen += wlen;
        } else {
            flush();
            if (overflow) break;
            // Long word: chunk across lines
            while (wlen > 0 && !overflow) {
                int chunk = (wlen <= 20) ? wlen : 20;
                memcpy(lineBuf, ws, chunk);
                lineLen = chunk;
                ws     += chunk;
                wlen   -= chunk;
                if (wlen > 0) flush();
            }
        }
    }
    flush(); // Flush any remainder

    // Bottom hint
    tft.setTextColor(COL_HINT_TXT, COL_BG);
    tft.setCursor(2, 152);
    tft.print("BACK: new question");
}

// ============================================================
//  HELPER â€” processJoystick
//  Reads ADC, applies dead-zone + initial-delay + repeat-rate,
//  and forwards movement to AIKeyboard::navigate().
// ============================================================
static void processJoystick(Adafruit_ST7735 &tft) {
    int vrx = analogRead(JOY_X_PIN);
    int vry = analogRead(JOY_Y_PIN);

    bool centered = (vrx > JOY_DEAD_LO && vrx < JOY_DEAD_HI &&
                     vry > JOY_DEAD_LO && vry < JOY_DEAD_HI);

    if (centered) {
        joyCentered = true;
        joyMoving   = false;
        return;
    }

    // Decode direction from ADC value
    int dx = 0, dy = 0;
    if      (vrx < JOY_THRESH_LO) dx = -1;
    else if (vrx > JOY_THRESH_HI) dx =  1;
    if      (vry < JOY_THRESH_LO) dy = -1;
    else if (vry > JOY_THRESH_HI) dy =  1;

    unsigned long now = millis();

    if (joyCentered) {
        // First movement out of dead-zone: act immediately
        joyCentered  = false;
        joyMoving    = true;
        joyMoveStart = now;
        lastRepeat   = now;
        lastDx = dx;  lastDy = dy;
        AIKeyboard::navigate(dx, dy);
        AIKeyboard::render(tft);           // Dirty-only (2 keys repainted)

    } else if (joyMoving) {
        // Auto-repeat after initial delay
        if ((now - joyMoveStart) > (unsigned long)JOY_INIT_DELAY) {
            if ((now - lastRepeat) >= (unsigned long)JOY_REPEAT_RATE) {
                lastRepeat = now;
                AIKeyboard::navigate(lastDx, lastDy);
                AIKeyboard::render(tft);
            }
        }
    }
}

// ============================================================
//  STATE: CHAT_KEYBOARD
// ============================================================
static void updateKeyboard(Adafruit_ST7735 &tft) {

    // â”€â”€ 1. Cursor blink (non-blocking) â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    unsigned long now = millis();
    if (now - lastBlink >= CURSOR_BLINK_MS) {
        lastBlink     = now;
        cursorVisible = !cursorVisible;
        inputDirty    = true;
    }
    if (inputDirty) {
        drawInputBox(tft);
        inputDirty = false;
    }

    // â”€â”€ 2. Joystick navigation â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    processJoystick(tft);

    // â”€â”€ 3. Physical ENTER button â†’ select current key â”€â”€â”€â”€â”€â”€â”€â”€â”€
    if (isEnterPressed()) {
        char k = AIKeyboard::selectCurrentKey();

        if (k == KEY_SHIFT) {
            // Shift toggled: all alpha labels change â†’ full redraw
            AIKeyboard::render(tft, true);

        } else if (k == KEY_BACKSPACE) {
            if (inputText.length() > 0) {
                inputText.remove(inputText.length() - 1);
                inputDirty = true;
            }

        } else if (k == KEY_ENTER) {
            // Submit question (only if non-empty)
            if (inputText.length() > 0) {
                aiState = CHAT_THINKING;
                clearButtonEvents();
            }

        } else if (k == KEY_SPACE) {
            if ((int)inputText.length() < MAX_INPUT_LEN) {
                inputText += ' ';
                inputDirty = true;
            }

        } else if (k != KEY_NONE) {
            if ((int)inputText.length() < MAX_INPUT_LEN) {
                inputText += k;
                inputDirty = true;
            }
        }
    }

    // â”€â”€ 4. BACK button â†’ signal main.cpp to exit to home â”€â”€â”€â”€â”€
    //   We consume the event here so main.cpp's own guard can't
    //   race with us; we set the _wantsExit flag instead.
    if (isBackPressed()) {
        _wantsExit = true;
    }
}

// ============================================================
//  STATE: CHAT_THINKING
// ============================================================
static void updateThinking(Adafruit_ST7735 &tft) {
    // Draw "Thinking..." screen once
    drawThinkingScreen(tft);

    // Blocking HTTP call to Flask â†’ Gemini (uses existing AIConnection)
    responseText = AIConnection::postAsk(inputText);

    // Render response and transition
    aiState = CHAT_RESPONSE;
    drawResponseBody(tft, responseText);
    clearButtonEvents();
}

// ============================================================
//  STATE: CHAT_RESPONSE
// ============================================================
static void updateResponse(Adafruit_ST7735 &tft) {
    // BACK or ENTER â†’ return to keyboard for a new question
    if (isBackPressed() || isEnterPressed()) {
        // Consume the event so main.cpp's isBackPressed() guard is not triggered
        inputText     = "";
        aiState       = CHAT_KEYBOARD;
        cursorVisible = true;
        lastBlink     = millis();
        joyCentered   = true;
        joyMoving     = false;
        inputDirty    = true;
        clearButtonEvents();

        // Full screen redraw
        tft.fillScreen(COL_BG);
        drawHeader(tft);
        AIKeyboard::init();
        drawInputBox(tft);
        AIKeyboard::render(tft, true);
    }
}

// ============================================================
//  PUBLIC â€” AIApp::init
// ============================================================
void AIApp::init(Adafruit_ST7735 &tft) {
    // Reset all state
    aiState       = CHAT_KEYBOARD;
    inputText     = "";
    responseText  = "";
    cursorVisible = true;
    lastBlink     = millis();
    joyCentered   = true;
    joyMoving     = false;
    inputDirty    = true;
    _wantsExit    = false;
    clearButtonEvents();

    // Full screen render
    tft.fillScreen(COL_BG);
    drawHeader(tft);
    AIKeyboard::init();
    drawInputBox(tft);
    AIKeyboard::render(tft, true);  // Force-draw all 49 keys
}

// ============================================================
//  PUBLIC â€” AIApp::shouldExit
// ============================================================
bool AIApp::shouldExit() {
    if (_wantsExit) {
        _wantsExit = false;
        return true;
    }
    return false;
}

// ============================================================
//  PUBLIC â€” AIApp::update
//  Called every loop() while STATE_AI is active.
// ============================================================
void AIApp::update(Adafruit_ST7735 &tft) {
    switch (aiState) {
        case CHAT_KEYBOARD: updateKeyboard(tft); break;
        case CHAT_THINKING: updateThinking(tft); break;
        case CHAT_RESPONSE: updateResponse(tft); break;
    }
}

