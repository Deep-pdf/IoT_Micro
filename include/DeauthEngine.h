#ifndef DEAUTHENGINE_H
#define DEAUTHENGINE_H

#include <Arduino.h>
#include <esp_wifi.h>

/* Send a single de‑authentication frame to the target BSSID. */
void sendDeauth(uint8_t* bssid, uint8_t channel);

#endif