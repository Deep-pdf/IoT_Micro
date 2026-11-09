#include "home_animation.h"
#include "joe_frames.h"
#include "home_screen.h"
#include "esp32/rom/tjpgd.h"

// Animation state
static int s_animFrame = 0;              // 0 to 29 (frames 1 to 30)
static int s_animDirection = 1;          // +1 = forward, -1 = reverse
static unsigned long s_lastAnimTick = 0;
static bool s_animActive = false;

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
    int16_t x_offset;
    int16_t y_offset;
    int16_t clip_x0;
    int16_t clip_x1;
    int16_t clip_y0;
    int16_t clip_y1;
};

static JpgRenderContext s_renderCtx;

// Output callback function for TJpgDec (called per MCU block)
static UINT jpgOutputFunc(JDEC* jdec, void* bitmap, JRECT* rect) {
    JpgRenderContext* ctx = &s_renderCtx;

    int16_t screen_left   = rect->left   + ctx->x_offset;
    int16_t screen_right  = rect->right  + ctx->x_offset;
    int16_t screen_top    = rect->top    + ctx->y_offset;
    int16_t screen_bottom = rect->bottom + ctx->y_offset;

    // Reject block if completely outside visible clipping bounds
    if (screen_right < ctx->clip_x0 || screen_left > ctx->clip_x1 ||
        screen_bottom < ctx->clip_y0 || screen_top > ctx->clip_y1) {
        return 1;
    }

    // Clip to visible area
    int16_t draw_left   = (screen_left   < ctx->clip_x0) ? ctx->clip_x0 : screen_left;
    int16_t draw_right  = (screen_right  > ctx->clip_x1) ? ctx->clip_x1 : screen_right;
    int16_t draw_top    = (screen_top    < ctx->clip_y0) ? ctx->clip_y0 : screen_top;
    int16_t draw_bottom = (screen_bottom > ctx->clip_y1) ? ctx->clip_y1 : screen_bottom;

    int16_t draw_w = draw_right - draw_left + 1;
    int16_t draw_h = draw_bottom - draw_top + 1;
    int16_t block_w = rect->right - rect->left + 1;

    // Maximum MCU block size is 16x16 = 256 pixels
    uint16_t block_buf[256];
    int count = 0;

    uint8_t *src_bytes = (uint8_t*)bitmap;
    int16_t y_start = draw_top - screen_top;
    int16_t y_end   = draw_bottom - screen_top;
    int16_t x_start = draw_left - screen_left;
    int16_t x_end   = draw_right - screen_left;

    for (int16_t by = y_start; by <= y_end; by++) {
        int src_row_offset = by * block_w * 3;
        for (int16_t bx = x_start; bx <= x_end; bx++) {
            int src_idx = src_row_offset + bx * 3;
            uint8_t r = src_bytes[src_idx];
            uint8_t g = src_bytes[src_idx + 1];
            uint8_t b = src_bytes[src_idx + 2];
            // Fast RGB888 to RGB565 conversion
            block_buf[count++] = ((uint16_t)(r & 0xF8) << 8) | ((uint16_t)(g & 0xFC) << 3) | (b >> 3);
        }
    }

    if (count > 0) {
        ctx->tft->drawRGBBitmap(draw_left, draw_top, block_buf, draw_w, draw_h);
    }

    return 1;
}

void renderJoeFrame(Adafruit_ST7735 &tft, int frameIndex) {
    if (frameIndex < 0 || frameIndex >= JOE_FRAME_COUNT) return;

    const uint8_t *jpgData = joe_frames[frameIndex];
    uint16_t jpgLen = joe_frame_lens[frameIndex];

    JpgStream stream;
    stream.data = jpgData;
    stream.len = jpgLen;
    stream.offset = 0;

    s_renderCtx.tft = &tft;

#if ANIM_USE_FULLSCREEN_ZORDER
    // 1:1 scale (128x192), vertically centered on display (Y offset -16)
    // Clipped so Top Status Bar (0..9) and Bottom Dock (122..159) are protected
    s_renderCtx.x_offset = 0;
    s_renderCtx.y_offset = -16;
    s_renderCtx.clip_x0  = 0;
    s_renderCtx.clip_x1  = 127;
    s_renderCtx.clip_y0  = 10;   // Middle starts at Y=10
    s_renderCtx.clip_y1  = 121;  // Middle ends at Y=121
    uint8_t scale = 0;           // 1:1 scale
#else
    // 1:2 hardware scale (64x96), placed inside right-side media panel flush to side and bottom bar
    s_renderCtx.x_offset = 64;
    s_renderCtx.y_offset = 26;
    s_renderCtx.clip_x0  = 64;   // Strictly protect left column (X < 64) from animation overwrites
    s_renderCtx.clip_x1  = 127;
    s_renderCtx.clip_y0  = 26;   // Strictly protect date region (Y < 26) from animation overwrites
    s_renderCtx.clip_y1  = 121;  // Strictly protect bottom dock (Y >= 122) from animation overwrites
    uint8_t scale = 1;           // 1:2 scale
#endif

    JDEC jdec;
    JRESULT res = jd_prepare(&jdec, jpgInputFunc, s_tjpgd_work, sizeof(s_tjpgd_work), &stream);
    if (res == JDR_OK) {
        jd_decomp(&jdec, jpgOutputFunc, scale);
    }

    // Z-Order Requirement: Redraw date and clock text on top of animation pixels
    redrawHomeScreenText(tft);
}

void startHomeAnimation(Adafruit_ST7735 &tft) {
    // 1. Reset frame index to frame 1 (index 0)
    s_animFrame = 0;
    // 2. Set playback direction to forward
    s_animDirection = 1;
    // 3. Reset animation timing state
    s_lastAnimTick = millis();
    s_animActive = true;
    // 4. Display frame 1 immediately
    renderJoeFrame(tft, 0);
}

void stepHomeAnimation(Adafruit_ST7735 &tft) {
    if (!s_animActive) return;

    unsigned long now = millis();
    if (now - s_lastAnimTick < ANIM_FRAME_INTERVAL_MS) {
        return; // Frame not due yet
    }
    s_lastAnimTick = now;

    // Ping-pong sequence state machine:
    // Forward: 1 -> 2 -> ... -> 29 -> 30
    // Reverse: 29 -> 28 -> ... -> 2 -> 1
    // Repeat: 2 -> 3 -> ...
    s_animFrame += s_animDirection;
    if (s_animFrame >= JOE_FRAME_COUNT - 1) {
        s_animFrame = JOE_FRAME_COUNT - 1;
        s_animDirection = -1; // Reverse direction on next tick
    } else if (s_animFrame <= 0) {
        s_animFrame = 0;
        s_animDirection = 1;  // Forward direction on next tick
    }

    renderJoeFrame(tft, s_animFrame);
}

void stopHomeAnimation() {
    s_animActive = false;
}

bool isHomeAnimationActive() {
    return s_animActive;
}

int getHomeAnimationFrame() {
    return s_animFrame;
}
