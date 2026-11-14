#include "focus_timer/FocusTimerApp.h"
#include "focus_timer/shinchan_frames.h"
#include "button.h"
#include "joystick.h"
#include "esp32/rom/tjpgd.h"

#define PIN_BTN_A    13   // GPIO 13: Enter / A
#define PIN_BTN_BACK 25   // GPIO 25: Back

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

    int16_t w = rect->right - rect->left + 1;
    int16_t h = rect->bottom - rect->top + 1;

    // Reject block if completely out of bounds (safety guard)
    if (rect->right < 0 || rect->left >= 128 || rect->bottom < 0 || rect->top >= 160) {
        return 1;
    }

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
        jd_decomp(&jdec, jpgOutputFunc, 0); // 1:1 scale (128x160 full screen)
    }

    // Always redraw the foreground UI overlay on top of animation pixels
    drawOverlayUI(tft);
}

void FocusTimerApp::drawOverlayUI(Adafruit_ST7735 &tft) {
    tft.setFont(NULL);
    tft.setTextWrap(false);

    uint16_t colOrange    = tft.color565(255, 122, 0);
    uint16_t colDarkCard  = tft.color565(16, 18, 24);
    uint16_t colCardBorder= tft.color565(70, 75, 85);
    uint16_t colBoxDark   = tft.color565(32, 36, 44);
    uint16_t colBoxBorder = tft.color565(60, 65, 78);

    if (s_timerState == TIMER_STATE_SELECTION) {
        // --- Selection Screen UI ---
        // 1. Dark background HUD card
        tft.fillRoundRect(4, 6, 120, 58, 4, colDarkCard);
        tft.drawRoundRect(4, 6, 120, 58, 4, colCardBorder);

        // Header Title
        tft.setTextSize(1);
        tft.setTextColor(colOrange);
        tft.setCursor(31, 10);
        tft.print("FOCUS TIMER");

        // 2. Three numeric selectable boxes: [HOURS] [MINUTES] [SECONDS]
        // Order: HOURS -> MINUTES -> SECONDS
        const int boxW = 34;
        const int boxH = 30;
        const int boxY = 22;
        const int boxX[3] = {8, 47, 86};

        for (int i = 0; i < 3; i++) {
            int bx = boxX[i];
            bool isSelected = (s_selectedField == (TimerField)i);

            if (isSelected) {
                // Highlighted active box with IoT_Micro orange accent
                tft.fillRoundRect(bx, boxY, boxW, boxH, 3, colOrange);
                tft.drawRoundRect(bx, boxY, boxW, boxH, 3, ST77XX_WHITE);

                // Small arrow indicators above and below active box
                tft.fillTriangle(bx + 17, 18, bx + 13, 21, bx + 21, 21, ST77XX_WHITE);
                tft.fillTriangle(bx + 17, 54, bx + 13, 51, bx + 21, 51, ST77XX_WHITE);

                tft.setTextColor(ST77XX_BLACK);
            } else {
                // Unselected box with dark slate styling
                tft.fillRoundRect(bx, boxY, boxW, boxH, 3, colBoxDark);
                tft.drawRoundRect(bx, boxY, boxW, boxH, 3, colBoxBorder);

                tft.setTextColor(ST77XX_WHITE);
            }

            // Value digits (00-99 or 00-59)
            uint8_t val = (i == 0) ? s_selectedHours : (i == 1) ? s_selectedMinutes : s_selectedSeconds;
            char valStr[4];
            snprintf(valStr, sizeof(valStr), "%02d", val);

            tft.setTextSize(2);
            tft.setCursor(bx + 5, boxY + 2);
            tft.print(valStr);

            // Unit label: HOUR / MIN / SEC
            const char* unitStr = (i == 0) ? "HOUR" : (i == 1) ? "MIN" : "SEC";
            int unitX = (i == 0) ? (bx + 5) : (bx + 8);

            tft.setTextSize(1);
            tft.setCursor(unitX, boxY + 20);
            tft.print(unitStr);
        }

        // 3. Compact helper prompt pill below card
        tft.fillRoundRect(6, 68, 116, 12, 3, ST77XX_BLACK);
        tft.drawRoundRect(6, 68, 116, 12, 3, colCardBorder);
        tft.setTextSize(1);
        tft.setTextColor(ST77XX_WHITE);
        tft.setCursor(11, 70);
        tft.print("A:START   BACK:EXIT");

    } else if (s_timerState == TIMER_STATE_RUNNING || s_timerState == TIMER_STATE_PAUSED) {
        // --- Countdown Running / Paused UI ---
        bool isPaused = (s_timerState == TIMER_STATE_PAUSED);
        uint16_t borderColor = isPaused ? ST77XX_YELLOW : colOrange;

        tft.fillRoundRect(6, 8, 116, 54, 4, colDarkCard);
        tft.drawRoundRect(6, 8, 116, 54, 4, borderColor);

        // Header status indicator
        tft.setTextSize(1);
        if (isPaused) {
            tft.setTextColor(ST77XX_YELLOW);
            tft.setCursor(46, 13);
            tft.print("PAUSED");
        } else {
            tft.setTextColor(colOrange);
            tft.setCursor(28, 13);
            tft.print("* FOCUSING *");
        }

        // Remaining time in HH:MM:SS format
        uint32_t hrs = s_remainingSeconds / 3600UL;
        uint32_t mins = (s_remainingSeconds % 3600UL) / 60UL;
        uint32_t secs = s_remainingSeconds % 60UL;

        char timeStr[12];
        snprintf(timeStr, sizeof(timeStr), "%02lu:%02lu:%02lu", (unsigned long)hrs, (unsigned long)mins, (unsigned long)secs);

        tft.setTextSize(2);
        tft.setTextColor(isPaused ? ST77XX_YELLOW : ST77XX_WHITE);
        tft.setCursor(16, 25);
        tft.print(timeStr);

        // Bottom status & controls hint
        tft.setTextSize(1);
        if (s_btnAHeld) {
            tft.setTextColor(ST77XX_RED);
            tft.setCursor(9, 47);
            tft.print("HOLD A: RESETTING...");
        } else if (isPaused) {
            tft.setTextColor(ST77XX_YELLOW);
            tft.setCursor(4, 47);
            tft.print("BACK:RESUME HOLD A:RST");
        } else {
            tft.setTextColor(tft.color565(170, 175, 185));
            tft.setCursor(6, 47);
            tft.print("BACK:PAUSE HOLD A:RST");
        }

    } else if (s_timerState == TIMER_STATE_COMPLETED) {
        // --- Timer Completed UI ---
        uint16_t colCompletedCard = tft.color565(40, 10, 10);
        tft.fillRoundRect(6, 8, 116, 54, 4, colCompletedCard);
        tft.drawRoundRect(6, 8, 116, 54, 4, ST77XX_YELLOW);

        tft.setTextSize(1);
        tft.setTextColor(ST77XX_YELLOW);
        tft.setCursor(34, 13);
        tft.print("TIME'S UP!");

        tft.setTextSize(2);
        tft.setTextColor(ST77XX_WHITE);
        tft.setCursor(16, 25);
        tft.print("00:00:00");

        tft.setTextSize(1);
        tft.setTextColor(ST77XX_WHITE);
        tft.setCursor(10, 47);
        tft.print("PRESS BACK: SELECT");
    }
}

static void handleButtonAShort() {
    if (s_timerState == TIMER_STATE_SELECTION) {
        uint32_t totalSec = (uint32_t)s_selectedHours * 3600UL +
                            (uint32_t)s_selectedMinutes * 60UL +
                            (uint32_t)s_selectedSeconds;
        if (totalSec > 0) {
            s_remainingSeconds = totalSec;
            s_timerState = TIMER_STATE_RUNNING;
            s_lastTickMs = millis();
        }
    }
}

static void handleButtonALong() {
    // Hold A >= 3 seconds: Stop & reset to 00:00:00
    s_timerState = TIMER_STATE_SELECTION;
    s_selectedHours = 0;
    s_selectedMinutes = 0;
    s_selectedSeconds = 0;
    s_selectedField = FIELD_HOURS;
    s_remainingSeconds = 0;
}

static void handleButtonBackShort() {
    if (s_timerState == TIMER_STATE_SELECTION) {
        s_wantsExit = true;
    } else if (s_timerState == TIMER_STATE_RUNNING) {
        s_timerState = TIMER_STATE_PAUSED;
    } else if (s_timerState == TIMER_STATE_PAUSED) {
        s_timerState = TIMER_STATE_RUNNING;
        s_lastTickMs = millis();
    } else if (s_timerState == TIMER_STATE_COMPLETED) {
        s_timerState = TIMER_STATE_SELECTION;
        s_selectedHours = 0;
        s_selectedMinutes = 0;
        s_selectedSeconds = 0;
        s_selectedField = FIELD_HOURS;
    }
}

static void handleButtonBackLong() {
    // Hold Back >= 2 seconds: Exit app to Apps Grid
    // Wait for physical button release before exiting so parent doesn't register ghost click
    while (digitalRead(PIN_BTN_BACK) == LOW) {
        delay(10);
    }
    clearButtonEvents();
    s_wantsExit = true;
}

static void updateButtonsAndJoystick(Adafruit_ST7735 &tft) {
    unsigned long now = millis();
    bool uiNeedsRedraw = false;

    // --- 1. Joystick Controls ---
    Joystick.update();
    if (s_timerState == TIMER_STATE_SELECTION) {
        if (Joystick.wasLeftPressed()) {
            s_selectedField = (TimerField)((s_selectedField + 2) % 3);
            uiNeedsRedraw = true;
        } else if (Joystick.wasRightPressed()) {
            s_selectedField = (TimerField)((s_selectedField + 1) % 3);
            uiNeedsRedraw = true;
        } else if (Joystick.wasUpPressed()) {
            if (s_selectedField == FIELD_HOURS) {
                s_selectedHours = (s_selectedHours + 1) % 100;
            } else if (s_selectedField == FIELD_MINUTES) {
                s_selectedMinutes = (s_selectedMinutes + 1) % 60;
            } else if (s_selectedField == FIELD_SECONDS) {
                s_selectedSeconds = (s_selectedSeconds + 1) % 60;
            }
            uiNeedsRedraw = true;
        } else if (Joystick.wasDownPressed()) {
            if (s_selectedField == FIELD_HOURS) {
                s_selectedHours = (s_selectedHours + 99) % 100;
            } else if (s_selectedField == FIELD_MINUTES) {
                s_selectedMinutes = (s_selectedMinutes + 59) % 60;
            } else if (s_selectedField == FIELD_SECONDS) {
                s_selectedSeconds = (s_selectedSeconds + 59) % 60;
            }
            uiNeedsRedraw = true;
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
                    handleButtonAShort();
                    uiNeedsRedraw = true;
                }
            }
        }
    }
    if (s_btnAHeld && !s_btnALongHandled) {
        if ((now - s_btnAHoldStart) >= 3000) {
            s_btnALongHandled = true;
            handleButtonALong();
            uiNeedsRedraw = true;
        } else if (s_timerState != TIMER_STATE_SELECTION) {
            // Update UI while holding so user sees visual feedback
            uiNeedsRedraw = true;
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
                    handleButtonBackShort();
                    uiNeedsRedraw = true;
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

    if (uiNeedsRedraw) {
        FocusTimerApp::drawOverlayUI(tft);
    }
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
    tft.fillScreen(ST77XX_BLACK);

    // Render frame 1 immediately upon entering Focus Timer
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
        bool secondPassed = false;
        while (now - s_lastTickMs >= 1000UL) {
            s_lastTickMs += 1000UL;
            if (s_remainingSeconds > 0) {
                s_remainingSeconds--;
                secondPassed = true;
                if (s_remainingSeconds == 0) {
                    s_timerState = TIMER_STATE_COMPLETED;
                    break;
                }
            }
        }
        if (secondPassed) {
            drawOverlayUI(tft);
        }
    }

    // 3. Step background Shinchan animation (non-blocking, exactly 100 ms)
    if (now - s_lastFrameMs >= FOCUS_TIMER_ANIM_INTERVAL_MS) {
        s_lastFrameMs = now;
        // Strictly forward loop: 0 -> 1 -> 2 -> ... -> 29 -> 0
        s_currentFrame = (s_currentFrame + 1) % SHINCHAN_FRAME_COUNT;
        renderFrame(tft, s_currentFrame);
        // (renderFrame calls drawOverlayUI at the end of each frame)
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
