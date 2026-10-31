#include "NesPalette.h"
#include "core/ppu_palettes.h"

uint16_t NesPalette::s_enhancedPalette[8][64];
int NesPalette::s_satBoostPct = NES_SATURATION_BOOST_PCT;
int NesPalette::s_conBoostPct = NES_CONTRAST_BOOST_PCT;
int NesPalette::s_expReducePct = NES_EXPOSURE_REDUCE_PCT;
bool NesPalette::s_initialized = false;

uint16_t NesPalette::enhanceColor(uint16_t rgb565, int satBoostPct, int conBoostPct, int expReducePct) {
    // Pure black remains pure black
    if (rgb565 == 0x0000) return 0x0000;
    // Pure white remains pure white (preserves sharp HUD text and cloud whites)
    if (rgb565 == 0xFFFF) return 0xFFFF;

    // 1. Unpack RGB565 to 0..255 (exact integer midpoint rounding)
    uint32_t r5 = (rgb565 >> 11) & 0x1F;
    uint32_t g6 = (rgb565 >> 5) & 0x3F;
    uint32_t b5 = rgb565 & 0x1F;

    int32_t r = (int32_t)((r5 * 255 + 15) / 31);
    int32_t g = (int32_t)((g6 * 255 + 31) / 63);
    int32_t b = (int32_t)((b5 * 255 + 15) / 31);

    // 2. Exposure reduction: scale RGB channels down slightly (cuts TFT backlight bleed / wash-out)
    if (expReducePct > 0) {
        r = (r * (100 - expReducePct)) / 100;
        g = (g * (100 - expReducePct)) / 100;
        b = (b * (100 - expReducePct)) / 100;
    }

    // 3. Perceptual luminance (Rec. 601 integer coefficients: 77, 150, 29 / 256)
    int32_t lum = (77 * r + 150 * g + 29 * b) >> 8;

    // 4. Saturation adjustment around luminance (S = 1.0 + satBoostPct / 100)
    int32_t r_sat = lum + ((r - lum) * (100 + satBoostPct)) / 100;
    int32_t g_sat = lum + ((g - lum) * (100 + satBoostPct)) / 100;
    int32_t b_sat = lum + ((b - lum) * (100 + satBoostPct)) / 100;

    // 5. Contrast adjustment around midpoint 128
    int32_t r_con = 128 + ((r_sat - 128) * (100 + conBoostPct)) / 100;
    int32_t g_con = 128 + ((g_sat - 128) * (100 + conBoostPct)) / 100;
    int32_t b_con = 128 + ((b_sat - 128) * (100 + conBoostPct)) / 100;

    // 6. Clamping to [0..255]
    if (r_con < 0) r_con = 0; else if (r_con > 255) r_con = 255;
    if (g_con < 0) g_con = 0; else if (g_con > 255) g_con = 255;
    if (b_con < 0) b_con = 0; else if (b_con > 255) b_con = 255;

    // 7. Quantize back to RGB565 with midpoint rounding
    uint32_t r5_out = (uint32_t)((r_con * 31 + 127) / 255);
    uint32_t g6_out = (uint32_t)((g_con * 63 + 127) / 255);
    uint32_t b5_out = (uint32_t)((b_con * 31 + 127) / 255);

    return (uint16_t)((r5_out << 11) | (g6_out << 5) | b5_out);
}

void NesPalette::init(int satBoostPct, int conBoostPct, int expReducePct) {
    s_satBoostPct = satBoostPct;
    s_conBoostPct = conBoostPct;
    s_expReducePct = expReducePct;

    // Precompute the entire 8x64 enhanced palette table
    for (int e = 0; e < 8; e++) {
        for (int c = 0; c < 64; c++) {
            uint16_t orig = palette_NTSC565[e][c];
            s_enhancedPalette[e][c] = enhanceColor(orig, satBoostPct, conBoostPct, expReducePct);
        }
    }
    s_initialized = true;

    Serial.printf("[NesPalette] Initialized: Saturation=+%d%%, Contrast=+%d%%, Exposure=-%d%% (512-entry LUT ready)\n",
                  s_satBoostPct, s_conBoostPct, s_expReducePct);
}

const uint16_t (*NesPalette::getEnhancedPaletteArray())[64] {
    if (!s_initialized) init();
    return s_enhancedPalette;
}

const uint16_t* NesPalette::getEnhancedPalette(uint8_t emphasis) {
    if (!s_initialized) init();
    return s_enhancedPalette[emphasis & 7];
}

const uint16_t* NesPalette::getOriginalPalette(uint8_t emphasis) {
    return palette_NTSC565[emphasis & 7];
}
