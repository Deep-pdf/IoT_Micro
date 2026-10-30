#ifndef NES_DEBUG_H
#define NES_DEBUG_H

#include <stdint.h>

#define PROFILE_SCOPE(tag) do {} while(0)
#define LOG(msg)
#define LOGF(fmt, ...)
inline void profileReport(uint32_t frame_interval = 60) { (void)frame_interval; }

#endif // NES_DEBUG_H
