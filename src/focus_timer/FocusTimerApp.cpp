#include "focus_timer/FocusTimerApp.h"
#include "focus_timer/shinchan_frames.h"
#include "button.h"
#include "esp32/rom/tjpgd.h"

// Animation state
static int s_currentFrame = 0;              // 0 to 29 (frames 1 to 30)
static unsigned long s_lastFrameMs = 0;     // Timestamp of last frame transition
static bool s_wantsExit = false;            // Set true on Back button press
static bool s_active = false;               // Active playback flag

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
}

void FocusTimerApp::init(Adafruit_ST7735 &tft) {
    s_wantsExit = false;
    s_active = true;
    s_currentFrame = 0; // Reset to frame 1 (index 0)

    tft.setRotation(0); // 128x160 Portrait
    tft.fillScreen(ST77XX_BLACK);

    // Render frame 1 immediately upon entering Focus Timer
    renderFrame(tft, s_currentFrame);
    s_lastFrameMs = millis();
}

void FocusTimerApp::update(Adafruit_ST7735 &tft) {
    if (!s_active) return;

    // Check Back button to request exit
    if (isBackPressed()) {
        s_wantsExit = true;
        return;
    }

    // Non-blocking step based on millis() timing
    unsigned long now = millis();
    if (now - s_lastFrameMs >= FOCUS_TIMER_ANIM_INTERVAL_MS) {
        s_lastFrameMs = now;
        // Strict forward loop: 0 -> 1 -> 2 -> ... -> 29 -> 0
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

