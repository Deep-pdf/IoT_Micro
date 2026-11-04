/*
 * Clean audio stub for isolated Gnuboy integration on classic ESP32
 * Disables audio processing, I2S, and DMA buffers while preserving
 * emulator timing assumptions and cycle counters.
 */

#include <string.h>
#include <stdlib.h>
#include "gnuboy.h"
#include "sound.h"
#include "hw.h"

static gb_snd_t s_snd;

gb_snd_t *gb_sound_init(void)
{
	memset(&s_snd, 0, sizeof(s_snd));
	return &s_snd;
}

void gb_sound_reset(bool hard)
{
	memset(&s_snd, 0, sizeof(s_snd));
	s_snd.rate = 0;
	s_snd.cycles = 0;
	GB.audio.pos = 0;
	R_NR52 = 0xF1;
}

void gb_sound_write(byte r, byte b)
{
	// Clean no-op: sound registers are stored in ioregs without audio DSP calculation
	(void)r;
	(void)b;
}

void gb_sound_dirty(void)
{
	// Clean no-op
}

void gb_sound_emulate(void)
{
	// Drain cycle accumulator to prevent integer overflow while consuming 0 CPU cycles
	s_snd.cycles = 0;
}

