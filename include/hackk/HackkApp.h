#ifndef HACKK_APP_H
#define HACKK_APP_H

#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>

/*
 * HackkApp — Integration wrapper for the deauth/HydraTFT subsystem.
 *
 * Called from the main IoT_Micro project to:
 *   1. init()   — set up WiFi in APSTA mode, hand the shared TFT to hackk,
 *                  initialise the deauth UI, and draw the splash screen.
 *   2. update() — poll input, dispatch to the deauth UI, and process
 *                  attack status events.  Called repeatedly from loop().
 *   3. shouldExit() — returns true when the user presses BACK from the
 *                     main HydraTFT menu, signalling that we should
 *                     return to the IoT_Micro home screen.
 */
namespace HackkApp {
    void init(Adafruit_ST7735 &tft);
    void update(Adafruit_ST7735 &tft);
    bool shouldExit();
}

#endif // HACKK_APP_H
