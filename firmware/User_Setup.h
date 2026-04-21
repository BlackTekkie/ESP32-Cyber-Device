// TFT_eSPI User Setup for ESP32 Cyber Device
// ILI9341 2.8" TFT with XPT2046 touch
// Pin definitions from PCB brief

#define USER_SETUP_ID 1

// Driver
#define ILI9341_DRIVER

// Pins
#define TFT_CS   17
#define TFT_DC   16
#define TFT_RST  -1   // tied to 3.3V on PCB — no reset pin needed
#define TFT_MOSI 23
#define TFT_SCLK 18
#define TFT_MISO 19
#define TFT_BL   32   // backlight — controlled by firmware

// Touch (XPT2046 shares SPI bus)
#define TOUCH_CS 21

// SPI frequency
#define SPI_FREQUENCY       40000000
#define SPI_READ_FREQUENCY  20000000
#define SPI_TOUCH_FREQUENCY  2500000

// Fonts to include
#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_FONT6
#define LOAD_FONT7
#define LOAD_FONT8
#define LOAD_GFXFF
#define SMOOTH_FONT
