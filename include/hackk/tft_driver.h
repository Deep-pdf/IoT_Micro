#pragma once
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include "hackk/ui_theme.h"

/*
 * TFT driver for the hackk (deauth) subsystem.
 *
 * IMPORTANT: Unlike the standalone deauth project, this does NOT own
 * the Adafruit_ST7735 instance. Instead it holds a pointer to the
 * main project's tft object, set via hackk_tft_set_instance().
 */

// Pointer to the main project's tft — set once at hackk init time
extern Adafruit_ST7735 *hackk_tft;

void hackk_tft_set_instance(Adafruit_ST7735 *main_tft);
void hackk_tft_init();      // resets display state for hackk screens
void tft_clear();
void tft_header(const char *title);
void tft_footer(const char *hint);
void tft_row(int y, const char *text, bool selected);
void tft_center(int y, const char *text, uint16_t color, uint8_t size);
