#ifndef JOE_FRAMES_H
#define JOE_FRAMES_H

#include <Arduino.h>

#define JOE_FRAME_COUNT 30
#define JOE_FRAME_WIDTH 128
#define JOE_FRAME_HEIGHT 192

extern const uint8_t* const joe_frames[JOE_FRAME_COUNT];
extern const uint16_t joe_frame_lens[JOE_FRAME_COUNT];

#endif // JOE_FRAMES_H
