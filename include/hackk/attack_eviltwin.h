#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Start an Evil Twin attack using the SSID and channel of a scanned AP.
bool attack_eviltwin_start(int ap_index);

// Stop the Evil Twin attack and tear down the rogue AP.
void attack_eviltwin_stop(void);

// Returns true if the Evil Twin is currently running.
bool attack_eviltwin_is_running(void);

#ifdef __cplusplus
}
#endif
