#include "hackk/ui_screens.h"
#include "hackk/ui_theme.h"
#include "hackk/tft_driver.h"
#include "hackk/attack_eviltwin.h"
#include "hackk/wifi_controller.h"
#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static volatile int  s_scanning = 0;
static uint32_t s_scan_started_ms = 0;
static char          s_ssids[32][33];
static TaskHandle_t  s_scan_task = NULL;
static bool          s_scan_drawn = false;  // scanning screen painted once
static bool          s_run_header = false;  // running screen header/footer painted once

static void scan_task(void *arg) {
    int n = 0;
    n = wifi_controller_scan();          // always returns (10s max timeout)

    for (int i = 0; i < n && i < 32; i++) {
        wifi_ap_record_t r;
        wifi_controller_get_ap(i, &r);
        strncpy(s_ssids[i],
                (r.ssid[0] != 0) ? (const char *)r.ssid : "<hidden>",
                32);
        s_ssids[i][32] = '\0';
    }

    g_ui.ap_count = n;
    g_ui.cursor   = 0;
    g_ui.scroll   = 0;
    s_scanning    = 0;                   // ← guard: always clears
    s_scan_task   = NULL;
    vTaskDelete(NULL);
}

void ui_eviltwin_enter() {
    if (s_scan_task) return;
    s_scanning   = 1;
    s_scan_drawn = false;  // force scanning screen repaint
    s_run_header = false;  // reset for next run
    s_scan_started_ms = millis();
    xTaskCreate(scan_task, "etscan", 4096, NULL, 5, &s_scan_task);
}

void ui_eviltwin_draw() {
    if (s_scanning && (millis() - s_scan_started_ms > 12000)) {
        s_scanning  = 0;
        s_scan_task = NULL;
        g_ui.ap_count = 0;
    }

    if (s_scanning) {
        tft_clear();
        tft_header("Scanning...");
        tft_center(70, "Please wait", C_WARN, 1);
        return;
    }

    if (g_ui.cur == SCR_EVILTWIN_SCAN) {
        tft_clear();
        tft_header("Evil Twin - Pick AP");
        if (g_ui.ap_count == 0) {
            tft_center(70, "No APs found", C_WARN, 1);
            tft_footer("BK:back");
            return;
        }
        int rows = UI_LIST_H / ROW_H;
        if (g_ui.cursor < g_ui.scroll)            g_ui.scroll = g_ui.cursor;
        if (g_ui.cursor >= g_ui.scroll + rows)    g_ui.scroll = g_ui.cursor - rows + 1;

        for (int i = 0; i < rows; i++) {
            int idx = g_ui.scroll + i;
            if (idx >= g_ui.ap_count) break;
            tft_row(LIST_Y + i * ROW_H, s_ssids[idx], idx == g_ui.cursor);
        }
        tft_footer("OK:start BK:back");
        return;
    }

    if (g_ui.cur == SCR_EVILTWIN_RUNNING) {
        // Paint header/footer only once; only update the changing text lines
        if (!s_run_header) {
            tft_clear();
            tft_header("Evil Twin Running");
            tft_center(80, "Portal: 192.168.4.1", C_ACCENT, 1);
            tft_footer("BK:stop");
            s_run_header = true;
        }
        // Erase just the status line before redrawing
        if (hackk_tft) hackk_tft->fillRect(0, 44, SCR_W, 10, C_BG);
        tft_center(50, g_ui.status, C_FG, 1);
    }
}

void ui_eviltwin_handle(ui_input_t in) {
    if (s_scanning) return;

    if (g_ui.cur == SCR_EVILTWIN_SCAN) {
        if (in == INPUT_UP   && g_ui.cursor > 0) g_ui.cursor--;
        if (in == INPUT_DOWN && g_ui.cursor < g_ui.ap_count - 1) g_ui.cursor++;
        if (in == INPUT_ENTER && g_ui.ap_count > 0) attack_eviltwin_start(g_ui.cursor);
        if (in == INPUT_BACK)  ui_screen_set(SCR_MAIN);
        return;
    }
    if (g_ui.cur == SCR_EVILTWIN_RUNNING) {
        if (in == INPUT_BACK || in == INPUT_ENTER) attack_eviltwin_stop();
    }
}
