// Custom Retro-Go target for ESP32-S3 N16R8 + GMT024-08-SPI8P V1.3 (ST7789V IPS).

// Target definition
#define RG_TARGET_NAME              "GMT024-S3"

// Built-in microSD slot: native SDMMC in 1-bit mode.
// The board labels these pins SD_CLK=GPIO39, SD_CMD=GPIO38, SD_DATA/D0=GPIO40.
#define RG_STORAGE_ROOT             "/sd"
#define RG_STORAGE_SDMMC_HOST       SDMMC_HOST_SLOT_1
#define RG_STORAGE_SDMMC_SPEED      SDMMC_FREQ_DEFAULT
#define RG_GPIO_SDSPI_CLK           GPIO_NUM_39
#define RG_GPIO_SDSPI_CMD           GPIO_NUM_38
#define RG_GPIO_SDSPI_D0            GPIO_NUM_40

// External I2S amplifier; ESP32-S3 has no internal analog DAC.
#define RG_AUDIO_USE_INT_DAC        0
#define RG_AUDIO_USE_EXT_DAC        1

// Video: rotate the native 240x320 panel into Retro-Go's 320x240 landscape UI.
#define RG_SCREEN_DRIVER            0
#define RG_SCREEN_HOST              SPI2_HOST
#define RG_SCREEN_SPEED             SPI_MASTER_FREQ_40M
#define RG_DISPLAY_DEFAULT_FILTER   RG_DISPLAY_FILTER_OFF
#define RG_DISABLE_SNES             1
#define RG_SCREEN_BACKLIGHT         0
#define RG_SCREEN_WIDTH             320
#define RG_SCREEN_HEIGHT            240
#define RG_SCREEN_ROTATE            0
#define RG_SCREEN_ROTATION          5
#define RG_SCREEN_RGB_BGR           0
#define RG_SCREEN_RESET_DELAY_MS    150
#define RG_SCREEN_SLEEP_DELAY_MS    120
#define RG_SCREEN_VISIBLE_AREA      {0, 0, 0, 0}
#define RG_SCREEN_SAFE_AREA         {0, 0, 0, 0}

// ST7789V generic sequence matching Adafruit_ST7789.init(240, 320), adapted
// to Retro-Go's 320x240 landscape coordinate space.
#define RG_SCREEN_INIT()                                              \
    ILI9341_CMD(0x11);                    /* Sleep out */             \
    rg_usleep(120 * 1000);                                            \
    ILI9341_CMD(0x3A, 0x55);              /* RGB565 */                \
    rg_usleep(10 * 1000);                                             \
    ILI9341_CMD(0x36, 0xA0);              /* MY | MV, RGB */          \
    ILI9341_CMD(0x2A, 0x00, 0x00, 0x01, 0x3F); /* X: 0..319 */       \
    ILI9341_CMD(0x2B, 0x00, 0x00, 0x00, 0xEF); /* Y: 0..239 */       \
    ILI9341_CMD(0x21);                    /* Inversion on */          \
    rg_usleep(10 * 1000);                                             \
    ILI9341_CMD(0x13);                    /* Normal display mode */   \
    rg_usleep(10 * 1000);                                             \
    ILI9341_CMD(0x29);                    /* Display on */            \
    rg_usleep(20 * 1000);

// Buttons are active-low: each switch connects its GPIO to GND.
#define RG_GAMEPAD_GPIO_MAP {\
    {RG_KEY_UP,     .num = GPIO_NUM_42, .pullup = 1, .level = 0},\
    {RG_KEY_DOWN,   .num = GPIO_NUM_2,  .pullup = 1, .level = 0},\
    {RG_KEY_LEFT,   .num = GPIO_NUM_1,  .pullup = 1, .level = 0},\
    {RG_KEY_RIGHT,  .num = GPIO_NUM_41, .pullup = 1, .level = 0},\
    {RG_KEY_A,      .num = GPIO_NUM_21, .pullup = 1, .level = 0},\
    {RG_KEY_B,      .num = GPIO_NUM_47, .pullup = 1, .level = 0},\
    {RG_KEY_START,  .num = GPIO_NUM_48, .pullup = 1, .level = 0},\
    {RG_KEY_SELECT, .num = GPIO_NUM_45, .pullup = 1, .level = 0},\
}

// Eight physical buttons leave no dedicated menu/option keys.
#define RG_GAMEPAD_VIRT_MAP {\
    {RG_KEY_MENU,   .src = RG_KEY_START | RG_KEY_SELECT},\
    {RG_KEY_OPTION, .src = RG_KEY_SELECT | RG_KEY_A},\
}

#define RG_RECOVERY_BTN             RG_KEY_DOWN
#define RG_BATTERY_DRIVER           0

// GMT024 SPI: CS is driven by GPIO14 and BL is permanently tied to 3V3.
#define RG_GPIO_LCD_MISO            GPIO_NUM_NC
#define RG_GPIO_LCD_MOSI            GPIO_NUM_11
#define RG_GPIO_LCD_CLK             GPIO_NUM_10
#define RG_GPIO_LCD_CS              GPIO_NUM_14
#define RG_GPIO_LCD_DC              GPIO_NUM_13
#define RG_GPIO_LCD_RST             GPIO_NUM_12

// External I2S amplifier: BCLK, LRC/WS and audio data input.
#define RG_GPIO_SND_I2S_BCK         GPIO_NUM_46
#define RG_GPIO_SND_I2S_WS          GPIO_NUM_9
#define RG_GPIO_SND_I2S_DATA        GPIO_NUM_3

#define RG_UPDATER_ENABLE           0
