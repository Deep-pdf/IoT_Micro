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
//  HELPER — drawHeader
// ============================================================
static void drawHeader(Adafruit_ST7735 &tft) {
    tft.fillRect(0, 0, 128, HDR_H, COL_HDR_BG);

    // "GEMINI AI" in default clean font (size 1) - highly visible & crisp, not bold
    tft.setFont(NULL);
    tft.setTextSize(1);
    tft.setTextColor(COL_HDR_TXT, COL_HDR_BG);

    const char *title = "GEMINI AI";
    int titleW = (int)strlen(title) * 6;  // 9 chars * 6 px = 54 px
    int16_t titleX = (128 - titleW) / 2;  // 37 px

    tft.setCursor(titleX, 3);
    tft.print(title);

    // Small accent indicators
    tft.setTextColor(COL_HDR_ACC, COL_HDR_BG);
    tft.setCursor(2,   3);  tft.print(">");   // left robot indicator
    tft.setCursor(118, 3);  tft.print("~");   // right Wi-Fi indicator
}

// ============================================================
//  HELPER — drawInputBox
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

    // Scroll to end: show last (INP_CHARS × INP_LINES) characters
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
//  HELPER — clearKbArea
// ============================================================
static void clearKbArea(Adafruit_ST7735 &tft) {
    tft.fillRect(0, KB_Y, 128, 160 - KB_Y, COL_BG);
}

// ============================================================
//  HELPER — drawThinkingScreen
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

    // "Thinking…" centred
    const char *msg  = "Thinking...";
    int16_t     msgW = (int16_t)(strlen(msg) * 6);
    tft.setTextColor(COL_THINK_TXT, COL_BG);
    tft.setCursor((128 - msgW) / 2, KB_Y + 50);
    tft.print(msg);
}

// Pagination and response text scrolling
static const int MAX_Q_LINES    = 6;
static const int MAX_RESP_LINES = 80;

static String qLines[MAX_Q_LINES];
static int    qLineCount = 0;

static String respLines[MAX_RESP_LINES];
static int    totalRespLines   = 0;
static int    respScrollRow    = 0;
static int    maxRespScroll    = 0;
static int    visibleRespLines = 0;
static int16_t respStartY       = 0;
static unsigned long lastScrollTime = 0;

// Cleans up string: converts single '\n' or '\r' to ' ', preserves double '\n\n' as paragraph break '\n'
static String normalizeResponseText(const String &raw) {
    String out = "";
    out.reserve(raw.length());

    size_t len = raw.length();
    for (size_t i = 0; i < len; i++) {
        char c = raw[i];
        if (c == '\r') continue;

        if (c == '\n') {
            if (i + 1 < len && (raw[i + 1] == '\n' || raw[i + 1] == '\r')) {
                out += '\n';
                while (i + 1 < len && (raw[i + 1] == '\n' || raw[i + 1] == '\r')) {
                    i++;
                }
            } else {
                if (out.length() > 0 && out[out.length() - 1] != ' ') {
                    out += ' ';
                }
            }
        } else {
            out += c;
        }
    }
    return out;
}

static void formatTextIntoLines(const String &text, int maxChars, String lines[], int &lineCount, int maxLines) {
    lineCount = 0;
    if (text.length() == 0) return;

    String currentLine = "";

    const char *p = text.c_str();
    while (*p && lineCount < maxLines) {
        if (*p == '\n') {
            lines[lineCount++] = currentLine;
            currentLine = "";
            p++;
            continue;
        }

        if (*p == ' ' && currentLine.length() == 0) {
            p++;
            continue;
        }

        const char *wStart = p;
        while (*p && *p != ' ' && *p != '\n') p++;
        int wLen = (int)(p - wStart);

        String word = "";
        for (int i = 0; i < wLen; i++) word += wStart[i];

        int spaceNeeded = (currentLine.length() > 0 ? 1 : 0) + wLen;
        if ((int)currentLine.length() + spaceNeeded <= maxChars) {
            if (currentLine.length() > 0) currentLine += ' ';
            currentLine += word;
        } else {
            if (currentLine.length() > 0) {
                lines[lineCount++] = currentLine;
                currentLine = "";
            }
            if (lineCount >= maxLines) break;

            while ((int)word.length() > maxChars && lineCount < maxLines) {
                lines[lineCount++] = word.substring(0, maxChars);
                word = word.substring(maxChars);
            }
            if (word.length() > 0 && lineCount < maxLines) {
                currentLine = word;
            }
        }
    }

    if (currentLine.length() > 0 && lineCount < maxLines) {
        lines[lineCount++] = currentLine;
    }
}

static void renderResponseTextOnly(Adafruit_ST7735 &tft) {
    // Clear only response body area (respStartY to 148)
    tft.fillRect(0, respStartY, 128, 149 - respStartY, COL_BG);

    tft.setFont(NULL);
    tft.setTextSize(1);
    tft.setTextColor(COL_RESP_TXT, COL_BG);

    int16_t currY = respStartY;
    for (int i = 0; i < visibleRespLines; i++) {
        int lineIdx = respScrollRow + i;
        if (lineIdx >= totalRespLines) break;

        tft.setCursor(4, currY);
        tft.print(respLines[lineIdx]);
        currY += 10;
    }
}

static void drawResponseScreen(Adafruit_ST7735 &tft, const String &text) {
    // 1. Clear full body display area below header (Y=14 to 160)
    tft.fillRect(0, 14, 128, 146, COL_BG);

    // 2. Format Question into lines (shows full question wrapped)
    String qFull = "Q: " + inputText;
    formatTextIntoLines(qFull, 20, qLines, qLineCount, MAX_Q_LINES);

    // Render Question Banner
    int qHeight = qLineCount * 9 + 4;
    tft.fillRect(0, 14, 128, qHeight, 0x0826); // Dark navy-gray banner
    tft.setFont(NULL);
    tft.setTextSize(1);
    tft.setTextColor(0x07FF, 0x0826);          // Cyan text on dark banner

    for (int i = 0; i < qLineCount; i++) {
        tft.setCursor(4, 16 + i * 9);
        tft.print(qLines[i]);
    }

    // Divider line
    tft.drawFastHLine(0, 14 + qHeight, 128, 0x07FF);

    // 3. Format Response Text into lines
    String normalized = normalizeResponseText(text);
    formatTextIntoLines(normalized, 20, respLines, totalRespLines, MAX_RESP_LINES);

    // Calculate response vertical geometry
    respStartY = 14 + qHeight + 3;
    int availH = 148 - respStartY;
    visibleRespLines = availH / 10;
    if (visibleRespLines < 1) visibleRespLines = 1;

    respScrollRow = 0;
    if (totalRespLines > visibleRespLines) {
        maxRespScroll = totalRespLines - visibleRespLines;
    } else {
        maxRespScroll = 0;
    }

    // 4. Render initial response text slice
    renderResponseTextOnly(tft);

    // 5. Render Bottom Hint & Scroll Indicator
    tft.fillRect(0, 149, 128, 11, COL_BG);
    tft.setFont(NULL);
    tft.setTextSize(1);
    tft.setTextColor(COL_HINT_TXT, COL_BG);
    tft.setCursor(4, 151);

    if (maxRespScroll > 0) {
        tft.print("BACK:exit  ^v:scroll");
    } else {
        tft.print("BACK: new question");
    }
}

// ============================================================
//  HELPER — processJoystick
//  Reads ADC, applies dead-zone + initial-delay + repeat-rate,
//  and forwards movement to AIKeyboard::navigate().
// ============================================================
static void processJoystick(Adafruit_ST7735 &tft) {
    int vrx = analogRead(JOY_X_PIN);
    int vry = analogRead(JOY_Y_PIN);

    // Calculate displacement relative to dead-zone thresholds (1200..2800)
    int diffX = 0;
    if (vrx < 1200)      diffX = vrx - 1200;      // negative -> left
    else if (vrx > 2800) diffX = vrx - 2800;      // positive -> right

    int diffY = 0;
    if (vry < 1200)      diffY = vry - 1200;      // negative -> up
    else if (vry > 2800) diffY = vry - 2800;      // positive -> down

    if (diffX == 0 && diffY == 0) {
        joyCentered = true;
        joyMoving   = false;
        return;
    }

    // Pick dominant axis
    int dx = 0, dy = 0;
    if (abs(diffX) >= abs(diffY)) {
        dx = (diffX < 0) ? -1 : 1;
    } else {
        dy = (diffY < 0) ? -1 : 1;
    }

    unsigned long now = millis();

    if (joyCentered) {
        // First movement out of dead-zone: act immediately
        joyCentered  = false;
        joyMoving    = true;
        joyMoveStart = now;
        lastRepeat   = now;
        lastDx       = dx;
        lastDy       = dy;
        AIKeyboard::navigate(dx, dy);
        AIKeyboard::render(tft);           // Dirty-only (2 keys repainted)

    } else if (joyMoving) {
        // Auto-repeat after initial delay
        if ((now - joyMoveStart) > (unsigned long)JOY_INIT_DELAY) {
            if ((now - lastRepeat) >= (unsigned long)JOY_REPEAT_RATE) {
                lastRepeat = now;
                lastDx     = dx;
                lastDy     = dy;
                AIKeyboard::navigate(dx, dy);
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

    // â”€â”€ 4. Physical BACK button â†’ delete recent character ──────
    if (isBackPressed()) {
        if (inputText.length() > 0) {
            inputText.remove(inputText.length() - 1);
            inputDirty = true;
        }
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
    drawResponseScreen(tft, responseText);
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
    // 5-second long press on BACK button exits to Home screen
    if (isBackLongPressed()) {
        _wantsExit = true;
        clearButtonEvents();
        return;
    }

    switch (aiState) {
        case CHAT_KEYBOARD: updateKeyboard(tft); break;
        case CHAT_THINKING: updateThinking(tft); break;
        case CHAT_RESPONSE: updateResponse(tft); break;
    }
}

