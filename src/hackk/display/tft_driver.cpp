#include "hackk/tft_driver.h"

/*
 * Adapted TFT driver for integrated hackk subsystem.
 * Uses the main project's Adafruit_ST7735 instance via pointer
 * instead of creating its own. All drawing functions delegate to
 * the shared display through hackk_tft.
 */

Adafruit_ST7735 *hackk_tft = nullptr;

void hackk_tft_set_instance(Adafruit_ST7735 *main_tft) {
    hackk_tft = main_tft;
}

void hackk_tft_init() {
    // Do NOT call initR() — the main project already initialised the hardware.
    // Just reset text state for hackk screens.
    if (!hackk_tft) return;
    hackk_tft->setRotation(0);
    hackk_tft->fillScreen(C_BG);
    hackk_tft->setTextWrap(false);
    hackk_tft->setTextColor(C_FG, C_BG);
    hackk_tft->setTextSize(1);
    hackk_tft->setFont(NULL);  // reset to default built-in font
}

void tft_clear() { if (hackk_tft) hackk_tft->fillScreen(C_BG); }

void tft_header(const char *title) {
    if (!hackk_tft) return;
    hackk_tft->fillRect(0, 0, SCR_W, HDR_H, C_HDR);
    hackk_tft->setTextColor(C_FG, C_HDR);
    hackk_tft->setTextSize(1);
    hackk_tft->setFont(NULL);
    hackk_tft->setCursor(2, 4);
    hackk_tft->print(title);
}

void tft_footer(const char *hint) {
    if (!hackk_tft) return;
    int y = SCR_H - FOOT_H;
    hackk_tft->fillRect(0, y, SCR_W, FOOT_H, C_FOOT);
    hackk_tft->setTextColor(C_FG, C_FOOT);
    hackk_tft->setTextSize(1);
    hackk_tft->setFont(NULL);
    hackk_tft->setCursor(2, y + 1);
    hackk_tft->print(hint);
}

void tft_row(int y, const char *text, bool selected) {
    if (!hackk_tft) return;
    uint16_t bg = selected ? C_SEL_BG : C_BG;
    uint16_t fg = selected ? C_SEL_FG : C_FG;
    hackk_tft->fillRect(0, y, SCR_W, ROW_H, bg);
    hackk_tft->setTextColor(fg, bg);
    hackk_tft->setTextSize(1);
    hackk_tft->setFont(NULL);
    hackk_tft->setCursor(3, y + 3);
    // Truncate to fit: ~20 chars at size 1
    char buf[22];
    strncpy(buf, text, 21);
    buf[21] = 0;
    hackk_tft->print(buf);
}

void tft_center(int y, const char *text, uint16_t color, uint8_t size) {
    if (!hackk_tft) return;
    hackk_tft->setTextSize(size);
    hackk_tft->setFont(NULL);
    int16_t x1, y1; uint16_t w, h;
    hackk_tft->getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
    hackk_tft->setTextColor(color, C_BG);
    hackk_tft->setCursor((SCR_W - w) / 2, y);
    hackk_tft->print(text);
}
