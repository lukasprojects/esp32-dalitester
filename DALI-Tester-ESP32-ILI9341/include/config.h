/**
 * @file config.h
 * @brief Hardware pin configuration and settings for DALI Tester
 * 
 * Modify these defines to match your hardware setup.
 */

#ifndef CONFIG_H
#define CONFIG_H

// =============================================================================
// Firmware Version
// =============================================================================
#define FW_VERSION  "1.3.8"
#define HW_VERSION  "1.1"

// =============================================================================
// TFT Display Configuration (ILI9341 2.8" SPI)
// =============================================================================
#define TFT_CS      5           // Chip Select
#define TFT_DC      4           // Data/Command
#define TFT_RST     17          // Reset (-1 if connected to ESP32 EN/RST)
#define TFT_MOSI    23          // SPI MOSI (default ESP32 VSPI)
#define TFT_SCLK    18          // SPI SCLK (default ESP32 VSPI)
#define TFT_MISO    19          // SPI MISO (optional for touch)
#define TFT_LED     2           // Backlight control (HIGH = on)

// Display settings
#define TFT_WIDTH   320
#define TFT_HEIGHT  240
#define TFT_ROTATION 1      // Landscape mode

// =============================================================================
// Rotary Encoder Configuration
// =============================================================================
#define ENC_A       33      // Encoder A signal
#define ENC_B       32      // Encoder B signal
#define ENC_BTN     34      // Encoder push button (active LOW with external pullup)

#define ENCODER_DIR     -1   // Direction: +1 or -1 to reverse
#define ENC_STEP_DIV    4   // Steps per detent (typical: 4 for most encoders)

// Button timing
#define BTN_DEBOUNCE_MS     50      // Debounce time in ms
#define LONG_PRESS_MS       700     // Long press threshold in ms

// =============================================================================
// DALI Interface Configuration
// =============================================================================
#define DALI_TX     26      // DALI transmit pin
#define DALI_RX     27      // DALI receive pin

// TX/RX inversion settings (depends on your optocoupler/transistor circuit)
// For Waveshare Pico-DALI2: Try both polarities if one doesn't work
#define DALI_TX_INVERT  true    // true = TX HIGH pulls bus LOW (opto-coupler style)
#define DALI_RX_INVERT  false   // false for Waveshare Pico-DALI2

// =============================================================================
// DALI PSU Control
// =============================================================================
#define DALI_PSU_EN         25      // PSU enable pin
#define PSU_EN_ACTIVE_HIGH  true    // true if HIGH enables PSU

// =============================================================================
// UI Configuration
// =============================================================================
#define HEADER_HEIGHT       32      // Header bar height in pixels (larger for better badge visibility)
#define FOOTER_HEIGHT       20      // Footer bar height in pixels
#define CONTENT_TEXT_SIZE   2       // Text size for content area (1=6x8, 2=12x16)
#define MENU_ITEM_HEIGHT    24      // Height per menu item (adjust for text size)

// Colors (RGB565)
#define COLOR_BG            0x0000  // Black background
#define COLOR_TEXT          0xFFFF  // White text
#define COLOR_HEADER_BG     0x001F  // Blue header
#define COLOR_FOOTER_BG     0x0010  // Dark blue footer
#define COLOR_HIGHLIGHT     0x07E0  // Green highlight
#define COLOR_SELECTED      0x07FF  // Cyan selection
#define COLOR_WARNING       0xFBE0  // Orange warning
#define COLOR_ERROR         0xF800  // Red error
#define COLOR_SUCCESS       0x07E0  // Green success
#define COLOR_BADGE_IDLE    0x4208  // Gray badge
#define COLOR_BADGE_ACT     0x07E0  // Green - activity
#define COLOR_BADGE_SEND    0xFFE0  // Yellow - sending
#define COLOR_BADGE_ERR     0xF800  // Red - error

// =============================================================================
// Bus Monitor Configuration
// =============================================================================
#define BUSMON_MAX_ENTRIES  80      // Ring buffer size for bus monitor
#define BUSMON_REFRESH_MS   80      // Max refresh rate (~12fps)

// =============================================================================
// Locate Function
// =============================================================================
#define LOCATE_INTERVAL_MIN     200     // Minimum interval in ms
#define LOCATE_INTERVAL_MAX     3000    // Maximum interval in ms
#define LOCATE_INTERVAL_STEP    100     // Step size in ms
#define LOCATE_INTERVAL_DEFAULT 1000     // Default interval

// =============================================================================
// DALI Timing
// =============================================================================
#define DALI_TIMER_US   104     // Timer interrupt interval in microseconds

// =============================================================================
// Battery Monitoring (1S LiPo)
// =============================================================================
#define BATTERY_ADC_PIN     35      // GPIO 35 (ADC1_CH7, Input only)

// Voltage divider: Connect battery through voltage divider to this pin
// For 1S LiPo (4.2V max): Use 2:1 divider (e.g., 100k + 100k)
//   Battery+ ---[100k]---+---[100k]--- GND
//                        |
//                     GPIO 34
//
// Calibration values (adjust based on your resistors)
#define BATTERY_DIVIDER_RATIO   2.0     // Voltage divider ratio (R1+R2)/R2
#define BATTERY_VOLT_MIN        3.0     // Empty battery voltage (0%)
#define BATTERY_VOLT_MAX        4.2     // Full battery voltage (100%)
#define BATTERY_READ_INTERVAL   5000    // Read interval in ms
#define BATTERY_ADC_SAMPLES     16      // Number of samples to average

// ADC calibration (ESP32 ADC can be non-linear, adjust if needed)
#define BATTERY_ADC_VREF        3.3     // ADC reference voltage (with 11dB attenuation)

#endif // CONFIG_H
