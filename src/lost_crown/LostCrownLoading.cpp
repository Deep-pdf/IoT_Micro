/**
 * LostCrownLoading.cpp
 *
 * Full pixel-art recreation of the Lost Crown loading screen for the
 * 128×160 ST7735 TFT display.
 *
 * Scene breakdown (matching reference image):
 *   Sky (0–74):   Deep navy, cloud dithering, crescent moon (centre)
 *   Villain      : Oversized crowned silhouette upper-right integrated into sky
 *   Temple/Palace: Illuminated Varanasi-ghat architecture (y 60–115)
 *   River        : Reflective dark water with floating diyas (y 115–130)
 *   Foreground   : Stone ghat steps, hero lower-left with red scarf (y 120–145)
 *   UI strip     : "LOADING..." label + gold ornamental progress bar + taglines
 */

#include "LostCrownLoading.h"
#include <Adafruit_GFX.h>

// ─── Timing ──────────────────────────────────────────────────────────────────
static constexpr uint32_t LOAD_DURATION_MS = 4000;
static constexpr uint32_t COMPLETE_HOLD_MS = 500;

static uint32_t  startedAt   = 0;
static int       lastProgress = -1;

// ─── Palette (colour565 helpers) ─────────────────────────────────────────────
static Adafruit_ST7735 *_tft = nullptr;  // set in begin()

inline uint16_t C(uint8_t r, uint8_t g, uint8_t b) {
  return _tft->color565(r, g, b);
}

// Sky / atmosphere
#define SKY_DEEP     C(  6, 12, 32)   // deepest navy
#define SKY_MID      C( 12, 24, 58)   // mid sky
#define SKY_CLOUD    C( 22, 38, 78)   // cloud blue
#define MOON_WHITE   C(220,230,235)   // moon face
#define MOON_GREY    C(165,185,192)   // moon shadow rim

// Architecture
#define TEMPLE_DARK  C( 18, 24, 44)   // silhouette
#define TEMPLE_MID   C( 34, 40, 68)   // secondary stone
#define TORCH_AMBER  C(230,100, 18)   // torch/window light
#define TORCH_GOLD   C(255,177, 36)   // bright torch
#define TORCH_GLOW   C(180, 60,  0)   // warm halo
#define STONE_GREY   C( 55, 52, 72)   // step stone
#define STONE_LT     C( 70, 66, 88)   // lit stone edge

// Water / river
#define WATER_DEEP   C(  8, 16, 44)   // river base
#define WATER_REFL   C( 16, 32, 80)   // reflection stripe
#define DIYA_GOLD    C(255,200, 60)   // floating lamp
#define DIYA_GLOW    C(200,120, 30)   // lamp halo

// Villain
#define VIL_INK      C(  4,  6, 16)   // body silhouette
#define VIL_CROWN    C( 80, 42, 20)   // crown earthy gold
#define VIL_EYE      C(200, 20, 20)   // red glowing eye
#define VIL_CAPE     C( 55, 14, 22)   // dark red cape

// Hero
#define HERO_SKIN    C(160, 96, 52)   // skin
#define HERO_HAIR    C( 12,  8,  4)   // hair
#define HERO_SCARF   C(185, 35, 35)   // red scarf
#define HERO_SCARF2  C(100, 18, 24)   // scarf shadow
#define HERO_CLOTH   C(210,190,145)   // off-white clothing
#define HERO_BOOTS   C( 85, 52, 28)   // boots

// UI
#define UI_BLACK     C(  5,  2,  1)   // bar trough
#define GOLD_HI      C(245,196, 48)   // bright gold
#define GOLD_MID     C(210,140, 20)   // mid gold
#define GOLD_LO      C(130, 76,  8)   // dark gold
#define AMBER_BAR    C(220, 95, 18)   // bar fill
#define AMBER_HI     C(255,175, 50)   // bar highlight
#define TXT_GOLD     C(240,185, 40)   // "LOADING" text
#define TXT_CREAM    C(200,190,175)   // subtitle cream
#define TXT_DIM      C(120,112,100)   // dim tagline

// ─── Utility helpers ──────────────────────────────────────────────────────────

static void hline(int16_t x, int16_t y, int16_t len, uint16_t col) {
  _tft->drawFastHLine(x, y, len, col);
}
static void vline(int16_t x, int16_t y, int16_t len, uint16_t col) {
  _tft->drawFastVLine(x, y, len, col);
}
static void rect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t col) {
  _tft->fillRect(x, y, w, h, col);
}
static void px(int16_t x, int16_t y, uint16_t col) {
  _tft->drawPixel(x, y, col);
}
static void tri(int16_t x0,int16_t y0,int16_t x1,int16_t y1,int16_t x2,int16_t y2, uint16_t col) {
  _tft->fillTriangle(x0,y0,x1,y1,x2,y2,col);
}

// ─── Sky layer ────────────────────────────────────────────────────────────────
static void drawSky() {
  // Gradient: deepest at top, slightly lighter toward horizon
  for (int y = 0; y < 75; y++) {
    uint8_t t = y * 2;
    uint16_t c = C(6 + t/8, 12 + t/4, 32 + t/3);
    hline(0, y, 128, c);
  }

  // ── Dithered clouds ──
  // cloud band 1 (upper)
  for (int x = 0; x < 128; x += 16) {
    rect(x,     8, 12, 2, SKY_CLOUD);
    rect(x + 2, 7,  8, 1, SKY_CLOUD);
  }
  // cloud band 2
  for (int x = 4; x < 128; x += 20) {
    rect(x,     20, 14, 2, SKY_CLOUD);
    rect(x + 3, 19,  8, 1, SKY_CLOUD);
  }
  // cloud band 3 (near moon, diffuse)
  for (int x = 0; x < 52; x += 15) {
    rect(x, 28, 10, 2, C(20,34,68));
    rect(x + 2, 27, 6, 1, C(20,34,68));
  }
  for (int x = 80; x < 128; x += 13) {
    rect(x, 30, 9, 2, C(20,34,68));
  }
  // star pixels
  const int8_t stars[][2] = {
    {10,4},{40,3},{55,10},{90,5},{110,3},{118,15},{8,16},{30,14},{105,22}
  };
  for (auto &s : stars) px(s[0], s[1], C(180,185,200));
}

// ─── Crescent Moon ───────────────────────────────────────────────────────────
static void drawMoon() {
  // Position: centre-ish slightly right (x=62, y=30), r=10
  _tft->fillCircle(62, 30, 10, MOON_WHITE);
  // Carve out crescent shadow
  _tft->fillCircle(67, 28,  9, C(165,185,192));  // grey rim
  _tft->fillCircle(67, 28,  7, C(20,36,72));      // sky punch-out
}

// ─── Villain silhouette (upper-right, looming large) ─────────────────────────
static void drawVillain() {
  // Body / head — large dark silhouette integrated into sky
  // Head circle  (x=102, y=25, r=18)
  _tft->fillCircle(104, 22, 18, VIL_INK);
  // Body bulk extending down+left
  tri(84,38, 128,40, 128,80, VIL_INK);
  tri(84,38, 100,85, 128,80, VIL_INK);
  rect(90, 38, 38, 42, VIL_INK);

  // Crown — three spiky prongs above the head
  tri(88, 8,  92, 0,  96, 8,  VIL_CROWN);
  tri(96, 8, 101, 0, 105, 8,  VIL_CROWN);
  tri(105,8, 110, 0, 115,10,  VIL_CROWN);
  hline(88, 9, 28, GOLD_MID);          // crown base band
  hline(89,10, 26, GOLD_LO);

  // Crown gem
  rect(98, 12, 5, 3, VIL_EYE);
  px(100,13, C(255,80,80));

  // Glowing red eye
  rect(97, 30, 4, 3, VIL_EYE);
  px(98, 31, C(255,100,100));
  px(99, 30, C(255,150,150));

  // Earring dangle
  px(88, 35, GOLD_MID);
  px(88, 36, GOLD_MID);
  px(87, 37, GOLD_HI);

  // Cape sweep (dark red diagonal across body)
  tri(86,50, 128,55, 128,80, VIL_CAPE);
  tri(86,50, 118,80, 95,80,  C(40,10,18));

  // Moustache/face details
  hline(95,38,8, C(30,14,28));
  hline(96,40,7, C(20,10,20));
}

// ─── Palace / Temple ─────────────────────────────────────────────────────────
static void drawTemple() {
  const uint16_t dusk = C(14,20,42);   // sky tone behind temple

  // Base platform / ghat steps
  rect(0, 110, 128, 50, STONE_GREY);
  // Step terraces going down to water
  rect(0, 112, 128, 2, STONE_LT);
  rect(0, 116, 128, 2, C(48,45,64));
  rect(0, 120, 128, 2, STONE_LT);

  // Main temple background fill
  rect(20, 65, 90, 48, TEMPLE_DARK);

  // ─ Large central dome ─
  _tft->fillCircle(64, 66, 12, TEMPLE_DARK);
  tri(52,66, 64,54, 76,66, TEMPLE_DARK);           // pointed spire base
  rect(62, 48, 4, 10, TORCH_GOLD);                  // flagpole
  rect(62, 47, 3, 2, TORCH_AMBER);                  // flag tip
  // dome highlight stripe
  hline(57, 62, 6, TEMPLE_MID);
  hline(58, 63, 4, C(42,50,80));

  // ─ Central dome windows (warm glow) ─
  for (int xi = 55; xi <= 70; xi += 6) {
    rect(xi, 72, 3, 5, TORCH_AMBER);
    rect(xi+1, 73, 1, 3, C(255,210,120));
  }

  // ─ Left temple towers ─
  // Tower 1 (far left)
  rect( 5, 80, 9, 33, TEMPLE_DARK);
  tri(  5,80,  9,73, 14,80, TEMPLE_DARK);
  rect( 8, 70, 2, 4, TORCH_GOLD);
  rect( 7, 87, 2, 4, TORCH_AMBER);
  px(8,88, C(255,200,80));
  // Tower 2
  rect(17, 74, 10, 39, TEMPLE_DARK);
  tri(17,74, 22,65, 27,74, TEMPLE_DARK);
  rect(21, 63, 2, 6, TORCH_GOLD);
  rect(19, 83, 3, 4, TORCH_AMBER);
  px(20,84, C(255,200,80));
  rect(22, 88, 3, 4, TORCH_AMBER);
  // Tower 3
  rect(30, 70, 9, 43, TEMPLE_DARK);
  tri(30,70, 34,62, 39,70, TEMPLE_DARK);
  rect(33, 59, 2, 7, TORCH_GOLD);
  rect(32, 78, 3, 5, TORCH_AMBER);
  px(33,79, C(255,220,100));

  // ─ Right temple towers ─
  rect(80, 72, 10, 41, TEMPLE_DARK);
  tri(80,72, 85,62, 90,72, TEMPLE_DARK);
  rect(84, 60, 2, 7, TORCH_GOLD);
  rect(82, 82, 3, 4, TORCH_AMBER);
  px(83,83, C(255,200,80));

  rect(92, 76, 9, 37, TEMPLE_DARK);
  tri(92,76, 96,68, 101,76, TEMPLE_DARK);
  rect(95, 65, 2, 6, TORCH_GOLD);
  rect(93, 86, 3, 4, TORCH_AMBER);

  // Far-right minor tower
  rect(108, 82, 7, 31, TEMPLE_DARK);
  tri(108,82, 111,76, 115,82, TEMPLE_DARK);
  rect(110, 73, 2, 6, TORCH_GOLD);

  // Ghat staircase (right side descending to water)
  for (int s = 0; s < 5; s++) {
    int sx = 82 + s*8;
    int sy = 108 + s*2;
    if (sx < 128) rect(sx, sy, 10, 2, C(62,58,80));
  }

  // Torch fires on ghat walls
  const int8_t torchX[] = {10, 28, 45, 80, 96, 113};
  const int8_t torchY[] = {108,106,108,108,107,106};
  for (int i = 0; i < 6; i++) {
    int tx = torchX[i], ty = torchY[i];
    rect(tx, ty,   2, 4, STONE_GREY);       // torch pole
    px(tx,   ty-1, TORCH_AMBER);            // flame base
    px(tx+1, ty-2, TORCH_GOLD);             // flame tip
    px(tx,   ty-2, TORCH_GLOW);
    // halo glow (1-px ring)
    px(tx-1, ty-1, C(80,40,5));
    px(tx+2, ty-1, C(80,40,5));
    px(tx,   ty-3, C(60,30,5));
  }

  // Ambient glow splash under windows
  for (int xi = 55; xi <= 72; xi += 6) {
    px(xi+1, 77, TORCH_GLOW);
  }
}

// ─── River (reflective water + floating diyas) ────────────────────────────────
static void drawRiver() {
  // Water base
  rect(0, 118, 128, 18, WATER_DEEP);

  // Shimmer stripes (horizontal dashes)
  for (int y = 119; y < 135; y += 3) {
    for (int x = (y & 1) ? 3 : 8; x < 128; x += 16) {
      hline(x, y, 8, WATER_REFL);
    }
  }

  // Torch/window reflections on water (vertical streaks)
  const int8_t refX[] = {10,28,45,64,80,96,113};
  for (int rx : refX) {
    for (int ry = 118; ry < 134; ry++) {
      px(rx,   ry, C(60+((ry-118)*4), 30, 2));
      px(rx+1, ry, C(80+((ry-118)*3), 40, 4));
    }
  }

  // Boats — simple pixel silhouettes
  // Boat 1 (centre-left)
  rect(38, 127, 14, 2, C(30,22,38));
  tri(38,129, 45,132, 52,129, C(24,18,30));
  // Boat 2 (centre-right)
  rect(74, 130, 12, 2, C(30,22,38));
  tri(74,132, 80,135, 86,132, C(24,18,30));

  // Floating diyas (clay lamps)
  const uint8_t diyaX[] = {20, 50, 68, 90, 108};
  const uint8_t diyaY[] = {131,128,133,129, 132};
  for (int i = 0; i < 5; i++) {
    int dx = diyaX[i], dy = diyaY[i];
    px(dx,   dy,   DIYA_GOLD);
    px(dx+1, dy,   DIYA_GOLD);
    px(dx,   dy+1, DIYA_GLOW);
    px(dx+1, dy+1, DIYA_GLOW);
    // reflection ripple
    px(dx,   dy+2, C(80,50,10));
    px(dx+1, dy+3, C(50,30,5));
  }
}

// ─── Hero character (lower-left) ─────────────────────────────────────────────
static void drawHero() {
  // Positioning: feet at y≈138, centre at x≈24

  // Boots
  rect(19,133, 6,4, HERO_BOOTS);
  rect(27,133, 6,4, HERO_BOOTS);

  // Legs (white dhoti)
  rect(20,122,12,12, HERO_CLOTH);
  // Belt
  rect(19,121, 14,2, C(80,50,24));

  // Torso
  rect(19,110, 14,12, HERO_CLOTH);

  // Arms
  rect(17,112,  3,8,  HERO_CLOTH);
  rect(33,112,  3,8,  HERO_CLOTH);
  // Hand holding torch (right arm extended forward)
  px(35,116, TORCH_AMBER);
  px(35,115, TORCH_GOLD);
  px(34,114, C(255,200,80));

  // Head
  _tft->fillCircle(26,105, 6, HERO_SKIN);
  // Hair / headband
  rect(20,100, 12,5, HERO_HAIR);
  rect(20,105, 12,3, HERO_HAIR);
  rect(21,104, 10,1, TORCH_AMBER);  // headband strip

  // Red scarf / cape — flowing left-downward
  tri(19,111,  4,104, 19,122, HERO_SCARF);
  tri( 4,104,  2,120, 19,122, HERO_SCARF2);
  // Scarf tail
  tri(19,120,  6,130, 12,138, HERO_SCARF);
  tri(12,138,  3,133,  6,130, HERO_SCARF2);

  // Shadow underfoot
  _tft->drawFastHLine(18,137, 18, C(20,18,28));
}

// ─── Left-side stone foreground wall ─────────────────────────────────────────
static void drawForeground() {
  // Stone platform the hero stands on
  rect(0,138, 50,22, STONE_GREY);
  // mortar lines
  for (int y = 140; y < 160; y += 5) hline(0,y, 50, C(44,40,58));
  for (int x = 8;   x < 50;  x += 12) vline(x,138,22, C(44,40,58));
  // top edge highlight
  hline(0,138,50, STONE_LT);

  // Small red banner flag top-left
  rect( 0,48, 2,22, C(70,40,20));  // flag pole
  tri(  2,48, 14,54,  2,60, C(165,28,28));
}

// ─── Right-side OM flag ───────────────────────────────────────────────────────
static void drawOmFlag() {
  // Flag pole
  rect(123,60, 2,50, C(80,60,30));
  // Red banner
  tri(123,60, 125,60, 123,90, C(160,25,25));
  rect(105,60, 18,30,          C(155,22,22));
  // OM symbol (simplified pixel glyph) — 3×5 at x=110,y=70
  // "OM" as pixel blocks
  rect(110,70, 2,8, C(220,170,50));
  rect(113,70, 5,2, C(220,170,50));
  rect(113,74, 5,2, C(220,170,50));
  rect(113,78, 3,2, C(220,170,50));
  rect(116,76, 2,4, C(220,170,50));
  px(118,74, C(220,170,50));
  // Torch on right side
  rect(120,102, 4,12, STONE_GREY);
  px(121, 101, TORCH_AMBER);
  px(122, 100, TORCH_GOLD);
  px(121, 100, TORCH_GLOW);
  px(122,  99, C(255,220,100));
}

// ─── Top UI: Title "LOST CROWN" ───────────────────────────────────────────────
static void drawTitle() {
  _tft->setFont(NULL);
  _tft->setTextWrap(false);

  // Small crown above title
  tri( 7,9,  10,4,  13,9,  GOLD_HI);
  tri(13,9,  17,3,  20,9,  GOLD_HI);
  tri(20,9,  23,5,  27,9,  GOLD_HI);
  rect( 7,9, 21,4, GOLD_MID);       // crown base
  hline(7,13,20, GOLD_LO);

  // "LOST" — size 2 bold, gold
  _tft->setTextColor(GOLD_HI);
  _tft->setTextSize(2);
  _tft->setCursor(5, 16);
  _tft->print("LOST");
  // shadow
  _tft->setTextColor(C(80,50,5));
  _tft->setCursor(6, 17);
  _tft->print("LOST");
  _tft->setTextColor(GOLD_HI);
  _tft->setCursor(5, 16);
  _tft->print("LOST");

  // "CROWN" — size 1, cream/silver
  _tft->setTextColor(TXT_CREAM);
  _tft->setTextSize(1);
  _tft->setCursor(5, 33);
  _tft->print("C R O W N");

  // Decorative rule under title
  hline( 5,42, 55, GOLD_MID);
  px(5,41, GOLD_HI); px(60,41, GOLD_HI);

  // "A JOURNEY / THROUGH / LEGENDS / AWAITS..." — small italic-look, cream
  _tft->setTextColor(TXT_CREAM);
  _tft->setTextSize(1);
  _tft->setCursor(5, 48); _tft->print("A JOURNEY");
  _tft->setCursor(5, 57); _tft->print("THROUGH");
  _tft->setCursor(5, 66); _tft->print("LEGENDS");
  _tft->setCursor(5, 75); _tft->print("AWAITS...");
}

// ─── LOADING label ────────────────────────────────────────────────────────────
static void drawLoadingLabel() {
  _tft->setFont(NULL);
  _tft->setTextWrap(false);
  _tft->setTextSize(1);

  // Shadow
  _tft->setTextColor(C(50,25,0));
  _tft->setCursor(32,137);
  _tft->print("LOADING...");

  // Bright gold text
  _tft->setTextColor(TXT_GOLD);
  _tft->setCursor(31,136);
  _tft->print("LOADING...");
}

// ─── Tagline text ─────────────────────────────────────────────────────────────
static void drawTaglines() {
  _tft->setFont(NULL);
  _tft->setTextWrap(false);
  _tft->setTextSize(1);

  // Small ornament row
  _tft->setTextColor(GOLD_LO);
  _tft->setCursor(42, 153);
  _tft->print("* * *");

  // "THE CROWN SHALL RETURN" — dim cream, tiny
  _tft->setTextColor(TXT_DIM);
  _tft->setCursor(8, 153);   // only fits if we drop size — shift to align
  // Print a single-char width compressed version
  _tft->setCursor(9, 153);
  _tft->print("CROWN SHALL RETURN");
}

// ─── Loading bar ─────────────────────────────────────────────────────────────
static void drawLoadingBar(int progress) {
  // Bar rect: x=10, y=144, w=108, h=8
  const int16_t BX=10, BY=144, BW=108, BH=8;

  // Outer gold border (2-tone bevel)
  // Top highlight
  _tft->drawFastHLine(BX-1, BY-1, BW+2, GOLD_HI);
  _tft->drawFastVLine(BX-1, BY-1, BH+2, GOLD_HI);
  // Bottom shadow
  _tft->drawFastHLine(BX-1, BY+BH, BW+2, GOLD_LO);
  _tft->drawFastVLine(BX+BW, BY-1, BH+2, GOLD_LO);

  // Corner diamonds (pixel)
  px(BX-3, BY+BH/2,   GOLD_HI);
  px(BX-2, BY+BH/2-1, GOLD_MID);
  px(BX-2, BY+BH/2+1, GOLD_MID);
  px(BX+BW+2, BY+BH/2,   GOLD_HI);
  px(BX+BW+1, BY+BH/2-1, GOLD_MID);
  px(BX+BW+1, BY+BH/2+1, GOLD_MID);

  // Dark trough
  rect(BX, BY, BW, BH, UI_BLACK);

  // Fill
  int filled = (BW * progress) / 100;
  if (filled > 0) {
    // Base amber gradient (two horizontal bands)
    rect(BX, BY,       filled, BH/2, AMBER_HI);   // top brighter half
    rect(BX, BY+BH/2,  filled, BH/2, AMBER_BAR);  // bottom darker half

    // Pixel column texture (every 4 px a 1-px dark seam)
    for (int sx = BX+3; sx < BX+filled; sx += 4) {
      vline(sx, BY+1, BH-2, C(90,40,5));
    }

    // Bright highlight strip on very top
    if (filled > 2)
      hline(BX+1, BY+1, filled-2, GOLD_HI);
  }

  // Inner border outline over everything
  _tft->drawRect(BX, BY, BW, BH, GOLD_MID);
}

// ─── Full static scene draw ──────────────────────────────────────────────────
static void drawScene() {
  _tft->fillScreen(SKY_DEEP);
  drawSky();
  drawMoon();
  drawVillain();
  drawTemple();
  drawRiver();
  drawForeground();
  drawOmFlag();
  drawHero();
  drawTitle();
  drawLoadingLabel();
  drawTaglines();
}

// ─── Public API ──────────────────────────────────────────────────────────────
namespace LostCrownLoading {

void begin(Adafruit_ST7735 &tft) {
  _tft = &tft;
  startedAt    = millis();
  lastProgress = -1;
  drawScene();
  drawLoadingBar(0);
  lastProgress = 0;
}

bool update(Adafruit_ST7735 &tft) {
  _tft = &tft;
  const uint32_t elapsed  = millis() - startedAt;
  // Natural easing: use a slight ease-in-out curve so the bar
  // doesn't feel mechanical. progress = ease(t/T) * 100
  const uint32_t T        = LOAD_DURATION_MS;
  int progress;
  if (elapsed >= T) {
    progress = 100;
  } else {
    // Ease-in-out cubic:  f(x) = x<0.5 ? 4x³ : 1-(-2x+2)³/2
    float x = (float)elapsed / T;
    float f;
    if (x < 0.5f) f = 4.0f * x * x * x;
    else {
      float t = -2.0f*x + 2.0f;
      f = 1.0f - (t*t*t) / 2.0f;
    }
    progress = (int)(f * 100.0f);
  }

  if (progress != lastProgress) {
    drawLoadingBar(progress);
    lastProgress = progress;
  }
  return elapsed >= T + COMPLETE_HOLD_MS;
}

} // namespace LostCrownLoading
