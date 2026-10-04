#pragma once
#include <Adafruit_GFX.h>

// ST7735 red tab is 128x160
#define SCR_W   128
#define SCR_H   160

// Layout
#define HDR_H   16
#define ROW_H   14
#define FOOT_H  10
#define LIST_Y  HDR_H
#define UI_LIST_H  (SCR_H - HDR_H - FOOT_H)   // 134 px

// RGB565 palette
#define C_BG       0x0000   // black
#define C_FG       0xFFFF   // white
#define C_HDR      0x001F   // blue
#define C_FOOT     0x4208   // dark grey
#define C_SEL_BG   0x07E0   // green
#define C_SEL_FG   0x0000   // black text on selection
#define C_ACCENT   0xF800   // red
#define C_WARN     0xFFE0   // yellow
