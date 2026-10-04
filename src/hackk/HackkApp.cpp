#include "hackk/HackkApp.h"
#include "hackk/tft_driver.h"
#include "hackk/ui_input.h"
#include "hackk/ui_screens.h"
#include "hackk/ui_status.h"
#include "hackk/wifi_controller.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include <Arduino.h>
#include "hackk/hackk_config.h"

/*
 * Global exit flag — set by ui_menu.cpp when BACK is pressed on SCR_MAIN.
 * Declared extern in ui_menu.cpp so it can be written from there.
 */
volatile bool hackk_exit_requested = false;

namespace HackkApp {

static bool _initialised = false;

void init(Adafruit_ST7735 &tft)
{
    hackk_exit_requested = false;

    // 1. Hand the main project's TFT instance to the hackk subsystem
    hackk_tft_set_instance(&tft);

    // 2. Reset display state for hackk screens (clear, default font, etc.)
    hackk_tft_init();

    // 3. Init WiFi in APSTA mode so scans work and AP can be restored later
    //    (Arduino WiFi.begin was already called by the main project; we just
    //     switch to APSTA mode on top of that.)
    esp_wifi_set_mode(WIFI_MODE_APSTA);

    wifi_config_t ap_cfg = {0};
    strncpy((char *)ap_cfg.ap.ssid, MGMT_AP_SSID, sizeof(ap_cfg.ap.ssid));
    ap_cfg.ap.ssid_len = strlen(MGMT_AP_SSID);
    ap_cfg.ap.channel  = MGMT_AP_CHANNEL;
    ap_cfg.ap.authmode = WIFI_AUTH_WPA2_PSK;
    strncpy((char *)ap_cfg.ap.password, MGMT_AP_PASSWORD,
            sizeof(ap_cfg.ap.password));

    esp_wifi_set_config(WIFI_IF_AP, &ap_cfg);

    Serial.println(">>> HackkApp: WiFi APSTA configured");

    // 4. Init wifi controller (no-op but kept for API compat)
    wifi_controller_init();

    // 5. Init input subsystem (joystick + buttons for the deauth UI)
    ui_input_init();

    // 6. Show the splash screen
    ui_screen_set(SCR_SPLASH);
    ui_draw();

    _initialised = true;
    Serial.println(">>> HackkApp: initialised, entering deauth UI");
}

void update(Adafruit_ST7735 &tft)
{
    if (!_initialised) return;

    // 1. Poll input (50 ms blocking timeout, same as standalone deauth)
    ui_input_t in = ui_input_poll(50);
    if (in != INPUT_NONE) {
        ui_handle(in);
        ui_draw();
    }

    // 2. Process attack status events from FreeRTOS queue
    ui_event_t ev;
    while (ui_status_pop(&ev)) {
        ui_apply_event(&ev);
        ui_draw();
    }

    // 3. Periodic redraw while scanning (so AP list appears when scan finishes)
    static uint32_t last = 0;
    if (millis() - last > 300) {
        if (ui_deauth_is_scanning()) ui_draw();
        last = millis();
    }
}

bool shouldExit()
{
    if (hackk_exit_requested) {
        _initialised = false;
        hackk_exit_requested = false;
        hackk_tft_set_instance(nullptr);
        return true;
    }
    return false;
}

} // namespace HackkApp
