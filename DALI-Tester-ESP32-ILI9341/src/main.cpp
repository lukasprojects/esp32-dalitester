/**
 * @file main.cpp
 * @brief DALI Tester - Main application entry point
 * 
 * ESP32-based DALI bus tester with TFT display and rotary encoder control.
 * Supports DALI Master functionality and Bus Monitoring.
 * 
 * Hardware:
 * - ESP32 DevKit
 * - ILI9341 2.2" TFT Display (SPI)
 * - Rotary Encoder with push button
 * - DALI Interface (TX/RX via optocoupler/transistor)
 * - Optional internal PSU control
 * 
 * See config.h for pin assignments and settings.
 */

#include <Arduino.h>
#include <SPI.h>
#include "config.h"
#include "ui/ui.h"
#include "ui/screens.h"
#include "dali/dali_interface.h"

// =============================================================================
// Forward Declarations
// =============================================================================
void handleInput();
void handleScreenUpdate();

// =============================================================================
// Setup
// =============================================================================
void setup() {
    // Initialize serial for debugging
    Serial.begin(115200);
    Serial.println(F("\n\n================================="));
    Serial.println(F("DALI Tester - ESP32 + ILI9341"));
    Serial.println(F("================================="));
    
    // Initialize SPI
    SPI.begin(TFT_SCLK, TFT_MISO, TFT_MOSI, TFT_CS);
    
    // Initialize UI (display)
    Serial.println(F("Initializing TFT display..."));
    uiInit();
    
    // Show splash screen
    tft.fillScreen(COLOR_BG);
    uiDrawTextCentered(80, "DALI Tester", COLOR_HIGHLIGHT, 2);
    uiDrawTextCentered(120, "ESP32 + ILI9341", COLOR_TEXT, 1);
    uiDrawTextCentered(140, "Initializing...", COLOR_TEXT, 1);
    
    // Initialize encoder
    Serial.println(F("Initializing encoder..."));
    encoderInit();
    
    // Initialize DALI interface
    Serial.println(F("Initializing DALI interface..."));
    daliInit();
    
    // Initialize battery monitoring
    Serial.println(F("Initializing battery monitoring..."));
    batteryInit();
    
    // Short delay to show splash
    delay(1000);
    
    // Clear and draw home screen
    tft.fillScreen(COLOR_BG);
    ui.needsFullRedraw = true;
    
    Serial.println(F("Initialization complete!"));
    Serial.println(F("Ready for operation."));
}

// =============================================================================
// Main Loop
// =============================================================================
void loop() {
    // Update encoder state
    encoderUpdate();
    
    // Easter egg: 8-second hold on home screen launches Pong
    static uint32_t pongHoldStart = 0;
    if (ui.currentScreen == SCREEN_HOME && encoder.btnPressed) {
        if (pongHoldStart == 0) pongHoldStart = millis();
        if ((millis() - pongHoldStart) >= 8000) {
            pongHoldStart = 0;
            memset(&pongState, 0, sizeof(pongState));
            uiSetScreen(SCREEN_PONG);
        }
    } else {
        pongHoldStart = 0;
    }
    
    // Get input events
    int16_t delta = encoderGetDelta();
    ButtonEvent btn = encoderGetButtonEvent();
    
    // Handle input for current screen
    if (delta != 0 || btn != BTN_NONE) {
        if (ui.currentScreen < SCREEN_COUNT) {
            screenHandlers[ui.currentScreen].input(delta, btn);
        }
    }
    
    // Screen-specific updates (e.g., locate toggle, bus monitor)
    if (ui.currentScreen == SCREEN_LOCATE) {
        screenLocateUpdate();
    } else if (ui.currentScreen == SCREEN_BUS_MONITOR) {
        screenBusMonUpdate();
    } else if (ui.currentScreen == SCREEN_SCAN_DEVICE_INFO) {
        screenScanDeviceInfoUpdate();
    } else if (ui.currentScreen == SCREEN_PONG) {
        screenPongUpdate();
    } else {
        // Always check for bus activity (for status badge)
        daliMonitorUpdate();
    }
    
    // Update DALI status (auto-timeout ACT->IDLE)
    daliStatusUpdate();
    
    // Update battery reading
    batteryUpdate();
    
    // Update display
    bool wasFullRedraw = ui.needsFullRedraw;
    if (ui.needsFullRedraw || ui.needsPartialRedraw) {
        if (ui.currentScreen < SCREEN_COUNT) {
            screenHandlers[ui.currentScreen].draw();
        }
        ui.needsPartialRedraw = false;
    }
    
    // Always update status badges (for live status display)
    // Force redraw after full screen redraw
    if (wasFullRedraw) {
        ui.needsFullRedraw = true;  // Temporarily restore for badge
    }
    uiDrawStatusBadge();
    ui.needsFullRedraw = false;
    
    // Small delay to prevent CPU hogging
    delay(5);
}

// =============================================================================
// Debug Helper (optional)
// =============================================================================
#ifdef DEBUG_ENABLED
void debugPrintStatus() {
    static uint32_t lastDebugPrint = 0;
    uint32_t now = millis();
    
    if (now - lastDebugPrint >= 1000) {
        lastDebugPrint = now;
        
        Serial.printf("Screen: %d, Selected: %d, Scroll: %d\n", 
                      ui.currentScreen, ui.selectedIndex, ui.scrollOffset);
        Serial.printf("Encoder: pos=%ld, btn=%d\n", 
                      encoder.position, encoder.btnPressed);
        Serial.printf("DALI Status: %s, PSU: %s\n", 
                      daliStatusToString(daliGetStatus()),
                      daliPsuGet() ? "INT" : "EXT");
    }
}
#endif