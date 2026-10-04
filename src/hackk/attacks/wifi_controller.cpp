#include "hackk/wifi_controller.h"
#include <WiFi.h>
#include <string.h>
#include "esp_wifi.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "wifi_ctrl";

static wifi_ap_record_t g_aps[32];
static uint16_t         g_ap_count = 0;

void wifi_controller_init(void) {
    // Nothing to do — Arduino WiFi handles events internally.
    // Kept for API compatibility with main.cpp.
}

int wifi_controller_scan(void)
{
    g_ap_count = 0;

    // Make sure mode is APSTA
    wifi_mode_t mode;
    esp_wifi_get_mode(&mode);
    if (mode != WIFI_MODE_APSTA) {
        esp_wifi_set_mode(WIFI_MODE_APSTA);
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    // Blocking Arduino scan — has its own internal timeout.
    // Args: async=false, show_hidden=false, passive=false, ms_per_channel=120
    int n = WiFi.scanNetworks(false, false, false, 120);

    if (n <= 0) {
        ESP_LOGW(TAG, "Scan returned %d", n);
        return 0;
    }
    if (n > 32) n = 32;

    for (int i = 0; i < n; i++) {
        String ssid = WiFi.SSID(i);
        strncpy((char *)g_aps[i].ssid, ssid.c_str(),
                sizeof(g_aps[i].ssid) - 1);
        g_aps[i].ssid[sizeof(g_aps[i].ssid) - 1] = 0;

        const uint8_t *b = WiFi.BSSID(i);
        if (b) memcpy(g_aps[i].bssid, b, 6);

        g_aps[i].primary  = WiFi.channel(i);
        g_aps[i].rssi     = WiFi.RSSI(i);
        g_aps[i].authmode = (wifi_auth_mode_t)WiFi.encryptionType(i);
    }

    g_ap_count = n;
    ESP_LOGI(TAG, "Scan complete: %d APs", n);
    return n;
}

int wifi_controller_get_count(void) { return g_ap_count; }

bool wifi_controller_get_ap(int index, wifi_ap_record_t *out)
{
    if (!out || index < 0 || index >= g_ap_count) return false;
    *out = g_aps[index];
    return true;
}
