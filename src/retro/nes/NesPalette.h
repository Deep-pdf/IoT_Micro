#ifndef NES_PALETTE_H
#define NES_PALETTE_H

#include <stdint.h>
#include <Arduino.h>

// ==============================================================================
// Configurable NES Color Enhancement Settings
// Tuned for ST7735 TFT: reduced exposure for deep retro tones, bold vivid saturation
// ==============================================================================
#ifndef NES_SATURATION_BOOST_PCT
#define NES_SATURATION_BOOST_PCT 50  // +50% saturation boost (bold, vivid, punchy colors)
#endif

#ifndef NES_CONTRAST_BOOST_PCT
#define NES_CONTRAST_BOOST_PCT 8     // +8% contrast boost
#endif

#ifndef NES_EXPOSURE_REDUCE_PCT
#define NES_EXPOSURE_REDUCE_PCT 12   // -12% exposure reduction (eliminates backlight wash-out)
#endif

class NesPalette {
public:
    // Build the enhanced 8x64 palette lookup tables from the base NTSC565 palette
    static void init(int satBoostPct = NES_SATURATION_BOOST_PCT,
                     int conBoostPct = NES_CONTRAST_BOOST_PCT,
                     int expReducePct = NES_EXPOSURE_REDUCE_PCT);

    // Returns pointer to 8x64 enhanced RGB565 palette array (usable directly by PPU)
    static const uint16_t (*getEnhancedPaletteArray())[64];

    // Returns pointer to 64-entry enhanced RGB565 palette for specific emphasis (0..7)
    static const uint16_t* getEnhancedPalette(uint8_t emphasis = 0);

    // Returns pointer to 64-entry original un-enhanced RGB565 palette for specific emphasis (0..7)
    static const uint16_t* getOriginalPalette(uint8_t emphasis = 0);

    // Integer-based perceptual color enhancement for single RGB565 value
    static uint16_t enhanceColor(uint16_t rgb565,
                                int satBoostPct = NES_SATURATION_BOOST_PCT,
                                int conBoostPct = NES_CONTRAST_BOOST_PCT,
                                int expReducePct = NES_EXPOSURE_REDUCE_PCT);

    // Get active configuration values
    static int getSaturationBoost() { return s_satBoostPct; }
    static int getContrastBoost()   { return s_conBoostPct; }
    static int getExposureReduce()  { return s_expReducePct; }

private:
    static uint16_t s_enhancedPalette[8][64];
    static int s_satBoostPct;
    static int s_conBoostPct;
    static int s_expReducePct;
    static bool s_initialized;
};

#endif // NES_PALETTE_H
