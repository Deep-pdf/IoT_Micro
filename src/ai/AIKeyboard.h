#ifndef AI_KEYBOARD_H
#define AI_KEYBOARD_H

#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>

// ============================================================
//  SPECIAL KEY SENTINELS
//  Non-printable ASCII values that can never appear in typed text.
// ============================================================
#define KEY_NONE       '\0'
#define KEY_SHIFT      '\x01'
#define KEY_BACKSPACE  '\x08'   // '\b'
#define KEY_ENTER      '\x0A'   // '\n'
#define KEY_SPACE      ' '

// ============================================================
//  AIKeyboard
//  Owns the on-screen keyboard: grid layout, cursor selection,
//  joystick navigation, and TFT rendering.
//  All methods are static — no heap allocation required.
// ============================================================
class AIKeyboard {
public:
    // Initialise: reset selection to row 0 / col 0 ('a'), shift off.
    static void init();

    // Render keyboard to TFT.
    //   forceRedraw = true  : repaint every key (use on init / shift toggle)
    //   forceRedraw = false : dirty-only — repaint only the key that changed
    static void render(Adafruit_ST7735 &tft, bool forceRedraw = false);

    // Move cursor by (dx, dy).  +x = right, +y = down.
    // Cross-row navigation lands on the key whose X-centre is nearest
    // to the current key's X-centre (no jumping over empty spaces).
    static void navigate(int dx, int dy);

    // Return the effective char of the currently highlighted key.
    //   Shift key  : toggles shift state, returns KEY_SHIFT.
    //                Caller MUST call render(tft, true) to refresh all labels.
    //   Alpha keys : returns upper/lower based on current shift state.
    //   Others     : returns the raw char value (digit, symbol, BSP, ENTER…).
    static char selectCurrentKey();

    // True if SHIFT is currently active.
    static bool isShiftOn();

private:
    static int  _row;
    static int  _col;
    static int  _prevRow;
    static int  _prevCol;
    static bool _shiftOn;

    static int      rowKeyCount(int row);
    static char     keyBaseValue(int row, int col);
    static void     getKeyRect(int row, int col,
                               int16_t &x, int16_t &y,
                               int16_t &w, int16_t &h);
    static uint16_t keyFillColor(int row, int col);
    static uint16_t keyTextColor(int row, int col);
    static void     keyLabel(int row, int col, char *buf, int bufLen);
    static void     drawKey(Adafruit_ST7735 &tft, int row, int col, bool selected);
};

#endif // AI_KEYBOARD_H
