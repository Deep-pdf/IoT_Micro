#include "focus_timer/FocusTimerApp.h"
#include "focus_timer/shinchan_frames.h"
#include "focus_timer/TimerFontCondensed.h"
#include "button.h"
#include "joystick.h"
#include "esp32/rom/tjpgd.h"

#define PIN_BTN_A    13   // GPIO 13: Enter / A
#define PIN_BTN_BACK 25   // GPIO 25: Back

// Vertical position of the floating timer digits (open upper sky above Shinchan)
#define FLOATING_TIMER_Y 28

// Animation state
static int s_currentFrame = 0;              // 0 to 29 (frames 1 to 30)
static unsigned long s_lastFrameMs = 0;     // Timestamp of last frame transition
static bool s_wantsExit = false;            // Set true on Back button press / hold >= 2s
static bool s_active = false;               // Active playback flag

// Timer state machine
static FocusTimerState s_timerState = TIMER_STATE_SELECTION;
static TimerField s_selectedField = FIELD_HOURS;
static uint8_t s_selectedHours = 0;         // 00 to 99
static uint8_t s_selectedMinutes = 0;       // 00 to 59
static uint8_t s_selectedSeconds = 0;       // 00 to 59
static uint32_t s_remainingSeconds = 0;     // Remaining countdown seconds
static unsigned long s_lastTickMs = 0;      // Millis timestamp for 1-second countdown ticks

// Button debouncing & hold detection
static int s_rawBtnA = HIGH;
static int s_stableBtnA = HIGH;
static unsigned long s_debounceBtnA = 0;
static unsigned long s_btnAHoldStart = 0;
static bool s_btnAHeld = false;
static bool s_btnALongHandled = false;

static int s_rawBtnBack = HIGH;
static int s_stableBtnBack = HIGH;
static unsigned long s_debounceBtnBack = 0;
static unsigned long s_btnBackHoldStart = 0;
static bool s_btnBackHeld = false;
static bool s_btnBackLongHandled = false;

// Shared TJpgDec workspace pool (ChaN's TJpgDec requires ~3100 bytes)
static uint8_t s_tjpgd_work[3100];

// In-memory JPEG stream reader
struct JpgStream {
    const uint8_t *data;
    size_t len;
    size_t offset;
};

// Input callback function for TJpgDec
static UINT jpgInputFunc(JDEC* jdec, BYTE* buf, UINT len) {
    JpgStream* stream = (JpgStream*)jdec->device;
    if (stream->offset + len > stream->len) {
        len = (UINT)(stream->len - stream->offset);
    }
    if (buf && len > 0) {
        memcpy(buf, stream->data + stream->offset, len);
    }
    stream->offset += len;
    return len;
}

// Rendering context passed to the output callback
struct JpgRenderContext {
    Adafruit_ST7735 *tft;
};

static JpgRenderContext s_renderCtx;

// Output callback function for TJpgDec (called per MCU block)
static UINT jpgOutputFunc(JDEC* jdec, void* bitmap, JRECT* rect) {
    JpgRenderContext* ctx = &s_renderCtx;
    if (!ctx->tft) return 1;

    // Safety guard
    if (rect->right < 0 || rect->left >= 128 || rect->bottom < 0 || rect->top >= 160) {
        return 1;
    }

    int16_t w = rect->right - rect->left + 1;
    int16_t h = rect->bottom - rect->top + 1;

    // Maximum MCU block size is 16x16 = 256 pixels
    uint16_t block_buf[256];
    int count = 0;

    uint8_t *src_bytes = (uint8_t*)bitmap;
    for (int16_t by = 0; by < h; by++) {
        int src_row_offset = by * w * 3;
        for (int16_t bx = 0; bx < w; bx++) {
            int src_idx = src_row_offset + bx * 3;
            uint8_t r = src_bytes[src_idx];
            uint8_t g = src_bytes[src_idx + 1];
            uint8_t b = src_bytes[src_idx + 2];
            // Fast RGB888 to RGB565 conversion
            block_buf[count++] = ((uint16_t)(r & 0xF8) << 8) | ((uint16_t)(g & 0xFC) << 3) | (b >> 3);
        }
    }

    if (count > 0) {
        ctx->tft->drawRGBBitmap(rect->left, rect->top, block_buf, w, h);
    }

    return 1;
}

// Forward declaration
static void drawFloatingTimer(Adafruit_ST7735 &tft);

void FocusTimerApp::renderFrame(Adafruit_ST7735 &tft, int frameIndex) {
    if (frameIndex < 0 || frameIndex >= SHINCHAN_FRAME_COUNT) return;

    const uint8_t *jpgData = shinchan_frames[frameIndex];
    uint16_t jpgLen = shinchan_frame_lens[frameIndex];

    JpgStream stream;
    stream.data = jpgData;
    stream.len = jpgLen;
    stream.offset = 0;

    s_renderCtx.tft = &tft;

    JDEC jdec;
    JRESULT res = jd_prepare(&jdec, jpgInputFunc, s_tjpgd_work, sizeof(s_tjpgd_work), &stream);
    if (res == JDR_OK) {
        // Fullscreen 128x160 decode (scale 0 = 1:1 direct mapping)
        jd_decomp(&jdec, jpgOutputFunc, 0);
    }

    // Render transparent floating timer digits directly over the newly decoded full-screen frame
    drawFloatingTimer(tft);
}

// -----------------------------------------------------------------------------
// Minimal Floating Timer UI (Digits Only, Transparent Overlay)
// -----------------------------------------------------------------------------

static void drawFloatingTimer(Adafruit_ST7735 &tft) {
    uint16_t colOrange = tft.color565(255, 122, 0);

    // Format current time digits
    uint8_t h, m, s;
    if (s_timerState == TIMER_STATE_SELECTION) {
        h = s_selectedHours;
        m = s_selectedMinutes;
        s = s_selectedSeconds;
    } else {
        h = (uint8_t)(s_remainingSeconds / 3600UL);
        m = (uint8_t)((s_remainingSeconds % 3600UL) / 60UL);
        s = (uint8_t)(s_remainingSeconds % 60UL);
    }

    char str[9];
    snprintf(str, sizeof(str), "%02u:%02u:%02u", h, m, s);

    // Exact horizontal coordinates (104 px width, symmetrically centered on 128 px screen):
    // Left margin = 12 px, Right margin = 12 px
    // H0: 12, H1: 27, Colon 0: 43, M0: 50, M1: 65, Colon 1: 81, S0: 88, S1: 103
    static const int xPos[8] = {12, 27, 43, 50, 65, 81, 88, 103};
    const int startY = FLOATING_TIMER_Y;

    // Determine colors per character:
    uint16_t charColors[8];
    for (int i = 0; i < 8; i++) {
        if (s_timerState == TIMER_STATE_SELECTION) {
            bool isSelected = false;
            if (s_selectedField == FIELD_HOURS && (i == 0 || i == 1)) isSelected = true;
            else if (s_selectedField == FIELD_MINUTES && (i == 3 || i == 4)) isSelected = true;
            else if (s_selectedField == FIELD_SECONDS && (i == 6 || i == 7)) isSelected = true;

            charColors[i] = isSelected ? colOrange : ST77XX_WHITE;
        } else if (s_timerState == TIMER_STATE_PAUSED) {
            charColors[i] = ST77XX_YELLOW;
        } else if (s_timerState == TIMER_STATE_COMPLETED) {
            charColors[i] = ST77XX_YELLOW;
        } else {
            charColors[i] = ST77XX_WHITE;
        }
    }

    tft.startWrite();

    // Pass 1: Render 1-pixel dark drop shadow behind all glyphs (Black, offset +1, +1)
    for (int i = 0; i < 8; i++) {
        char c = str[i];
        int x = xPos[i];
        if (c == ':') {
            for (int r = 0; r < TIMER_FONT_DIGIT_H; r++) {
                uint8_t row = pgm_read_byte(&timer_glyph_colon[r]);
                if (!row) continue;
                for (int b = 0; b < TIMER_FONT_COLON_W; b++) {
                    if (row & (1 << (3 - b))) {
                        tft.writePixel(x + b + 1, startY + r + 1, ST77XX_BLACK);
                    }
                }
            }
        } else if (c >= '0' && c <= '9') {
            int idx = c - '0';
            const uint16_t* glyph = timer_glyphs[idx];
            for (int r = 0; r < TIMER_FONT_DIGIT_H; r++) {
                uint16_t row = pgm_read_word(&glyph[r]);
                if (!row) continue;
                for (int b = 0; b < TIMER_FONT_DIGIT_W; b++) {
                    if (row & (1 << (12 - b))) {
                        tft.writePixel(x + b + 1, startY + r + 1, ST77XX_BLACK);
                    }
                }
            }
        }
    }

    // Pass 2: Render crisp foreground glyph pixels
    for (int i = 0; i < 8; i++) {
        char c = str[i];
        int x = xPos[i];
        uint16_t col = charColors[i];
        if (c == ':') {
            for (int r = 0; r < TIMER_FONT_DIGIT_H; r++) {
                uint8_t row = pgm_read_byte(&timer_glyph_colon[r]);
                if (!row) continue;
                for (int b = 0; b < TIMER_FONT_COLON_W; b++) {
                    if (row & (1 << (3 - b))) {
                        tft.writePixel(x + b, startY + r, col);
                    }
                }
            }
        } else if (c >= '0' && c <= '9') {
            int idx = c - '0';
            const uint16_t* glyph = timer_glyphs[idx];
            for (int r = 0; r < TIMER_FONT_DIGIT_H; r++) {
                uint16_t row = pgm_read_word(&glyph[r]);
                if (!row) continue;
                for (int b = 0; b < TIMER_FONT_DIGIT_W; b++) {
                    if (row & (1 << (12 - b))) {
                        tft.writePixel(x + b, startY + r, col);
                    }
                }
            }
        }
    }

    // In Selection mode: Draw a clean subtle underline beneath the 2 active digits
    if (s_timerState == TIMER_STATE_SELECTION) {
        int ulX = 12;
        if (s_selectedField == FIELD_MINUTES) ulX = 50;
        else if (s_selectedField == FIELD_SECONDS) ulX = 88;

        // Shadow for underline
        for (int px = ulX; px < ulX + 28; px++) {
            tft.writePixel(px + 1, startY + 31, ST77XX_BLACK);
            tft.writePixel(px + 1, startY + 32, ST77XX_BLACK);
        }
        // Foreground underline
        for (int px = ulX; px < ulX + 28; px++) {
            tft.writePixel(px, startY + 30, colOrange);
            tft.writePixel(px, startY + 31, colOrange);
        }
    }

    tft.endWrite();
}

void FocusTimerApp::drawOverlayUI(Adafruit_ST7735 &tft) {
    drawFloatingTimer(tft);
}

// -----------------------------------------------------------------------------
// Button Action Handlers
// -----------------------------------------------------------------------------

static void handleButtonAShort(Adafruit_ST7735 &tft) {
    if (s_timerState == TIMER_STATE_SELECTION) {
        uint32_t totalSec = (uint32_t)s_selectedHours * 3600UL +
                            (uint32_t)s_selectedMinutes * 60UL +
                            (uint32_t)s_selectedSeconds;
        if (totalSec > 0) {
            s_remainingSeconds = totalSec;
            s_timerState = TIMER_STATE_RUNNING;
            s_lastTickMs = millis();
            FocusTimerApp::renderFrame(tft, s_currentFrame);
        }
    }
}

static void handleButtonALong(Adafruit_ST7735 &tft) {
    // Hold A >= 3 seconds: Stop & reset to 00:00:00
    s_timerState = TIMER_STATE_SELECTION;
    s_selectedHours = 0;
    s_selectedMinutes = 0;
    s_selectedSeconds = 0;
    s_selectedField = FIELD_HOURS;
    s_remainingSeconds = 0;
    FocusTimerApp::renderFrame(tft, s_currentFrame);
}

static void handleButtonBackShort(Adafruit_ST7735 &tft) {
    if (s_timerState == TIMER_STATE_SELECTION) {
        s_wantsExit = true;
    } else if (s_timerState == TIMER_STATE_RUNNING) {
        s_timerState = TIMER_STATE_PAUSED;
        FocusTimerApp::renderFrame(tft, s_currentFrame);
    } else if (s_timerState == TIMER_STATE_PAUSED) {
        s_timerState = TIMER_STATE_RUNNING;
        s_lastTickMs = millis();
        FocusTimerApp::renderFrame(tft, s_currentFrame);
    } else if (s_timerState == TIMER_STATE_COMPLETED) {
        s_timerState = TIMER_STATE_SELECTION;
        s_selectedHours = 0;
        s_selectedMinutes = 0;
        s_selectedSeconds = 0;
        s_selectedField = FIELD_HOURS;
        FocusTimerApp::renderFrame(tft, s_currentFrame);
    }
}

static void handleButtonBackLong() {
    // Hold Back >= 2 seconds: Exit app to Apps Grid
    while (digitalRead(PIN_BTN_BACK) == LOW) {
        delay(10);
    }
    clearButtonEvents();
    s_wantsExit = true;
}

static void updateButtonsAndJoystick(Adafruit_ST7735 &tft) {
    unsigned long now = millis();

    // --- 1. Joystick Controls (Selection Screen Only) ---
    Joystick.update();
    if (s_timerState == TIMER_STATE_SELECTION) {
        bool changed = false;
        if (Joystick.wasLeftPressed()) {
            s_selectedField = (TimerField)((s_selectedField + 2) % 3);
            changed = true;
        } else if (Joystick.wasRightPressed()) {
            s_selectedField = (TimerField)((s_selectedField + 1) % 3);
            changed = true;
        } else if (Joystick.wasUpPressed()) {
            if (s_selectedField == FIELD_HOURS) {
                s_selectedHours = (s_selectedHours + 1) % 100;
            } else if (s_selectedField == FIELD_MINUTES) {
                s_selectedMinutes = (s_selectedMinutes + 1) % 60;
            } else if (s_selectedField == FIELD_SECONDS) {
                s_selectedSeconds = (s_selectedSeconds + 1) % 60;
            }
            changed = true;
        } else if (Joystick.wasDownPressed()) {
            if (s_selectedField == FIELD_HOURS) {
                s_selectedHours = (s_selectedHours + 99) % 100;
            } else if (s_selectedField == FIELD_MINUTES) {
                s_selectedMinutes = (s_selectedMinutes + 59) % 60;
            } else if (s_selectedField == FIELD_SECONDS) {
                s_selectedSeconds = (s_selectedSeconds + 59) % 60;
            }
            changed = true;
        }

        if (changed) {
            FocusTimerApp::renderFrame(tft, s_currentFrame);
        }
    }

    // --- 2. Button A (GPIO 13) Handling ---
    int rA = digitalRead(PIN_BTN_A);
    if (rA != s_rawBtnA) {
        s_debounceBtnA = now;
        s_rawBtnA = rA;
    }
    if ((now - s_debounceBtnA) > 30) {
        if (rA != s_stableBtnA) {
            s_stableBtnA = rA;
            if (s_stableBtnA == LOW) {
                s_btnAHoldStart = now;
                s_btnAHeld = true;
                s_btnALongHandled = false;
            } else {
                s_btnAHeld = false;
                if (!s_btnALongHandled) {
                    handleButtonAShort(tft);
                }
            }
        }
    }
    if (s_btnAHeld && !s_btnALongHandled) {
        if ((now - s_btnAHoldStart) >= 3000) {
            s_btnALongHandled = true;
            handleButtonALong(tft);
        }
    }

    // --- 3. Button BACK (GPIO 25) Handling ---
    int rB = digitalRead(PIN_BTN_BACK);
    if (rB != s_rawBtnBack) {
        s_debounceBtnBack = now;
        s_rawBtnBack = rB;
    }
    if ((now - s_debounceBtnBack) > 30) {
        if (rB != s_stableBtnBack) {
            s_stableBtnBack = rB;
            if (s_stableBtnBack == LOW) {
                s_btnBackHoldStart = now;
                s_btnBackHeld = true;
                s_btnBackLongHandled = false;
            } else {
                s_btnBackHeld = false;
                if (!s_btnBackLongHandled) {
                    handleButtonBackShort(tft);
                }
            }
        }
    }
    if (s_btnBackHeld && !s_btnBackLongHandled) {
        if ((now - s_btnBackHoldStart) >= 2000) {
            s_btnBackLongHandled = true;
            handleButtonBackLong();
            return;
        }
    }

    // Clear global button events so other listeners do not receive stale events
    clearButtonEvents();
}

void FocusTimerApp::init(Adafruit_ST7735 &tft) {
    s_wantsExit = false;
    s_active = true;
    s_currentFrame = 0; // Reset to frame 1 (index 0)

    // Reset timer state machine
    s_timerState = TIMER_STATE_SELECTION;
    s_selectedField = FIELD_HOURS;
    s_selectedHours = 0;
    s_selectedMinutes = 0;
    s_selectedSeconds = 0;
    s_remainingSeconds = 0;

    // Reset button states
    s_rawBtnA = HIGH;
    s_stableBtnA = HIGH;
    s_debounceBtnA = 0;
    s_btnAHeld = false;
    s_btnALongHandled = false;

    s_rawBtnBack = HIGH;
    s_stableBtnBack = HIGH;
    s_debounceBtnBack = 0;
    s_btnBackHeld = false;
    s_btnBackLongHandled = false;

    tft.setRotation(0); // 128x160 Portrait

    // Render frame 1 immediately with transparent floating timer overlay
    renderFrame(tft, s_currentFrame);
    s_lastFrameMs = millis();
}

void FocusTimerApp::update(Adafruit_ST7735 &tft) {
    if (!s_active) return;

    // 1. Process inputs
    updateButtonsAndJoystick(tft);
    if (s_wantsExit) return;

    unsigned long now = millis();

    // 2. Step countdown timer (independent of animation frame timing)
    if (s_timerState == TIMER_STATE_RUNNING) {
        while (now - s_lastTickMs >= 1000UL) {
            s_lastTickMs += 1000UL;
            if (s_remainingSeconds > 0) {
                s_remainingSeconds--;
                if (s_remainingSeconds == 0) {
                    s_timerState = TIMER_STATE_COMPLETED;
                    renderFrame(tft, s_currentFrame);
                    break;
                }
            }
        }
    }

    // 3. Step background Shinchan animation (non-blocking, exactly 100 ms)
    // Decodes full 128x160 frame and draws transparent floating digits overlay
    if (now - s_lastFrameMs >= FOCUS_TIMER_ANIM_INTERVAL_MS) {
        s_lastFrameMs = now;
        // Strictly forward loop: 0 -> 1 -> 2 -> ... -> 29 -> 0
        s_currentFrame = (s_currentFrame + 1) % SHINCHAN_FRAME_COUNT;
        renderFrame(tft, s_currentFrame);
    }
}

void FocusTimerApp::stop() {
    s_active = false;
    s_wantsExit = false;
}

bool FocusTimerApp::shouldExit() {
    if (s_wantsExit) {
        s_wantsExit = false;
        return true;
    }
    return false;
}

int FocusTimerApp::getCurrentFrame() {
    return s_currentFrame;
}

FocusTimerState FocusTimerApp::getState() {
    return s_timerState;
}

uint32_t FocusTimerApp::getRemainingSeconds() {
    return s_remainingSeconds;
}
