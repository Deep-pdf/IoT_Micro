// wsl_bypasser.cpp
// Overrides the ESP32 WiFi stack's internal deauth-frame sanity check.
// Without this, ieee80211_raw_frame_sanity_check() rejects management frames
// injected via esp_wifi_80211_tx(), making deauth attacks silently fail.
//
// The override works because -Wl,-zmuldefs lets the linker keep the first
// definition it sees. This file is compiled into user code, which is linked
// before the closed-source WiFi firmware library, so this version wins.
//
// Used by: risinek/esp32-wifi-penetration-tool, Marauder, Bruce, Evil-M5.

#include <stdint.h>

extern "C" int ieee80211_raw_frame_sanity_check(int32_t arg,
                                                int32_t arg2,
                                                int32_t arg3) {
    (void)arg; (void)arg2; (void)arg3;
    return 0;   // 0 = pass — allow all raw frames through
}
