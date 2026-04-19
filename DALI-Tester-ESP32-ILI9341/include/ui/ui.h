/**
 * @file ui.h
 * @brief UI core functionality - display, encoder, navigation
 */

#ifndef UI_H
#define UI_H

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include "config.h"

// =============================================================================
// Screen IDs
// =============================================================================
enum ScreenId {
    SCREEN_HOME,
    SCREEN_SCAN_DEVICES,
    SCREEN_SCAN_DEVICE_INFO,    // Device info/locate context menu
    SCREEN_ADDRESS_DEVICE,
    SCREEN_CLONE_CONFIG,
    SCREEN_RESET_DEVICE,
    SCREEN_DALI_PSU,
    SCREEN_SEND_CMD,
    SCREEN_BROADCAST_CTRL,      // Broadcast dimmer control
    SCREEN_LOCATE,
    SCREEN_BUS_MONITOR,
    SCREEN_DEVICE_PARAMS,       // Device parameters (groups, scenes, config)
    SCREEN_CONFIRM_POPUP,
    SCREEN_PONG,
    SCREEN_COUNT
};

// =============================================================================
// Button Event Types
// =============================================================================
enum ButtonEvent {
    BTN_NONE,
    BTN_CLICK,          // Short press < 700ms
    BTN_LONG_PRESS      // Long press >= 700ms
};

// =============================================================================
// Encoder State
// =============================================================================
struct EncoderState {
    volatile int32_t position;
    volatile int32_t lastPosition;
    int32_t delta;              // Steps since last read (after division)
    volatile uint8_t encState;  // Quadrature state machine
    
    // Button state
    volatile bool btnPressed;
    volatile uint32_t btnPressTime;
    volatile bool btnReleased;
    volatile bool btnLongPressTriggered;
    ButtonEvent pendingEvent;
};

// =============================================================================
// Menu Item
// =============================================================================
struct MenuItem {
    const char* label;
    ScreenId targetScreen;
    void (*action)();
};

// =============================================================================
// UI State
// =============================================================================
struct UIState {
    ScreenId currentScreen;
    ScreenId previousScreen;
    int16_t scrollOffset;
    int16_t selectedIndex;
    int16_t maxItems;
    bool needsFullRedraw;
    bool needsPartialRedraw;
    uint32_t lastUpdateMs;
    
    // Popup state
    const char* popupTitle;
    const char* popupMessage;
    bool popupResult;
    void (*popupCallback)(bool);
    ScreenId popupReturnPrevious;  // saved previousScreen before popup
};

// =============================================================================
// External instances
// =============================================================================
extern Adafruit_ILI9341 tft;
extern EncoderState encoder;
extern UIState ui;

// =============================================================================
// Initialization
// =============================================================================
void uiInit();
void encoderInit();

// =============================================================================
// Encoder Functions
// =============================================================================
void encoderUpdate();
int16_t encoderGetDelta();
ButtonEvent encoderGetButtonEvent();
void IRAM_ATTR encoderISR();

// =============================================================================
// Display Functions
// =============================================================================
void uiClearContent();
void uiDrawHeader(const char* title);
void uiDrawFooter(const char* hint);
void uiDrawStatusBadge();
void uiDrawPsuBadge();
void uiDrawBatteryBadge();
void uiDrawBadge(int16_t x, int16_t y, const char* text, uint16_t bgColor, uint16_t textColor);

// =============================================================================
// Battery Monitoring
// =============================================================================
void batteryInit();
void batteryUpdate();
uint8_t batteryGetPercent();
float batteryGetVoltage();

// Text utilities
void uiDrawText(int16_t x, int16_t y, const char* text, uint16_t color = COLOR_TEXT, uint8_t size = 1);
void uiDrawTextCentered(int16_t y, const char* text, uint16_t color = COLOR_TEXT, uint8_t size = 1);
void uiDrawTextRight(int16_t y, const char* text, uint16_t color = COLOR_TEXT, uint8_t size = 1);

// List/Menu helpers
void uiDrawMenuItem(int16_t index, const char* label, bool selected, int16_t scrollOffset = 0);
void uiDrawListItem(int16_t index, const char* label, bool selected, int16_t scrollOffset = 0);
void uiDrawScrollbar(int16_t totalItems, int16_t visibleItems, int16_t scrollOffset);
int16_t uiGetVisibleItems();
int16_t uiGetContentHeight();
int16_t uiGetContentY();

// Value editors
void uiDrawField(int16_t y, const char* label, const char* value, bool selected, bool editing = false);
void uiDrawFieldInt(int16_t y, const char* label, int value, bool selected, bool editing = false);
void uiDrawButton(int16_t y, const char* label, bool selected);
void uiDrawProgressBar(int16_t y, int progress, int total, const char* label = nullptr);

// =============================================================================
// Screen Management
// =============================================================================
void uiSetScreen(ScreenId screen);
void uiGoBack();
void uiShowPopup(const char* title, const char* message, void (*callback)(bool));
void uiUpdate();

// =============================================================================
// Utility Functions
// =============================================================================
int16_t uiClamp(int16_t value, int16_t min, int16_t max);
int16_t uiWrap(int16_t value, int16_t min, int16_t max);

#endif // UI_H
