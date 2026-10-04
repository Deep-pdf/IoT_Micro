#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "esp_wifi_types.h"

#ifdef __cplusplus
extern "C" {
#endif

// Scans all channels and stores the AP list internally.
// Returns number of APs found.
int  wifi_controller_scan(void);

void wifi_controller_init(void);

// Returns the total number of APs from the last scan.
int  wifi_controller_get_count(void);

// Fills *out with the index-th AP from the last scan.
// Returns true if the index is valid.
bool wifi_controller_get_ap(int index, wifi_ap_record_t *out);

#ifdef __cplusplus
}
#endif
