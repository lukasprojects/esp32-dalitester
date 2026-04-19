/**
 * @file ui.cpp
 * @brief UI core implementation - display, encoder, navigation
 */

#include "ui/ui.h"
#include "dali/dali_interface.h"

// =============================================================================
// Display Instance
// =============================================================================
Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_RST);

// =============================================================================
// Encoder State
// =============================================================================
EncoderState encoder;
UIState ui;

// Quadrature encoder transition table
// State: AB (old) -> AB (new)
// Returns: 0=invalid, +1=CW, -1=CCW
static const int8_t encTable[16] PROGMEM = {
     0, -1,  1,  0,
     1,  0,  0, -1,
    -1,  0,  0,  1,
     0,  1, -1,  0
};

// =============================================================================
// Encoder ISR
// =============================================================================
void IRAM_ATTR encoderISR() {
    uint8_t a = digitalRead(ENC_A);
    uint8_t b = digitalRead(ENC_B);
    
    uint8_t newState = (a << 1) | b;
    uint8_t combined = (encoder.encState << 2) | newState;
    encoder.encState = newState;
    
    int8_t delta = pgm_read_byte(&encTable[combined]);
    encoder.position += delta * ENCODER_DIR;
}

// =============================================================================
// Encoder Initialization
// =============================================================================
void encoderInit() {
    pinMode(ENC_A, INPUT_PULLUP);
    pinMode(ENC_B, INPUT_PULLUP);
    pinMode(ENC_BTN, INPUT);  // External pullup
    
    encoder.position = 0;
    encoder.lastPosition = 0;
    encoder.delta = 0;
    encoder.encState = (digitalRead(ENC_A) << 1) | digitalRead(ENC_B);
    encoder.btnPressed = false;
    encoder.btnPressTime = 0;
    encoder.btnReleased = false;
    encoder.btnLongPressTriggered = false;
    encoder.pendingEvent = BTN_NONE;
    
    // Attach interrupts for encoder
    attachInterrupt(digitalPinToInterrupt(ENC_A), encoderISR, CHANGE);
    attachInterrupt(digitalPinToInterrupt(ENC_B), encoderISR, CHANGE);
}

// =============================================================================
// Encoder Update (call in loop)
// =============================================================================
void encoderUpdate() {
    // Calculate delta (with step division)
    int32_t rawDelta = encoder.position - encoder.lastPosition;
    encoder.delta = rawDelta / ENC_STEP_DIV;
    
    if (encoder.delta != 0) {
        encoder.lastPosition += encoder.delta * ENC_STEP_DIV;
    }
    
    // Button handling with debounce
    static uint32_t lastDebounceTime = 0;
    static bool lastBtnState = HIGH;
    bool currentBtnState = digitalRead(ENC_BTN);
    
    if (currentBtnState != lastBtnState) {
        lastDebounceTime = millis();
    }
    
    if ((millis() - lastDebounceTime) > BTN_DEBOUNCE_MS) {
        if (currentBtnState == LOW && !encoder.btnPressed) {
            // Button just pressed
            encoder.btnPressed = true;
            encoder.btnPressTime = millis();
            encoder.btnLongPressTriggered = false;
        }
        else if (currentBtnState == HIGH && encoder.btnPressed) {
            // Button just released
            encoder.btnPressed = false;
            
            if (!encoder.btnLongPressTriggered) {
                // It was a short press
                encoder.pendingEvent = BTN_CLICK;
            }
        }
    }
    
    // Check for long press while button is held
    if (encoder.btnPressed && !encoder.btnLongPressTriggered) {
        if ((millis() - encoder.btnPressTime) >= LONG_PRESS_MS) {
            encoder.btnLongPressTriggered = true;
            encoder.pendingEvent = BTN_LONG_PRESS;
        }
    }
    
    lastBtnState = currentBtnState;
}

int16_t encoderGetDelta() {
    int16_t d = encoder.delta;
    encoder.delta = 0;
    return d;
}

ButtonEvent encoderGetButtonEvent() {
    ButtonEvent evt = encoder.pendingEvent;
    encoder.pendingEvent = BTN_NONE;
    return evt;
}

// =============================================================================
// UI Initialization
// =============================================================================
void uiInit() {
    // Enable TFT backlight
    pinMode(TFT_LED, OUTPUT);
    digitalWrite(TFT_LED, HIGH);

    tft.begin();
    tft.setRotation(TFT_ROTATION);
    tft.fillScreen(COLOR_BG);
    
    ui.currentScreen = SCREEN_HOME;
    ui.previousScreen = SCREEN_HOME;
    ui.scrollOffset = 0;
    ui.selectedIndex = 0;
    ui.maxItems = 0;
    ui.needsFullRedraw = true;
    ui.needsPartialRedraw = false;
    ui.lastUpdateMs = 0;
    ui.popupTitle = nullptr;
    ui.popupMessage = nullptr;
    ui.popupCallback = nullptr;
}

// =============================================================================
// Display Helpers
// =============================================================================
int16_t uiGetContentY() {
    return HEADER_HEIGHT;
}

int16_t uiGetContentHeight() {
    return TFT_HEIGHT - HEADER_HEIGHT - FOOTER_HEIGHT;
}

int16_t uiGetVisibleItems() {
    return uiGetContentHeight() / MENU_ITEM_HEIGHT;
}

void uiClearContent() {
    tft.fillRect(0, HEADER_HEIGHT, TFT_WIDTH, uiGetContentHeight(), COLOR_BG);
}

void uiDrawHeader(const char* title) {
    tft.fillRect(0, 0, TFT_WIDTH, HEADER_HEIGHT, COLOR_HEADER_BG);
    tft.setTextSize(2);  // Larger title text
    tft.setTextColor(COLOR_TEXT);
    tft.setCursor(4, 8);
    tft.print(title);
    
    // Draw status badges on right side (vertically centered)
    // Order from right: Status | PSU | Battery
    uiDrawBatteryBadge();
    uiDrawPsuBadge();
    uiDrawStatusBadge();
}

void uiDrawFooter(const char* hint) {
    tft.fillRect(0, TFT_HEIGHT - FOOTER_HEIGHT, TFT_WIDTH, FOOTER_HEIGHT, COLOR_FOOTER_BG);
    tft.setTextSize(1);
    tft.setTextColor(COLOR_TEXT);
    tft.setCursor(4, TFT_HEIGHT - FOOTER_HEIGHT + 6);
    tft.print(hint);
}

void uiDrawBadge(int16_t x, int16_t y, const char* text, uint16_t bgColor, uint16_t textColor) {
    int16_t w = strlen(text) * 6 + 8;
    tft.fillRoundRect(x, y, w, 18, 4, bgColor);
    tft.setTextSize(1);
    tft.setTextColor(textColor);
    tft.setCursor(x + 4, y + 5);
    tft.print(text);
}

void uiDrawStatusBadge() {
    static DaliStatus lastStatus = (DaliStatus)255;  // Invalid initial value to force first draw
    
    DaliStatus status = daliGetStatus();
    
    // Only redraw if status changed or forced
    if (status == lastStatus && !ui.needsFullRedraw) {
        return;
    }
    lastStatus = status;
    
    const char* text = daliStatusToString(status);
    
    uint16_t bgColor;
    switch(status) {
        case DALI_STATUS_ACT:
        case DALI_STATUS_OK:
            bgColor = COLOR_BADGE_ACT;
            break;
        case DALI_STATUS_SEND:
            bgColor = COLOR_BADGE_SEND;
            break;
        case DALI_STATUS_BUS_ERROR:
        case DALI_STATUS_FAILED:
            bgColor = COLOR_BADGE_ERR;
            break;
        default:
            bgColor = COLOR_BADGE_IDLE;
            break;
    }
    
    // Clear old badge area (fixed width for consistency)
    int16_t badgeY = (HEADER_HEIGHT - 18) / 2;
    #define STATUS_BADGE_W  46
    tft.fillRect(TFT_WIDTH - STATUS_BADGE_W - 4, badgeY, STATUS_BADGE_W, 18, COLOR_HEADER_BG);
    
    int16_t w = strlen(text) * 6 + 8;
    // Center text in fixed badge area
    int16_t badgeX = TFT_WIDTH - STATUS_BADGE_W - 4 + (STATUS_BADGE_W - w) / 2;
    uiDrawBadge(badgeX, badgeY, text, bgColor, COLOR_TEXT);
}

void uiDrawPsuBadge() {
    const char* text = daliPsuGet() ? "INT" : "EXT";
    uint16_t bgColor = daliPsuGet() ? COLOR_WARNING : COLOR_BADGE_IDLE;
    
    #define PSU_BADGE_W  34
    int16_t w = strlen(text) * 6 + 8;
    int16_t badgeY = (HEADER_HEIGHT - 18) / 2;
    // Position left of status badge (fixed width)
    int16_t badgeX = TFT_WIDTH - STATUS_BADGE_W - PSU_BADGE_W - 10;
    tft.fillRect(badgeX, badgeY, PSU_BADGE_W, 18, COLOR_HEADER_BG);
    // Center text in badge
    uiDrawBadge(badgeX + (PSU_BADGE_W - w) / 2, badgeY, text, bgColor, COLOR_TEXT);
}

// =============================================================================
// Battery Monitoring
// =============================================================================
static float batteryVoltage = 0.0;
static uint8_t batteryPercent = 0;
static uint32_t batteryLastRead = 0;

void batteryInit() {
    pinMode(BATTERY_ADC_PIN, INPUT);
    analogSetAttenuation(ADC_11db);  // 0-3.3V range
    analogReadResolution(12);         // 12-bit ADC (0-4095)
    
    // Force immediate initial read (bypass interval check)
    batteryLastRead = 0;
    uint32_t savedInterval = millis();
    batteryLastRead = savedInterval - BATTERY_READ_INTERVAL - 1;
    batteryUpdate();
}

void batteryUpdate() {
    // Only read at configured interval
    if (millis() - batteryLastRead < BATTERY_READ_INTERVAL) {
        return;
    }
    batteryLastRead = millis();
    
    // Read and average multiple samples
    uint32_t sum = 0;
    for (int i = 0; i < BATTERY_ADC_SAMPLES; i++) {
        sum += analogRead(BATTERY_ADC_PIN);
        delayMicroseconds(100);
    }
    float adcValue = (float)sum / BATTERY_ADC_SAMPLES;
    
    // Convert to voltage
    // ADC value (0-4095) -> voltage at pin (0-3.3V) -> actual battery voltage
    float pinVoltage = (adcValue / 4095.0) * BATTERY_ADC_VREF;
    batteryVoltage = pinVoltage * BATTERY_DIVIDER_RATIO;
    
    // Calculate percentage (linear mapping)
    if (batteryVoltage <= BATTERY_VOLT_MIN) {
        batteryPercent = 0;
    } else if (batteryVoltage >= BATTERY_VOLT_MAX) {
        batteryPercent = 100;
    } else {
        batteryPercent = (uint8_t)((batteryVoltage - BATTERY_VOLT_MIN) / 
                         (BATTERY_VOLT_MAX - BATTERY_VOLT_MIN) * 100.0);
    }
}

uint8_t batteryGetPercent() {
    return batteryPercent;
}

float batteryGetVoltage() {
    return batteryVoltage;
}

void uiDrawBatteryBadge() {
    static float lastVoltage = -1.0;  // Invalid initial value to force first draw
    
    float voltage = batteryGetVoltage();
    
    // Only redraw if changed significantly or forced (0.05V threshold)
    if (abs(voltage - lastVoltage) < 0.05 && !ui.needsFullRedraw) {
        return;
    }
    lastVoltage = voltage;
    
    // Format: "3.8V"
    char text[6];
    snprintf(text, sizeof(text), "%.1fV", voltage);
    
    // Color based on voltage level
    uint16_t bgColor;
    if (voltage > 3.7) {
        bgColor = COLOR_BADGE_ACT;    // Green
    } else if (voltage > 3.4) {
        bgColor = COLOR_WARNING;       // Orange
    } else {
        bgColor = COLOR_BADGE_ERR;     // Red
    }
    
    // Position: left of PSU badge (fixed width)
    #define BATT_BADGE_W  40
    int16_t w = strlen(text) * 6 + 8;
    int16_t badgeY = (HEADER_HEIGHT - 18) / 2;
    int16_t badgeX = TFT_WIDTH - STATUS_BADGE_W - PSU_BADGE_W - BATT_BADGE_W - 16;
    
    // Clear old badge area
    tft.fillRect(badgeX, badgeY, BATT_BADGE_W, 18, COLOR_HEADER_BG);
    
    // Center text in badge
    uiDrawBadge(badgeX + (BATT_BADGE_W - w) / 2, badgeY, text, bgColor, COLOR_TEXT);
}

void uiDrawText(int16_t x, int16_t y, const char* text, uint16_t color, uint8_t size) {
    tft.setTextSize(size);
    tft.setTextColor(color);
    tft.setCursor(x, y);
    tft.print(text);
}

void uiDrawTextCentered(int16_t y, const char* text, uint16_t color, uint8_t size) {
    int16_t w = strlen(text) * 6 * size;
    int16_t x = (TFT_WIDTH - w) / 2;
    uiDrawText(x, y, text, color, size);
}

void uiDrawTextRight(int16_t y, const char* text, uint16_t color, uint8_t size) {
    int16_t w = strlen(text) * 6 * size;
    uiDrawText(TFT_WIDTH - w - 4, y, text, color, size);
}

void uiDrawMenuItem(int16_t index, const char* label, bool selected, int16_t scrollOffset) {
    int16_t visibleIndex = index - scrollOffset;
    if (visibleIndex < 0 || visibleIndex >= uiGetVisibleItems()) return;
    
    int16_t y = HEADER_HEIGHT + visibleIndex * MENU_ITEM_HEIGHT;
    
    if (selected) {
        tft.fillRect(0, y, TFT_WIDTH, MENU_ITEM_HEIGHT, COLOR_SELECTED);
        tft.setTextColor(COLOR_BG);
    } else {
        tft.fillRect(0, y, TFT_WIDTH, MENU_ITEM_HEIGHT, COLOR_BG);
        tft.setTextColor(COLOR_TEXT);
    }
    
    tft.setTextSize(CONTENT_TEXT_SIZE);
    tft.setCursor(8, y + (MENU_ITEM_HEIGHT - 8 * CONTENT_TEXT_SIZE) / 2);
    tft.print(label);
}

void uiDrawListItem(int16_t index, const char* label, bool selected, int16_t scrollOffset) {
    uiDrawMenuItem(index, label, selected, scrollOffset);
}

void uiDrawScrollbar(int16_t totalItems, int16_t visibleItems, int16_t scrollOffset) {
    // Only draw scrollbar if there are more items than fit on screen
    if (totalItems <= visibleItems) return;
    
    int16_t barX = TFT_WIDTH - 6;
    int16_t barY = HEADER_HEIGHT;
    int16_t barH = uiGetContentHeight();
    int16_t barW = 4;
    
    // Draw track (dark background)
    tft.fillRect(barX, barY, barW, barH, 0x2104);  // Dark gray
    
    // Calculate thumb size and position
    int16_t thumbH = max(10, (visibleItems * barH) / totalItems);
    int16_t maxScroll = totalItems - visibleItems;
    int16_t thumbY = barY + (scrollOffset * (barH - thumbH)) / maxScroll;
    
    // Draw thumb (bright)
    tft.fillRect(barX, thumbY, barW, thumbH, COLOR_HIGHLIGHT);
}

void uiDrawField(int16_t y, const char* label, const char* value, bool selected, bool editing) {
    uint16_t labelColor = selected ? COLOR_HIGHLIGHT : COLOR_TEXT;
    uint16_t valueColor;
    uint16_t bgColor;
    
    if (editing) {
        // Editing mode: highlighted value with distinct background
        bgColor = COLOR_SELECTED;
        valueColor = COLOR_BG;
    } else if (selected) {
        // Selected but not editing: subtle highlight
        bgColor = 0x0841;  // Dark background
        valueColor = COLOR_SELECTED;
    } else {
        bgColor = COLOR_BG;
        valueColor = COLOR_TEXT;
    }
    
    int16_t rowH = MENU_ITEM_HEIGHT;
    tft.fillRect(0, y, TFT_WIDTH, rowH, bgColor);
    
    // Draw editing indicator
    if (editing) {
        tft.fillRect(0, y, 4, rowH, COLOR_HIGHLIGHT);  // Left bar when editing
    }
    
    tft.setTextSize(CONTENT_TEXT_SIZE);
    int16_t textY = y + (rowH - 8 * CONTENT_TEXT_SIZE) / 2;
    tft.setTextColor(labelColor);
    tft.setCursor(8, textY);
    tft.print(label);
    tft.print(": ");
    tft.setTextColor(valueColor);
    tft.print(value);
    
    // Draw edit brackets when editing
    if (editing) {
        int16_t charW = 6 * CONTENT_TEXT_SIZE;
        int16_t valueX = 8 + (strlen(label) + 2) * charW;
        int16_t valueW = strlen(value) * charW;
        tft.drawRect(valueX - 2, textY - 2, valueW + 4, 8 * CONTENT_TEXT_SIZE + 4, COLOR_HIGHLIGHT);
    }
}

void uiDrawFieldInt(int16_t y, const char* label, int value, bool selected, bool editing) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%d", value);
    uiDrawField(y, label, buf, selected, editing);
}

void uiDrawButton(int16_t y, const char* label, bool selected) {
    int16_t charW = 6 * CONTENT_TEXT_SIZE;
    int16_t w = strlen(label) * charW + 20;
    int16_t h = 8 * CONTENT_TEXT_SIZE + 8;
    int16_t x = (TFT_WIDTH - w) / 2;
    
    uint16_t bgColor = selected ? COLOR_HIGHLIGHT : COLOR_BADGE_IDLE;
    uint16_t textColor = COLOR_TEXT;
    
    tft.fillRoundRect(x, y, w, h, 4, bgColor);
    tft.setTextSize(CONTENT_TEXT_SIZE);
    tft.setTextColor(textColor);
    tft.setCursor(x + 10, y + (h - 8 * CONTENT_TEXT_SIZE) / 2);
    tft.print(label);
}

void uiDrawProgressBar(int16_t y, int progress, int total, const char* label) {
    int16_t barWidth = TFT_WIDTH - 40;
    int16_t barHeight = 12;
    int16_t x = 20;
    
    // Clear area
    tft.fillRect(0, y - 20, TFT_WIDTH, 40, COLOR_BG);
    
    // Draw label if provided
    if (label) {
        uiDrawTextCentered(y - 18, label, COLOR_TEXT, 1);
    }
    
    // Draw bar background
    tft.drawRect(x, y, barWidth, barHeight, COLOR_TEXT);
    
    // Draw progress fill
    int16_t fillWidth = (progress * (barWidth - 2)) / total;
    if (fillWidth > 0) {
        tft.fillRect(x + 1, y + 1, fillWidth, barHeight - 2, COLOR_HIGHLIGHT);
    }
    
    // Draw percentage
    char buf[8];
    snprintf(buf, sizeof(buf), "%d%%", (progress * 100) / total);
    int16_t textW = strlen(buf) * 6;
    tft.setTextColor(COLOR_TEXT);
    tft.setCursor(x + (barWidth - textW) / 2, y + 2);
    tft.print(buf);
}

// =============================================================================
// Screen Management
// =============================================================================
void uiSetScreen(ScreenId screen) {
    ui.previousScreen = ui.currentScreen;
    ui.currentScreen = screen;
    ui.scrollOffset = 0;
    ui.selectedIndex = 0;
    ui.needsFullRedraw = true;
}

void uiGoBack() {
    if (ui.currentScreen == SCREEN_HOME) {
        return;  // Already at home
    }
    ScreenId target = ui.previousScreen;
    ui.previousScreen = SCREEN_HOME;  // Safe fallback for next back
    ui.currentScreen = target;
    ui.scrollOffset = 0;
    ui.selectedIndex = 0;
    ui.needsFullRedraw = true;
}

void uiShowPopup(const char* title, const char* message, void (*callback)(bool)) {
    ui.popupTitle = title;
    ui.popupMessage = message;
    ui.popupCallback = callback;
    ui.popupResult = false;
    ui.popupReturnPrevious = ui.previousScreen;  // save for restore after popup
    ui.previousScreen = ui.currentScreen;
    ui.currentScreen = SCREEN_CONFIRM_POPUP;
    ui.selectedIndex = 1;  // Start on NO
    ui.needsFullRedraw = true;
}

// =============================================================================
// Utility Functions
// =============================================================================
int16_t uiClamp(int16_t value, int16_t minVal, int16_t maxVal) {
    if (value < minVal) return minVal;
    if (value > maxVal) return maxVal;
    return value;
}

int16_t uiWrap(int16_t value, int16_t minVal, int16_t maxVal) {
    int16_t range = maxVal - minVal + 1;
    while (value < minVal) value += range;
    while (value > maxVal) value -= range;
    return value;
}
