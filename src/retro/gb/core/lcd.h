#pragma once

#include "gnuboy.h"

#ifdef __cplusplus
extern "C" {
#endif

void gb_lcd_init(void);
void gb_lcd_deinit(void);
void gb_lcd_reset(bool hard);
void gb_lcd_emulate(int cycles);
void gb_lcd_stat_trigger(void);
void gb_lcd_lcdc_change(byte b);
void gb_lcd_pal_dirty(void);

#ifdef __cplusplus
}
#endif
