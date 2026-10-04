#pragma once
#include "hackk/ui_input.h"
#include "hackk/ui_status.h"
void ui_deauth_enter();
void ui_eviltwin_enter();
bool ui_deauth_is_scanning();   // true while wifi scan task is running

typedef enum {
    SCR_SPLASH,
    SCR_MAIN,
    SCR_DEAUTH_SCAN,
    SCR_DEAUTH_RUNNING,
    SCR_EVILTWIN_SCAN,
    SCR_EVILTWIN_RUNNING,
    SCR_ABOUT,
} screen_t;

typedef struct {
    screen_t cur;
    int      cursor;
    int      scroll;
    int      ap_count;
    char     status[64];
    uint32_t frames;
} ui_ctx_t;

// Top-level dispatcher
void ui_handle(ui_input_t in);
void ui_apply_event(const ui_event_t *ev);
void ui_draw();
void ui_screen_set(screen_t s);

extern ui_ctx_t g_ui;
