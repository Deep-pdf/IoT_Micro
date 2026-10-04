#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// Restores the normal management AP after an attack that took over
// the radio (e.g., Evil Twin). Call this from the teardown path.
void management_helper_restore_ap(void);

#ifdef __cplusplus
}
#endif
