#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Start a deauthentication attack on a specific AP index (from scan list).
// Returns true if the attack was successfully initiated.
bool attack_dos_start(int ap_index);
bool attack_dos_start_by_bssid(const uint8_t *bssid, uint8_t channel);

// Stop any running deauthentication attack.
void attack_dos_stop(void);

// Returns true if a deauth attack is currently running.
bool attack_dos_is_running(void);

// Returns the number of deauth frames sent since the attack started.
uint32_t attack_dos_frames_sent(void);

#ifdef __cplusplus
}
#endif
