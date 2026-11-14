#ifndef SHINCHAN_FRAMES_H
#define SHINCHAN_FRAMES_H

#include <Arduino.h>

#define SHINCHAN_FRAME_COUNT  30
#define SHINCHAN_FRAME_WIDTH  128
#define SHINCHAN_FRAME_HEIGHT 160

extern const uint8_t* const shinchan_frames[SHINCHAN_FRAME_COUNT];
extern const uint16_t shinchan_frame_lens[SHINCHAN_FRAME_COUNT];

#endif // SHINCHAN_FRAMES_H
