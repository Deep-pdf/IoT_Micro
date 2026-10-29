#include "retro/RetroApp.h"
#include "button.h"
#include "Dream_Orphans_Bd6pt7b.h"

// Game entry structure
struct GameInfo {
    const char* title;
    const char* system;
    const char* genre;
    const char* status;
    uint16_t color;
};

static const GameInfo GAMES[] = {
    { "Super Mario Bros.", "NES (nofrendo)", "Platformer", "Milestone 2", 0xF800 /* Red */ },
    { "Contra",            "NES (nofrendo)", "Action Run&Gun", "Milestone 3", 0x07E0 /* Green */ },
    { "Pokemon Red",       "Game Boy (gnuboy)", "RPG Adventure", "Milestone 4", 0x001F /* Blue */ }
};

static const int TOTAL_GAMES = sizeof(GAMES) / sizeof(GAMES[0]);

static bool exitRequested = false;
static int selectedGame = 0;
static bool needsRedraw = true;
static bool joyCentered = true;
static unsigned long messageDismissTime = 0;
static const char* statusBanner = nullptr;

static void drawHeader(Adafruit_ST7735 &tft) {
    // Header bar (Height 18)
    tft.fillRect(0, 0, 128, 18, 0x18E3); // Dark slate
    tft.drawFastHLine(0, 18, 128, 0xFD40); // Amber dividing line

    tft.setFont(&Dream_Orphans_Bd6pt7b);
    tft.setTextSize(1);
    tft.setTextColor(0xFFFF);
    tft.setCursor(6, 13);
    tft.print("RETRO ARCADE");

    // Game index indicator on right
    char countBuf[8];
    snprintf(countBuf, sizeof(countBuf), "%d/%d", selectedGame + 1, TOTAL_GAMES);
    tft.setCursor(98, 13);
    tft.print(countBuf);
}

static void drawGameCard(Adafruit_ST7735 &tft) {
    const GameInfo &game = GAMES[selectedGame];

    // Card background
    tft.fillRect(0, 19, 128, 123, 0x0000); // Black background

    // Card frame
    tft.drawRoundRect(6, 24, 116, 114, 5, 0x4A69);
    tft.drawRoundRect(7, 25, 114, 112, 5, game.color);

    // System badge
    tft.fillRoundRect(12, 30, 104, 16, 3, 0x2124);
    tft.setFont(NULL); // Default 5x7 font
    tft.setTextSize(1);
    tft.setTextColor(0xFFE0); // Yellow
    tft.setCursor(18, 34);
    tft.print(game.system);

    // Game Title with Dream Orphans
    tft.setFont(&Dream_Orphans_Bd6pt7b);
    tft.setTextSize(1);
    tft.setTextColor(0xFFFF);
    tft.setCursor(12, 64);
    tft.print(game.title);

    // Genre info
    tft.setFont(NULL);
    tft.setTextColor(0x8410); // Gray
    tft.setCursor(14, 76);
    tft.print("Genre: ");
    tft.setTextColor(0xFFFF);
    tft.print(game.genre);

    // Core Status info
    tft.setTextColor(0x8410);
    tft.setCursor(14, 90);
    tft.print("Core: ");
    tft.setTextColor(0x07FF); // Cyan
    tft.print(game.status);

    // Carousel indicator dots
    int dotStartX = 64 - (TOTAL_GAMES * 10) / 2;
    for (int i = 0; i < TOTAL_GAMES; i++) {
        uint16_t dotCol = (i == selectedGame) ? 0xFD40 : 0x4A69;
        tft.fillCircle(dotStartX + i * 12 + 4, 112, (i == selectedGame) ? 3 : 2, dotCol);
    }

    // Action / Prompt Banner
    if (statusBanner != nullptr && millis() < messageDismissTime) {
        tft.fillRect(8, 122, 112, 14, 0x001F);
        tft.setTextColor(0xFFFF);
        tft.setCursor(14, 125);
        tft.print(statusBanner);
    } else {
        statusBanner = nullptr;
        tft.setTextColor(0xFD40);
        tft.setCursor(14, 125);
        tft.print("< Press ENTER >");
    }
}

static void drawFooter(Adafruit_ST7735 &tft) {
    // Footer bottom bar (Height 18)
    tft.fillRect(0, 142, 128, 18, 0x18E3);
    tft.drawFastHLine(0, 142, 128, 0x4A69);

    tft.setFont(NULL);
    tft.setTextSize(1);
    tft.setTextColor(0xAD55);
    tft.setCursor(6, 147);
    tft.print("LEFT/RIGHT:Pick BACK:Exit");
}

void RetroApp::init(Adafruit_ST7735 &tft) {
    exitRequested = false;
    needsRedraw = true;
    joyCentered = true;
    statusBanner = nullptr;

    tft.setRotation(0);
    tft.fillScreen(ST77XX_BLACK);
}

void RetroApp::update(Adafruit_ST7735 &tft) {
    if (exitRequested) return;

    if (isBackPressed()) {
        exitRequested = true;
        return;
    }

    // Read Joystick X for switching games
    int vrx = readJoystickX();
    int vry = readJoystickY();
    bool isCentered = (vrx > 1500 && vrx < 2700 && vry > 1500 && vry < 2700);

    if (isCentered) {
        joyCentered = true;
    } else if (joyCentered) {
        if (vrx < 1000) { // Left
            selectedGame = (selectedGame - 1 + TOTAL_GAMES) % TOTAL_GAMES;
            needsRedraw = true;
            joyCentered = false;
        } else if (vrx > 3000) { // Right
            selectedGame = (selectedGame + 1) % TOTAL_GAMES;
            needsRedraw = true;
            joyCentered = false;
        }
    }

    // Enter button handling
    if (isEnterPressed()) {
        statusBanner = "Core in Next Stage!";
        messageDismissTime = millis() + 1500;
        needsRedraw = true;
    }

    // Redraw if needed or if temporary status banner expires
    if (statusBanner != nullptr && millis() >= messageDismissTime) {
        statusBanner = nullptr;
        needsRedraw = true;
    }

    if (needsRedraw) {
        drawHeader(tft);
        drawGameCard(tft);
        drawFooter(tft);
        needsRedraw = false;
    }
}

bool RetroApp::shouldExit() {
    return exitRequested;
}

