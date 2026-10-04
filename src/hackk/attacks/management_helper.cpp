#include "hackk/management_helper.h"
#include "hackk/hackk_config.h"
#include <string.h>
#include "esp_wifi.h"
#include "esp_log.h"

static const char *TAG = "mgmt_helper";

void management_helper_restore_ap(void)
{
    ESP_LOGI(TAG, "Restoring management AP");

    // Configure the AP back to your normal settings
    wifi_config_t ap_cfg = {0};
    strncpy((char *)ap_cfg.ap.ssid, MGMT_AP_SSID, sizeof(ap_cfg.ap.ssid));
    ap_cfg.ap.ssid_len       = strlen(MGMT_AP_SSID);
    ap_cfg.ap.channel        = MGMT_AP_CHANNEL;
    ap_cfg.ap.authmode       = WIFI_AUTH_WPA2_PSK;
    ap_cfg.ap.max_connection = 4;

    strncpy((char *)ap_cfg.ap.password, MGMT_AP_PASSWORD,
            sizeof(ap_cfg.ap.password));

    // Apply to AP interface and restart it cleanly
    esp_wifi_set_config(WIFI_IF_AP, &ap_cfg);
    esp_wifi_set_channel(MGMT_AP_CHANNEL, WIFI_SECOND_CHAN_NONE);

    ESP_LOGI(TAG, "Management AP restored on channel %u", MGMT_AP_CHANNEL);
}
