#include "hackk/ui_screens.h"
#include "hackk/ui_theme.h"
#include "hackk/tft_driver.h"
#include "hackk/attack_dos.h"
#include "hackk/attack_eviltwin.h"
#include "hackk/wifi_controller.h"
#include <Arduino.h>

ui_ctx_t g_ui = { SCR_SPLASH, 0, 0, 0, {0}, 0 };

// ── Forward decls ─────────────────────────────────────────────
void ui_deauth_draw();
void ui_deauth_handle(ui_input_t in);
void ui_eviltwin_draw();
void ui_eviltwin_handle(ui_input_t in);

void ui_screen_set(screen_t s) {
    g_ui.cur    = s;
    g_ui.cursor = 0;
    g_ui.scroll = 0;
}

// ── Main menu ────────────────────────────────────────────────
static const char *MAIN_ITEMS[] = { "Deauth Attack", "Evil Twin", "About" };
#define MAIN_N 3

static void main_draw() {
    tft_clear();
    tft_header("HydraTFT");
    for (int i = 0; i < MAIN_N; i++)
        tft_row(LIST_Y + i * ROW_H, MAIN_ITEMS[i], i == g_ui.cursor);
    tft_footer("OK:sel BK:back");
}

static void about_draw() {
    tft_clear();
    tft_header("About");
    tft_center(40, "HydraTFT", C_ACCENT, 2);
    tft_center(70, "WiFi Security Lab", C_FG, 1);
    tft_center(90, "ESP32 + ST7735", C_FG, 1);
    tft_center(120, "For owned networks", C_WARN, 1);
    tft_footer("BK:back");
}

// ── Dispatcher ───────────────────────────────────────────────
void ui_handle(ui_input_t in) {
    if (in == INPUT_NONE) return;

    switch (g_ui.cur) {
    case SCR_SPLASH:
        ui_screen_set(SCR_MAIN);
        break;

    case SCR_MAIN:
        if (in == INPUT_UP)   g_ui.cursor = (g_ui.cursor + MAIN_N - 1) % MAIN_N;
        if (in == INPUT_DOWN) g_ui.cursor = (g_ui.cursor + 1) % MAIN_N;
        if (in == INPUT_ENTER) {
            if (g_ui.cursor == 0) { ui_screen_set(SCR_DEAUTH_SCAN);   ui_deauth_enter();   }
            if (g_ui.cursor == 1) { ui_screen_set(SCR_EVILTWIN_SCAN); ui_eviltwin_enter(); }
            if (g_ui.cursor == 2) { ui_screen_set(SCR_ABOUT);         }
        }
        if (in == INPUT_BACK) {
            // Signal exit back to main IoT_Micro project
            // This is handled by HackkApp::shouldExit()
            extern volatile bool hackk_exit_requested;
            hackk_exit_requested = true;
        }
        break;

    case SCR_DEAUTH_SCAN:
    case SCR_DEAUTH_RUNNING:
        ui_deauth_handle(in);
        break;

    case SCR_EVILTWIN_SCAN:
    case SCR_EVILTWIN_RUNNING:
        ui_eviltwin_handle(in);
        break;

    case SCR_ABOUT:
        if (in == INPUT_BACK || in == INPUT_ENTER) ui_screen_set(SCR_MAIN);
        break;
    }
}

void ui_draw() {
    switch (g_ui.cur) {
    case SCR_SPLASH:          tft_clear();
                              tft_center(60, "HydraTFT", C_ACCENT, 2);
                              tft_center(90, "press ENTER", C_FG, 1);
                              break;
    case SCR_MAIN:            main_draw();          break;
    case SCR_DEAUTH_SCAN:
    case SCR_DEAUTH_RUNNING:  ui_deauth_draw();     break;
    case SCR_EVILTWIN_SCAN:
    case SCR_EVILTWIN_RUNNING:ui_eviltwin_draw();   break;
    case SCR_ABOUT:           about_draw();         break;
    }
}

// ── Event bus consumer ───────────────────────────────────────
void ui_apply_event(const ui_event_t *ev) {
    switch (ev->type) {
    case EVT_DEAUTH_STARTED:   strncpy(g_ui.status, ev->line, 63);
                               ui_screen_set(SCR_DEAUTH_RUNNING); break;
    case EVT_DEAUTH_STOPPED:   ui_screen_set(SCR_DEAUTH_SCAN);    break;
    case EVT_DEAUTH_FRAME:     g_ui.frames = ev->counter;         break;
    case EVT_EVILTWIN_STARTED: strncpy(g_ui.status, ev->line, 63);
                               ui_screen_set(SCR_EVILTWIN_RUNNING); break;
    case EVT_EVILTWIN_STOPPED: ui_screen_set(SCR_EVILTWIN_SCAN);  break;
    case EVT_LOG:
    case EVT_CREDS_CAPTURED:   strncpy(g_ui.status, ev->line, 63); break;
    }
}
