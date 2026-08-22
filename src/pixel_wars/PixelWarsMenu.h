#pragma once
#include "PixelWarsShared.h"

void drawPixelWarsLoadingScreen(int progress);
void drawPixelWarsStartMenu();
void navigatePixelWarsMenu(int direction);
void handlePixelWarsMenuSelection();
void drawPixelWarsHighScoreScreen();

// Draw helper primitives
void drawCenteredPWText(Adafruit_ST7735 &tft, const char *text, int16_t y, uint16_t color, uint8_t size, bool bold);
void drawTargetIcon(int16_t x, int16_t y, uint16_t color);
void drawTrophyIcon(int16_t x, int16_t y, uint16_t color);
void drawMiniShipIcon(int16_t x, int16_t y, uint16_t whiteColor, uint16_t redColor);
void drawExitIcon(int16_t x, int16_t y, uint16_t color);
void drawMenuCursor(int16_t rowY, int16_t rowH, uint16_t color);
void drawPixelWarsShip();
void drawPixelWarsPlanet();
