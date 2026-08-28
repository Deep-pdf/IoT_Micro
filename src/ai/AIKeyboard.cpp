#include "AIKeyboard.h"
#include <Arduino.h>
#include <string.h>

// ============================================================
//  PIXEL LAYOUT  (128 × 160 portrait display)
// ============================================================
//
//  Y=  0..13   Header    (14 px)
//  Y= 14..41   Input box (28 px)
//  Y= 42..     Keyboard  (118 px, rows below)
//
//  Row │ Content              │ Keys │ Y-start │ H
//  ────┼──────────────────────┼──────┼─────────┼────
//   0  │ a  b  c  d  e  f  g  h  i  │  9  │  43  │ 18
//   1  │ j  k  l  m  n  o  p  q  r  │  9  │  62  │ 18
//   2  │ s  t  u  v  w  x  y  z  <  │  9  │  81  │ 18
//   3  │ 1  2  3  4  5  6  7  8  9  0  │ 10 │ 100 │ 16
//   4  │ .  ,  ?  !  -  '  @  #     │  8  │ 117  │ 16
//   5  │ [SHF]  [   SPC   ] [DEL] [ENT] │ 4 │ 134 │ 22
//  ────┴──────────────────────┴──────┴─────────┴────
//  Bottom of row 5: 134+22=156  (4 px bottom safety margin)
//
// ============================================================

// ── Row geometry ─────────────────────────────────────────────
static const int16_t ROW_Y[6] = { 43,  62,  81, 100, 117, 134 };
static const int16_t ROW_H[6] = { 18,  18,  18,  16,  16,  22  };

// Alpha rows 0-2:  9 keys, W=13, gap=1, left-start x=2
//   9×13 + 8×1 = 125 px, (128-125)/2 ≈ 1.5 → use 2
#define ALPHA_KW  13
#define ALPHA_X0   2
#define ALPHA_GAP  1

// Number row 3:  10 keys, W=11, gap=1, left-start x=4
//   10×11 + 9×1 = 119 px, (128-119)/2 = 4.5 → use 4
#define NUM_KW    11
#define NUM_X0     4
#define NUM_GAP    1

// Symbol row 4:  8 keys, W=14, gap=1, left-start x=4
//   8×14 + 7×1 = 119 px, (128-119)/2 = 4.5 → use 4
#define SYM_KW    14
#define SYM_X0     4
#define SYM_GAP    1

// Function row 5:  custom widths and X positions
//   1 + 20 + 1 + 50 + 1 + 20 + 1 + 33 + 1 = 128  ✓
//   col  0=SHIFT  1=SPACE  2=BSP  3=ENTER
static const int16_t FUNC_X[4] = {  1,  22,  73,  94 };
static const int16_t FUNC_W[4] = { 20,  50,  20,  33 };

// ── Colour palette (RGB-565) ──────────────────────────────────
// Alpha keys    – light gray
#define COL_ALPHA      0xC618
// Number keys   – cyan
#define COL_NUM        0x07FF
// Symbol keys   – lime/green
#define COL_SYM        0x2FE0
// SHIFT (off)   – yellow
#define COL_SHIFT_OFF  0xFFE0
// SHIFT (on)    – orange-red  (clearly indicates shift is active)
#define COL_SHIFT_ON   0xFD20
// SPACE         – purple
#define COL_SPACE      0x801F
// BACKSPACE     – orange
#define COL_BSP        0xFB60
// ENTER         – green
#define COL_ENTER      0x07E0

// Key text
#define COL_TXT_DK     0x0000   // Black on light keys
#define COL_TXT_LT     0xFFFF   // White on dark keys

// Key borders
#define COL_BORDER_N   0x4208   // Dark gray  (normal)
#define COL_BORDER_S   0xFFFF   // White      (selected, double-drawn)

// ── Character tables (file-scope for clean switch-case) ───────
static const char SYMS[8]  = { '.', ',', '?', '!', '-', '\'', '@', '#' };
static const char FUNCS[4] = { KEY_SHIFT, KEY_SPACE, KEY_BACKSPACE, KEY_ENTER };

// ============================================================
//  STATIC MEMBER DEFINITIONS
// ============================================================
int  AIKeyboard::_row     = 0;
int  AIKeyboard::_col     = 0;
int  AIKeyboard::_prevRow = -1;
int  AIKeyboard::_prevCol = -1;
bool AIKeyboard::_shiftOn = false;

// ============================================================
//  rowKeyCount
// ============================================================
int AIKeyboard::rowKeyCount(int row) {
    switch (row) {
        case 0: case 1: case 2: return 9;
        case 3:                 return 10;
        case 4:                 return 8;
        case 5:                 return 4;
        default:                return 0;
    }
}

// ============================================================
//  keyBaseValue  – raw char value, ignoring shift state
// ============================================================
char AIKeyboard::keyBaseValue(int row, int col) {
    switch (row) {
        case 0: return (char)('a' + col);                          // a–i
        case 1: return (char)('j' + col);                          // j–r
        case 2: return (col < 8) ? (char)('s' + col)               // s–z
                                 : KEY_BACKSPACE;
        case 3: return (col < 9) ? (char)('1' + col) : '0';        // 1–0
        case 4: return SYMS[col];
        case 5: return FUNCS[col];
        default: return KEY_NONE;
    }
}

// ============================================================
//  getKeyRect
// ============================================================
void AIKeyboard::getKeyRect(int row, int col,
                             int16_t &x, int16_t &y,
                             int16_t &w, int16_t &h) {
    y = ROW_Y[row];
    h = ROW_H[row];

    if (row <= 2) {
        x = ALPHA_X0 + (int16_t)col * (ALPHA_KW + ALPHA_GAP);
        w = ALPHA_KW;
    } else if (row == 3) {
        x = NUM_X0 + (int16_t)col * (NUM_KW + NUM_GAP);
        w = NUM_KW;
    } else if (row == 4) {
        x = SYM_X0 + (int16_t)col * (SYM_KW + SYM_GAP);
        w = SYM_KW;
    } else { // row == 5
        x = FUNC_X[col];
        w = FUNC_W[col];
    }
}

// ============================================================
//  keyFillColor
// ============================================================
uint16_t AIKeyboard::keyFillColor(int row, int col) {
    if (row <= 2) return COL_ALPHA;
    if (row == 3) return COL_NUM;
    if (row == 4) return COL_SYM;
    // row 5
    switch (col) {
        case 0: return _shiftOn ? COL_SHIFT_ON : COL_SHIFT_OFF;
        case 1: return COL_SPACE;
        case 2: return COL_BSP;
        case 3: return COL_ENTER;
    }
    return COL_ALPHA;
}

// ============================================================
//  keyTextColor
// ============================================================
uint16_t AIKeyboard::keyTextColor(int row, int col) {
    // SPACE (row5 col1) and ENTER (row5 col3) are dark keys → white text
    if (row == 5 && (col == 1 || col == 3)) return COL_TXT_LT;
    return COL_TXT_DK;
}

// ============================================================
//  keyLabel  – writes the display string for a key into buf
// ============================================================
void AIKeyboard::keyLabel(int row, int col, char *buf, int bufLen) {
    char v = keyBaseValue(row, col);

    if (v == KEY_BACKSPACE) {
        // Row-2 BSP is narrow (13 px) → single char; row-5 BSP is wider
        strncpy(buf, (row == 2) ? "<" : "DEL", bufLen);
    } else if (v == KEY_ENTER) {
        strncpy(buf, "ENT", bufLen);
    } else if (v == KEY_SPACE) {
        strncpy(buf, "SPC", bufLen);
    } else if (v == KEY_SHIFT) {
        // "^" always; colour change shows active state
        strncpy(buf, "^", bufLen);
    } else if (v >= 'a' && v <= 'z') {
        // Apply shift for display
        buf[0] = _shiftOn ? (char)(v - 'a' + 'A') : v;
        buf[1] = '\0';
    } else {
        buf[0] = v;
        buf[1] = '\0';
    }

    buf[bufLen - 1] = '\0'; // guarantee null-termination
}

// ============================================================
//  drawKey  (internal)
// ============================================================
void AIKeyboard::drawKey(Adafruit_ST7735 &tft, int row, int col, bool selected) {
    int16_t x, y, w, h;
    getKeyRect(row, col, x, y, w, h);

    uint16_t fill = keyFillColor(row, col);
    uint16_t txt  = keyTextColor(row, col);

    // 1. Fill background
    tft.fillRect(x, y, w, h, fill);

    // 2. Border  (double-ring when selected for high contrast)
    if (selected) {
        tft.drawRect(x,     y,     w,     h,     COL_BORDER_S);
        if (w > 4 && h > 4) {
            tft.drawRect(x + 1, y + 1, w - 2, h - 2, COL_BORDER_S);
        }
    } else {
        tft.drawRect(x, y, w, h, COL_BORDER_N);
    }

    // 3. Label (default font, size 1 = 6×8 px per char, sharp and pixel-perfect)
    char buf[8];
    keyLabel(row, col, buf, sizeof(buf));

    tft.setFont(NULL);
    tft.setTextSize(1);
    tft.setTextColor(txt, fill); // bg=fill → no ghost pixels

    int     len = (int)strlen(buf);
    int16_t tx  = x + (w - len * 6) / 2;
    int16_t ty  = y + (h - 8)       / 2;
    if (tx < x + 1) tx = x + 1; // clamp inside border

    tft.setCursor(tx, ty);
    tft.print(buf);
}

// ============================================================
//  PUBLIC — init
// ============================================================
void AIKeyboard::init() {
    _row     = 0;
    _col     = 0;
    _prevRow = -1;
    _prevCol = -1;
    _shiftOn = false;
}

// ============================================================
//  PUBLIC — render
// ============================================================
void AIKeyboard::render(Adafruit_ST7735 &tft, bool forceRedraw) {
    if (forceRedraw) {
        // Repaint every key
        for (int r = 0; r < 6; r++) {
            for (int c = 0; c < rowKeyCount(r); c++) {
                drawKey(tft, r, c, (r == _row && c == _col));
            }
        }
        _prevRow = _row;
        _prevCol = _col;
    } else {
        // Dirty-only: only repaint old (deselect) + new (select)
        bool moved = (_row != _prevRow || _col != _prevCol);
        if (moved) {
            if (_prevRow >= 0 && _prevCol >= 0) {
                drawKey(tft, _prevRow, _prevCol, false);  // deselect old
            }
            drawKey(tft, _row, _col, true);               // select new
            _prevRow = _row;
            _prevCol = _col;
        }
    }
}

// ============================================================
//  PUBLIC — navigate
// ============================================================
void AIKeyboard::navigate(int dx, int dy) {
    int newRow = _row;
    int newCol = _col;

    if (dy != 0) {
        // ── Vertical: move to adjacent row ───────────────────
        newRow = _row + dy;
        if (newRow < 0) newRow = 0;
        if (newRow > 5) newRow = 5;

        if (newRow != _row) {
            // Proportional: land on key whose pixel-centre X is nearest
            // to the current key's pixel-centre X.
            int16_t cx, cy, cw, ch;
            getKeyRect(_row, _col, cx, cy, cw, ch);
            int curCX = (int)cx + (int)cw / 2;

            int bestCol  = 0;
            int bestDist = 32767;
            for (int c = 0; c < rowKeyCount(newRow); c++) {
                int16_t nx, ny, nw, nh;
                getKeyRect(newRow, c, nx, ny, nw, nh);
                int dist = abs((int)nx + (int)nw / 2 - curCX);
                if (dist < bestDist) { bestDist = dist; bestCol = c; }
            }
            newCol = bestCol;
        }

    } else if (dx != 0) {
        // ── Horizontal: move within the same row ─────────────
        int maxCol = rowKeyCount(_row) - 1;
        newCol = _col + dx;
        if (newCol < 0)      newCol = 0;
        if (newCol > maxCol) newCol = maxCol;
    }

    _row = newRow;
    _col = newCol;
}

// ============================================================
//  PUBLIC — selectCurrentKey
// ============================================================
char AIKeyboard::selectCurrentKey() {
    char v = keyBaseValue(_row, _col);

    if (v == KEY_SHIFT) {
        _shiftOn = !_shiftOn;
        return KEY_SHIFT;   // Caller must call render(tft, true)
    }

    // Apply shift to alpha characters
    if (_shiftOn && v >= 'a' && v <= 'z') {
        return (char)(v - 'a' + 'A');
    }

    return v;
}

// ============================================================
//  PUBLIC — isShiftOn
// ============================================================
bool AIKeyboard::isShiftOn() {
    return _shiftOn;
}
