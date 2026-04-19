/**
 * @file screens.cpp
 * @brief Screen handler implementations for all UI screens
 */

#include "ui/screens.h"
#include "ui/dali_icons_38x38.h"

// =============================================================================
// State Variables
// =============================================================================
ScanState scanState;
ScanDeviceInfoState scanDeviceInfoState;
BroadcastCtrlState broadcastCtrlState;
AddressState addressState;
CloneState cloneState;
ResetState resetState;
PsuState psuState;
SendCmdState sendCmdState;
LocateState locateState;
BusMonState busMonState;
PopupState popupState;
DeviceParamsState deviceParamsState;
PongState pongState;

// =============================================================================
// Home Menu Items (tile grid)
// =============================================================================
static const char* homeMenuItems[] = {
    "Scan Devices",
    "Device Parameters",
    "Address Device",
    "Clone Config",
    "Reset Device Config",
    "DALI PSU",
    "Send CMD",
    "Broadcast Control",
    "Locate",
    "Bus Monitor"
};
static const uint8_t HOME_MENU_COUNT = sizeof(homeMenuItems) / sizeof(homeMenuItems[0]);

// Tile labels (2 lines per tile for compact display)
// Max 8 chars per line at textSize=2 (12px/char, tile=102px)
static const char* homeTileLine1[] = {
    "Scan", "Device", "Address",
    "Clone", "Reset", "DALI",
    "Send", "Dimmer", "Locate",
    "Bus"
};
static const char* homeTileLine2[] = {
    "Devices", "Params", "Device",
    "Config", "Device", "PSU",
    "Command", "Control", "Device",
    "Monitor"
};

// Tile grid layout constants
#define TILE_COLS       3
#define TILE_ROWS       4
#define TILE_W          102
#define TILE_H          88
#define TILE_GAP        4
#define TILE_MARGIN_X   3
#define TILE_MARGIN_Y   2
#define TILE_RADIUS     5
#define TILE_BG         0x2945  // Dark gray tile background
#define TILE_BORDER     0x4A69  // Lighter gray border
#define TILE_ROW_H      (TILE_H + TILE_GAP)
#define TILE_ICON_COLOR 0xBDF7  // Light gray for icons
#define TILE_ICON_SEL   0x0000  // Black icons when selected

// Number of tile rows visible in content area
#define TILE_VISIBLE_ROWS  ((uiGetContentHeight() - TILE_MARGIN_Y) / TILE_ROW_H)

// Bitmap icon array (order matches home menu items)
static const uint8_t* const iconBitmaps[] = {
    icon_01_scan_devices,
    icon_02_device_parameters,
    icon_03_address_devices,
    icon_04_clone_config,
    icon_05_reset_device,
    icon_06_dali_psu,
    icon_07_send_cmd,
    icon_08_broadcast_control,
    icon_09_locate,
    icon_10_bus_monitor
};

static void drawTile(uint8_t index, bool selected, int16_t scrollPixelY) {
    uint8_t col = index % TILE_COLS;
    uint8_t row = index / TILE_COLS;

    int16_t x = TILE_MARGIN_X + col * (TILE_W + TILE_GAP);
    int16_t y = HEADER_HEIGHT + TILE_MARGIN_Y + row * TILE_ROW_H - scrollPixelY;

    // Skip tiles not fully within visible content area
    int16_t contentTop = HEADER_HEIGHT;
    int16_t contentBot = TFT_HEIGHT - FOOTER_HEIGHT;
    if (y < contentTop || y + TILE_H > contentBot) return;

    uint16_t bgColor = selected ? COLOR_SELECTED : TILE_BG;
    uint16_t textColor = selected ? COLOR_BG : COLOR_TEXT;
    uint16_t iconColor = selected ? TILE_ICON_SEL : TILE_ICON_COLOR;

    tft.fillRoundRect(x, y, TILE_W, TILE_H, TILE_RADIUS, bgColor);
    if (!selected) {
        tft.drawRoundRect(x, y, TILE_W, TILE_H, TILE_RADIUS, TILE_BORDER);
    }

    // Draw bitmap icon in upper portion of tile
    int16_t iconX = x + (TILE_W - DALI_ICON_WIDTH) / 2;
    int16_t iconY = y + 30 - DALI_ICON_HEIGHT / 2;
    tft.drawBitmap(iconX, iconY, iconBitmaps[index], DALI_ICON_WIDTH, DALI_ICON_HEIGHT, iconColor);

    // Draw text label below icon (size 2 = 12x16px per char)
    const char* line1 = homeTileLine1[index];
    const char* line2 = homeTileLine2[index];
    bool hasLine2 = strlen(line2) > 0;

    int16_t textY = y + TILE_H - (hasLine2 ? 36 : 20);

    tft.setTextSize(2);
    tft.setTextColor(textColor);

    int16_t w1 = strlen(line1) * 12;
    tft.setCursor(x + (TILE_W - w1) / 2, textY);
    tft.print(line1);

    if (hasLine2) {
        int16_t w2 = strlen(line2) * 12;
        tft.setCursor(x + (TILE_W - w2) / 2, textY + 18);
        tft.print(line2);
    }
}

// =============================================================================
// Utility Functions
// =============================================================================
const char* getTargetTypeString(uint8_t type) {
    switch(type) {
        case 0: return "Broadcast";
        case 1: return "Group";
        case 2: return "Short";
        default: return "???";
    }
}

const char* getCmdString(uint8_t cmd) {
    switch(cmd) {
        case 0: return "OFF";
        case 1: return "ON";
        case 2: return "MIN";
        case 3: return "MAX";
        case 4: return "SCENE";
        default: return "???";
    }
}

const char* getAddressModeString(uint8_t mode) {
    switch(mode) {
        case 0: return "Address ALL";
        case 1: return "Addr Unaddressed";
        case 2: return "Change Address";
        default: return "???";
    }
}

static const char* getDaliCmdName(uint8_t cmd) {
    switch(cmd) {
        case DALI_CMD_OFF: return "OFF";
        case DALI_CMD_RECALL_MAX: return "MAX";
        case DALI_CMD_RECALL_MIN: return "MIN";
        case DALI_CMD_QUERY_STATUS: return "Q_STATUS";
        case DALI_CMD_QUERY_ACTUAL_LEVEL: return "Q_LEVEL";
        case DALI_CMD_QUERY_DEVICE_TYPE: return "Q_DEVTYPE";
        default:
            if (cmd >= 0x10 && cmd <= 0x1F) {
                static char sceneBuf[12];
                snprintf(sceneBuf, sizeof(sceneBuf), "SCENE %d", cmd - 0x10);
                return sceneBuf;
            }
            return nullptr;
    }
}

void formatBusFrame(const BusMonitorEntry* entry, char* buf, size_t bufSize) {
    if (!entry->valid) {
        snprintf(buf, bufSize, "---");
        return;
    }
    
    if (entry->type == FRAME_BACKWARD) {
        snprintf(buf, bufSize, "REPLY 0x%02X (%d)", entry->dataByte, entry->dataByte);
        return;
    }
    
    if (entry->type == FRAME_EVENT) {
        // 24-bit DALI-2 Event frame
        // Format: [Instance Type][Instance Number][Event Info]
        // Byte 0: Device/Instance addressing
        // Byte 1: Instance Type + Instance Number
        // Byte 2: Event data
        
        uint8_t addrByte = entry->addrByte;
        uint8_t instByte = entry->dataByte;
        uint8_t eventData = entry->eventByte;
        
        // Extract device address (bits 7-1 of addrByte, or special addressing)
        uint8_t shortAddr = (addrByte >> 1) & 0x3F;
        
        // Instance type (bits 7-3 of instByte)
        uint8_t instType = (instByte >> 3) & 0x1F;
        // Instance number (bits 2-0 of instByte)
        uint8_t instNum = instByte & 0x07;
        
        // Decode event type based on instance type
        const char* eventName;
        switch(instType) {
            case 1:  eventName = "OccSensor"; break;  // Occupancy sensor
            case 2:  eventName = "LightSens"; break;  // Light sensor
            case 3:  eventName = "Button"; break;     // Push button
            case 4:  eventName = "Slider"; break;     // Slider/rotary
            default: eventName = "Event"; break;
        }
        
        snprintf(buf, bufSize, "SA%d %s.%d=%d", shortAddr, eventName, instNum, eventData);
        return;
    }
    
    // Forward frame (16-bit) - decode address
    uint8_t addr7 = entry->addrByte >> 1;  // Remove S bit
    bool isCmd = entry->addrByte & 0x01;    // S bit
    
    char targetStr[16];
    
    // Check for broadcast
    if ((entry->addrByte & 0xFE) == 0xFE) {
        strcpy(targetStr, "BC");
    }
    // Check for group
    else if ((addr7 & 0x60) == 0x40) {
        uint8_t group = (addr7 >> 1) & 0x0F;
        snprintf(targetStr, sizeof(targetStr), "G%d", group);
    }
    // Short address
    else if ((addr7 & 0x40) == 0x00) {
        uint8_t shortAddr = addr7 & 0x3F;
        snprintf(targetStr, sizeof(targetStr), "SA%d", shortAddr);
    }
    else {
        snprintf(targetStr, sizeof(targetStr), "?%02X", entry->addrByte);
    }
    
    if (!isCmd) {
        // S=0: Direct Arc Power (gear) or Device Command (DALI-2 control device)
        if (entry->dataByte >= 0x90) {
            // Values >= 0x90 are likely DALI-2 device queries/commands
            const char* cmdName = getDaliCmdName(entry->dataByte);
            if (cmdName) {
                snprintf(buf, bufSize, "%s D:%s", targetStr, cmdName);
            } else {
                snprintf(buf, bufSize, "%s D:0x%02X", targetStr, entry->dataByte);
            }
        } else {
            snprintf(buf, bufSize, "%s ARC %d", targetStr, entry->dataByte);
        }
    } else {
        // Command
        const char* cmdName = getDaliCmdName(entry->dataByte);
        if (cmdName) {
            snprintf(buf, bufSize, "%s %s", targetStr, cmdName);
        } else {
            snprintf(buf, bufSize, "%s CMD 0x%02X", targetStr, entry->dataByte);
        }
    }
}

// =============================================================================
// HOME SCREEN
// =============================================================================
// Track previous home menu state for partial redraws
static int16_t homePrevSelected = -1;
static int16_t homePrevScrollOffset = -1;

void screenHomeDraw() {
    int16_t scrollPx = ui.scrollOffset * TILE_ROW_H;

    if (ui.needsFullRedraw) {
        uiDrawHeader("DALI Tester");
        uiClearContent();
        // Footer with FW/HW version and credits (2 lines)
        tft.fillRect(0, TFT_HEIGHT - FOOTER_HEIGHT, TFT_WIDTH, FOOTER_HEIGHT, COLOR_FOOTER_BG);
        tft.setTextSize(1);
        tft.setTextColor(COLOR_TEXT);
        char footerBuf[52];
        snprintf(footerBuf, sizeof(footerBuf), "FW %s by Claude Code Opus 4.6", FW_VERSION);
        tft.setCursor(4, TFT_HEIGHT - FOOTER_HEIGHT + 2);
        tft.print(footerBuf);
        snprintf(footerBuf, sizeof(footerBuf), "HW %s by Lukas Meissner", HW_VERSION);
        tft.setCursor(4, TFT_HEIGHT - FOOTER_HEIGHT + 12);
        tft.print(footerBuf);
        ui.maxItems = HOME_MENU_COUNT;
        ui.needsFullRedraw = false;
        for (uint8_t i = 0; i < HOME_MENU_COUNT; i++) {
            drawTile(i, i == ui.selectedIndex, scrollPx);
        }
        int16_t totalRows = (HOME_MENU_COUNT + TILE_COLS - 1) / TILE_COLS;
        int16_t visRows = TILE_VISIBLE_ROWS;
        if (totalRows > visRows) {
            uiDrawScrollbar(totalRows, visRows, ui.scrollOffset);
        }
        homePrevSelected = ui.selectedIndex;
        homePrevScrollOffset = ui.scrollOffset;
        return;
    }

    if (ui.needsPartialRedraw) {
        if (ui.scrollOffset != homePrevScrollOffset) {
            // Scroll changed: must redraw all visible tiles
            uiClearContent();
            for (uint8_t i = 0; i < HOME_MENU_COUNT; i++) {
                drawTile(i, i == ui.selectedIndex, scrollPx);
            }
            int16_t totalRows = (HOME_MENU_COUNT + TILE_COLS - 1) / TILE_COLS;
            int16_t visRows = TILE_VISIBLE_ROWS;
            if (totalRows > visRows) {
                uiDrawScrollbar(totalRows, visRows, ui.scrollOffset);
            }
        } else {
            // Only selection changed: redraw old and new tile
            if (homePrevSelected >= 0 && homePrevSelected < HOME_MENU_COUNT && homePrevSelected != ui.selectedIndex) {
                drawTile(homePrevSelected, false, scrollPx);
            }
            drawTile(ui.selectedIndex, true, scrollPx);
        }
        homePrevSelected = ui.selectedIndex;
        homePrevScrollOffset = ui.scrollOffset;
        ui.needsPartialRedraw = false;
    }
}

void screenHomeInput(int16_t delta, ButtonEvent btn) {
    if (delta != 0) {
        ui.selectedIndex += delta;
        ui.selectedIndex = uiClamp(ui.selectedIndex, 0, HOME_MENU_COUNT - 1);

        // Scroll by row to keep selected tile visible
        int16_t selRow = ui.selectedIndex / TILE_COLS;
        int16_t visRows = TILE_VISIBLE_ROWS;
        if (selRow < ui.scrollOffset) {
            ui.scrollOffset = selRow;
        } else if (selRow >= ui.scrollOffset + visRows) {
            ui.scrollOffset = selRow - visRows + 1;
        }
        ui.needsPartialRedraw = true;
    }
    
    if (btn == BTN_CLICK) {
        switch(ui.selectedIndex) {
            case 0: 
                memset(&scanState, 0, sizeof(scanState));
                uiSetScreen(SCREEN_SCAN_DEVICES); 
                break;
            case 1:
                memset(&deviceParamsState, 0, sizeof(deviceParamsState));
                uiSetScreen(SCREEN_DEVICE_PARAMS);
                break;
            case 2: 
                memset(&addressState, 0, sizeof(addressState));
                uiSetScreen(SCREEN_ADDRESS_DEVICE); 
                break;
            case 3: 
                memset(&cloneState, 0, sizeof(cloneState));
                uiSetScreen(SCREEN_CLONE_CONFIG); 
                break;
            case 4: 
                memset(&resetState, 0, sizeof(resetState));
                uiSetScreen(SCREEN_RESET_DEVICE); 
                break;
            case 5: 
                psuState.internal = daliPsuGet();
                psuState.field = 0;
                uiSetScreen(SCREEN_DALI_PSU); 
                break;
            case 6: 
                memset(&sendCmdState, 0, sizeof(sendCmdState));
                uiSetScreen(SCREEN_SEND_CMD); 
                break;
            case 7:
                broadcastCtrlState.level = 128;
                broadcastCtrlState.isOn = false;
                uiSetScreen(SCREEN_BROADCAST_CTRL);
                break;
            case 8: 
                memset(&locateState, 0, sizeof(locateState));
                locateState.interval = LOCATE_INTERVAL_DEFAULT;
                uiSetScreen(SCREEN_LOCATE); 
                break;
            case 9: 
                memset(&busMonState, 0, sizeof(busMonState));
                uiSetScreen(SCREEN_BUS_MONITOR); 
                break;
        }
    }
}

// =============================================================================
// SCAN DEVICES SCREEN
// =============================================================================
void screenScanDraw() {
    if (ui.needsFullRedraw) {
        uiDrawHeader("Scan Devices");
        ui.needsFullRedraw = false;
    }
    
    uiClearContent();
    
    if (scanState.scanning) {
        uiDrawTextCentered(100, "Scanning...", COLOR_TEXT, CONTENT_TEXT_SIZE);
        uiDrawFooter("Please wait...");
        return;
    }
    
    if (!scanState.scanComplete) {
        if (scanState.noBusVoltage) {
            uiDrawTextCentered(80, "No bus voltage!", COLOR_ERROR, CONTENT_TEXT_SIZE);
            uiDrawTextCentered(110, "Check DALI PSU", COLOR_WARNING, CONTENT_TEXT_SIZE);
            uiDrawTextCentered(140, "Click to retry", COLOR_TEXT, CONTENT_TEXT_SIZE);
            uiDrawFooter("Click:Retry Hold:Back");
        } else {
            uiDrawTextCentered(100, "No scan data", COLOR_TEXT, CONTENT_TEXT_SIZE);
            uiDrawTextCentered(130, "Click to scan", COLOR_HIGHLIGHT, CONTENT_TEXT_SIZE);
            uiDrawFooter("Click:Scan Hold:Back");
        }
        return;
    }
    
    uint8_t totalDevices = scanState.deviceCount + scanState.ctrlDevCount + scanState.unaddressedCount;
    
    if (totalDevices == 0) {
        uiDrawTextCentered(100, "No devices", COLOR_WARNING, CONTENT_TEXT_SIZE);
        uiDrawTextCentered(130, "Click to rescan", COLOR_TEXT, CONTENT_TEXT_SIZE);
        uiDrawFooter("Click:Rescan Hold:Back");
        return;
    }
    
    // Display found devices summary
    char buf[40];
    if (scanState.ctrlDevCount > 0 || scanState.unaddressedCount > 0) {
        snprintf(buf, sizeof(buf), "G:%d D:%d U:%d", 
                 scanState.deviceCount, scanState.ctrlDevCount, scanState.unaddressedCount);
        uiDrawText(8, HEADER_HEIGHT + 4, buf, COLOR_WARNING, CONTENT_TEXT_SIZE);
    } else {
        snprintf(buf, sizeof(buf), "Found: %d", scanState.deviceCount);
        uiDrawText(8, HEADER_HEIGHT + 4, buf, COLOR_HIGHLIGHT, CONTENT_TEXT_SIZE);
    }
    
    int16_t startY = HEADER_HEIGHT + MENU_ITEM_HEIGHT;
    int16_t visible = (uiGetContentHeight() - MENU_ITEM_HEIGHT - 28) / MENU_ITEM_HEIGHT;
    
    for (int i = 0; i < totalDevices && i < visible; i++) {
        int16_t idx = i + ui.scrollOffset;
        if (idx >= totalDevices) break;
        
        // Determine which section: gear / ctrl device / unaddressed
        bool isGear = (idx < scanState.deviceCount);
        bool isCtrlDev = (idx >= scanState.deviceCount && idx < scanState.deviceCount + scanState.ctrlDevCount);
        bool isUnaddressed = (idx >= scanState.deviceCount + scanState.ctrlDevCount);
        
        if (isGear) {
            uint8_t addr = scanState.foundAddrs[idx];
            uint8_t devType = scanState.deviceTypes[idx];
            snprintf(buf, sizeof(buf), "SA%02d G:%s", addr, daliGetDeviceTypeName(devType));
        } else if (isCtrlDev) {
            uint8_t cdIdx = idx - scanState.deviceCount;
            uint8_t addr = scanState.ctrlDevAddrs[cdIdx];
            uint8_t devType = scanState.ctrlDevTypes[cdIdx];
            snprintf(buf, sizeof(buf), "SA%02d D:%s", addr, daliGetDeviceTypeName(devType));
        } else {
            uint8_t unIdx = idx - scanState.deviceCount - scanState.ctrlDevCount + 1;
            snprintf(buf, sizeof(buf), "Unaddressed #%d", unIdx);
        }
        
        bool selected = (idx == scanState.selectedDevice);
        int16_t y = startY + i * MENU_ITEM_HEIGHT;
        
        if (selected) {
            tft.fillRect(0, y, TFT_WIDTH, MENU_ITEM_HEIGHT, COLOR_SELECTED);
            tft.setTextColor(COLOR_BG);
        } else if (isCtrlDev) {
            tft.fillRect(0, y, TFT_WIDTH, MENU_ITEM_HEIGHT, COLOR_BG);
            tft.setTextColor(COLOR_HIGHLIGHT);
        } else if (isUnaddressed) {
            tft.fillRect(0, y, TFT_WIDTH, MENU_ITEM_HEIGHT, COLOR_BG);
            tft.setTextColor(COLOR_WARNING);
        } else {
            tft.fillRect(0, y, TFT_WIDTH, MENU_ITEM_HEIGHT, COLOR_BG);
            tft.setTextColor(COLOR_TEXT);
        }
        
        tft.setTextSize(CONTENT_TEXT_SIZE);
        tft.setCursor(8, y + (MENU_ITEM_HEIGHT - 8 * CONTENT_TEXT_SIZE) / 2);
        tft.print(buf);
    }
    
    // Show device details if selected
    if (scanState.selectedDevice >= 0 && scanState.selectedDevice < totalDevices) {
        int16_t infoY = TFT_HEIGHT - FOOTER_HEIGHT - 24;
        char infoBuf[40];
        
        if (scanState.selectedDevice < scanState.deviceCount) {
            uint8_t addr = scanState.foundAddrs[scanState.selectedDevice];
            uint8_t devType = scanState.deviceTypes[scanState.selectedDevice];
            snprintf(infoBuf, sizeof(infoBuf), "SA%d Gear - %s", addr, daliGetDeviceTypeName(devType));
            uiDrawText(8, infoY, infoBuf, COLOR_HIGHLIGHT, CONTENT_TEXT_SIZE);
        } else if (scanState.selectedDevice < scanState.deviceCount + scanState.ctrlDevCount) {
            uint8_t cdIdx = scanState.selectedDevice - scanState.deviceCount;
            uint8_t addr = scanState.ctrlDevAddrs[cdIdx];
            uint8_t devType = scanState.ctrlDevTypes[cdIdx];
            snprintf(infoBuf, sizeof(infoBuf), "SA%d Device - %s", addr, daliGetDeviceTypeName(devType));
            uiDrawText(8, infoY, infoBuf, COLOR_HIGHLIGHT, CONTENT_TEXT_SIZE);
        } else {
            snprintf(infoBuf, sizeof(infoBuf), "Unaddressed - use Address Device");
            uiDrawText(8, infoY, infoBuf, COLOR_WARNING, CONTENT_TEXT_SIZE);
        }
    }
    
    // Footer
    uiDrawFooter("Rotate:Nav Click:Info Hold:Back");
}

void screenScanInput(int16_t delta, ButtonEvent btn) {
    if (btn == BTN_LONG_PRESS) {
        uiGoBack();
        return;
    }
    
    uint8_t totalDevices = scanState.deviceCount + scanState.ctrlDevCount + scanState.unaddressedCount;
    
    if (!scanState.scanComplete || totalDevices == 0) {
        if (btn == BTN_CLICK && !scanState.scanning) {
            // Check bus voltage before scanning: sample RX line
            // Without bus voltage, RX stays permanently LOW
            uint8_t busHighCount = 0;
            for (uint8_t i = 0; i < 10; i++) {
                if (bus_is_high()) busHighCount++;
                delay(2);
            }
            if (busHighCount == 0) {
                // Bus appears dead - no voltage detected
                scanState.noBusVoltage = true;
                ui.needsFullRedraw = true;
                return;
            }
            scanState.noBusVoltage = false;

            // Start scan
            scanState.scanning = true;
            ui.needsFullRedraw = true;
            
            // 1. Scan for addressed control gear (S=1)
            scanState.deviceCount = daliScanDevices(scanState.foundAddrs, 64);
            
            // Query gear device types
            for (int i = 0; i < scanState.deviceCount; i++) {
                int16_t devType = daliQueryDeviceType(scanState.foundAddrs[i]);
                scanState.deviceTypes[i] = (devType >= 0) ? devType : 0xFF;
            }
            
            // 2. Scan for DALI-2 control devices (S=0, skip gear addresses)
            scanState.ctrlDevCount = daliScanControlDevices(
                scanState.ctrlDevAddrs, scanState.foundAddrs, scanState.deviceCount, 64);
            
            // Query control device types
            for (int i = 0; i < scanState.ctrlDevCount; i++) {
                int16_t devType = daliQueryDeviceDeviceType(scanState.ctrlDevAddrs[i]);
                scanState.ctrlDevTypes[i] = (devType >= 0) ? devType : 0xFF;
            }
            
            // 3. Count unaddressed devices on bus
            scanState.unaddressedCount = daliCountUnaddressed();
            scanState.checkedUnaddressed = true;
            
            scanState.scanning = false;
            scanState.scanComplete = true;
            scanState.selectedDevice = 0;
            ui.needsFullRedraw = true;
        }
        return;
    }
    
    // Navigate devices (gear + ctrl devices + unaddressed)
    if (delta != 0) {
        scanState.selectedDevice += delta;
        scanState.selectedDevice = uiClamp(scanState.selectedDevice, 0, totalDevices - 1);
        
        // Adjust scroll
        int16_t visible = (uiGetContentHeight() - 30) / 16;
        if (scanState.selectedDevice < ui.scrollOffset) {
            ui.scrollOffset = scanState.selectedDevice;
        } else if (scanState.selectedDevice >= ui.scrollOffset + visible) {
            ui.scrollOffset = scanState.selectedDevice - visible + 1;
        }
        ui.needsPartialRedraw = true;
    }
    
    // Click on device opens info screen (gear and ctrl devices)
    if (btn == BTN_CLICK && scanState.selectedDevice >= 0 && scanState.selectedDevice < totalDevices) {
        if (scanState.selectedDevice < scanState.deviceCount) {
            // Control gear - open info screen
            uint8_t addr = scanState.foundAddrs[scanState.selectedDevice];
            memset(&scanDeviceInfoState, 0, sizeof(scanDeviceInfoState));
            scanDeviceInfoState.addr = addr;
            scanDeviceInfoState.deviceType = scanState.deviceTypes[scanState.selectedDevice];
            uiSetScreen(SCREEN_SCAN_DEVICE_INFO);
        } else if (scanState.selectedDevice < scanState.deviceCount + scanState.ctrlDevCount) {
            // DALI-2 control device - open info screen
            uint8_t cdIdx = scanState.selectedDevice - scanState.deviceCount;
            uint8_t addr = scanState.ctrlDevAddrs[cdIdx];
            memset(&scanDeviceInfoState, 0, sizeof(scanDeviceInfoState));
            scanDeviceInfoState.addr = addr;
            scanDeviceInfoState.deviceType = scanState.ctrlDevTypes[cdIdx];
            uiSetScreen(SCREEN_SCAN_DEVICE_INFO);
        }
        // Unaddressed devices: no detail screen (no short address to query)
    }
}

// =============================================================================
// SCAN DEVICE INFO SCREEN (Context Menu)
// =============================================================================
#define LOCATE_INTERVAL_FIXED 2000  // 2 seconds for locate in scan screen

void screenScanDeviceInfoDraw() {
    if (ui.needsFullRedraw) {
        uiDrawHeader("Device Info");
        uiClearContent();
        ui.needsFullRedraw = false;
        
        // Query device info on first draw
        int32_t serial = daliQueryRandomAddress(scanDeviceInfoState.addr);
        scanDeviceInfoState.serialNumber = (serial >= 0) ? serial : 0;
        scanDeviceInfoState.serialValid = (serial >= 0);
        scanDeviceInfoState.actualLevel = daliQueryActualLevel(scanDeviceInfoState.addr);
        scanDeviceInfoState.status = daliQueryStatus(scanDeviceInfoState.addr);
    }
    
    const char* footerText;
    if (scanDeviceInfoState.locating) {
        footerText = "Click:Stop Locate";
    } else {
        footerText = "Rotate:Select Click:Action Hold:Back";
    }
    uiDrawFooter(footerText);
    
    int16_t y = HEADER_HEIGHT + 8;
    char buf[32];
    
    // Short Address
    snprintf(buf, sizeof(buf), "SA %d", scanDeviceInfoState.addr);
    uiDrawText(8, y, buf, COLOR_HIGHLIGHT, CONTENT_TEXT_SIZE);
    y += MENU_ITEM_HEIGHT;
    
    // Device Type
    snprintf(buf, sizeof(buf), "Type: %s", daliGetDeviceTypeName(scanDeviceInfoState.deviceType));
    uiDrawText(8, y, buf, COLOR_TEXT, CONTENT_TEXT_SIZE);
    y += MENU_ITEM_HEIGHT;
    
    // Serial Number (Random Address)
    // Note: 0xFFFFFF (all F's) indicates the device was never commissioned/randomized
    if (scanDeviceInfoState.serialValid) {
        if (scanDeviceInfoState.serialNumber == 0xFFFFFF) {
            snprintf(buf, sizeof(buf), "Serial: Not Assigned");
        } else {
            snprintf(buf, sizeof(buf), "Serial: %06lX", (unsigned long)scanDeviceInfoState.serialNumber);
        }
    } else {
        snprintf(buf, sizeof(buf), "Serial: ---");
    }
    uiDrawText(8, y, buf, COLOR_TEXT, CONTENT_TEXT_SIZE);
    y += MENU_ITEM_HEIGHT;
    
    // Actual Level
    if (scanDeviceInfoState.actualLevel >= 0) {
        snprintf(buf, sizeof(buf), "Level: %d", scanDeviceInfoState.actualLevel);
    } else {
        snprintf(buf, sizeof(buf), "Level: ---");
    }
    uiDrawText(8, y, buf, COLOR_TEXT, CONTENT_TEXT_SIZE);
    y += MENU_ITEM_HEIGHT;
    
    // Status
    if (scanDeviceInfoState.status >= 0) {
        snprintf(buf, sizeof(buf), "Status: 0x%02X", scanDeviceInfoState.status);
    } else {
        snprintf(buf, sizeof(buf), "Status: ---");
    }
    uiDrawText(8, y, buf, COLOR_TEXT, CONTENT_TEXT_SIZE);
    y += MENU_ITEM_HEIGHT + 8;
    
    // Locate button
    const char* locateText = scanDeviceInfoState.locating ? "STOP LOCATE" : "LOCATE (2s blink)";
    uiDrawButton(y, locateText, scanDeviceInfoState.field == 0);
    
    // Show locate status
    if (scanDeviceInfoState.locating) {
        y += 35;
        const char* stateText = scanDeviceInfoState.locateIsMax ? ">>> MAX <<<" : ">>> MIN <<<";
        uiDrawTextCentered(y, stateText, COLOR_HIGHLIGHT, CONTENT_TEXT_SIZE);
    }
}

void screenScanDeviceInfoUpdate() {
    if (!scanDeviceInfoState.locating) return;
    
    uint32_t now = millis();
    if (now - scanDeviceInfoState.locateLastToggle >= LOCATE_INTERVAL_FIXED) {
        scanDeviceInfoState.locateLastToggle = now;
        scanDeviceInfoState.locateIsMax = !scanDeviceInfoState.locateIsMax;
        
        // Build address (short address with S bit for command)
        uint8_t addrByte = (scanDeviceInfoState.addr << 1) | 0x01;
        uint8_t cmdByte = scanDeviceInfoState.locateIsMax ? DALI_CMD_RECALL_MAX : DALI_CMD_RECALL_MIN;
        daliCmdUi(addrByte, cmdByte, false);
        
        ui.needsPartialRedraw = true;
    }
}

void screenScanDeviceInfoInput(int16_t delta, ButtonEvent btn) {
    if (btn == BTN_LONG_PRESS) {
        scanDeviceInfoState.locating = false;
        uiGoBack();
        return;
    }
    
    if (btn == BTN_CLICK) {
        if (scanDeviceInfoState.locating) {
            // Stop locate
            scanDeviceInfoState.locating = false;
            ui.needsFullRedraw = true;
        } else {
            // Start locate
            scanDeviceInfoState.locating = true;
            scanDeviceInfoState.locateIsMax = false;
            scanDeviceInfoState.locateLastToggle = millis();
            ui.needsFullRedraw = true;
        }
    }
    
    // Only one field (Locate button) for now
    if (delta != 0) {
        ui.needsPartialRedraw = true;
    }
}

// =============================================================================
// ADDRESS DEVICE SCREEN
// =============================================================================
void screenAddressDraw() {
    if (ui.needsFullRedraw) {
        uiDrawHeader("Address Device");
        ui.needsFullRedraw = false;
        uiClearContent();
    }
    
    // Dynamic footer based on editing state
    const char* footerText = addressState.editing ? 
        "Rotate:Value Click:Confirm" : 
        "Rotate:Select Click:Edit Hold:Back";
    uiDrawFooter(footerText);
    
    int16_t y = HEADER_HEIGHT + 8;
    
    // Mode field
    uiDrawField(y, "Mode", getAddressModeString(addressState.mode), 
                addressState.field == 0, addressState.field == 0 && addressState.editing);
    y += 20;
    
    // Current address (only for Change mode)
    if (addressState.mode == 2) {
        char buf[8];
        snprintf(buf, sizeof(buf), "%d", addressState.currentAddr);
        uiDrawField(y, "Current", buf, addressState.field == 1,
                    addressState.field == 1 && addressState.editing);
    } else {
        uiDrawField(y, "Current", "N/A", false, false);
    }
    y += 20;
    
    // New address
    char buf[8];
    snprintf(buf, sizeof(buf), "%d", addressState.newAddr);
    uiDrawField(y, "New Addr", buf, addressState.field == 2,
                addressState.field == 2 && addressState.editing);
    y += 30;
    
    // Execute button
    uiDrawButton(y, "EXECUTE", addressState.field == 3);
}

void screenAddressInput(int16_t delta, ButtonEvent btn) {
    if (btn == BTN_LONG_PRESS) {
        if (addressState.editing) {
            addressState.editing = false;
        } else {
            uiGoBack();
            return;
        }
        ui.needsPartialRedraw = true;
        return;
    }
    
    if (delta != 0) {
        if (addressState.editing) {
            // Change value of current field
            switch(addressState.field) {
                case 0:  // Mode
                    addressState.mode = uiWrap(addressState.mode + delta, 0, 2);
                    break;
                case 1:  // Current (only in Change mode)
                    if (addressState.mode == 2) {
                        addressState.currentAddr = uiClamp(addressState.currentAddr + delta, 0, 63);
                    }
                    break;
                case 2:  // New
                    addressState.newAddr = uiClamp(addressState.newAddr + delta, 0, 63);
                    break;
            }
        } else {
            // Navigate between fields
            int8_t maxField = 3;
            addressState.field = uiClamp(addressState.field + delta, 0, maxField);
            
            // Skip Current field if not in Change mode
            if (addressState.field == 1 && addressState.mode != 2) {
                addressState.field = (delta > 0) ? 2 : 0;
            }
        }
        ui.needsPartialRedraw = true;
    }
    
    if (btn == BTN_CLICK) {
        if (addressState.field == 3) {
            // Execute action
            bool success = false;
            switch(addressState.mode) {
                case 0:  // Address ALL
                    success = daliCommissionAll();
                    break;
                case 1:  // Address Unaddressed
                    success = daliCommissionUnaddressed();
                    break;
                case 2:  // Change Address
                    success = daliChangeAddress(addressState.currentAddr, addressState.newAddr);
                    break;
            }
            daliSetStatus(success ? DALI_STATUS_OK : DALI_STATUS_FAILED);
        } else {
            // Toggle editing mode
            addressState.editing = !addressState.editing;
        }
        ui.needsPartialRedraw = true;
    }
}

// =============================================================================
// CLONE CONFIG SCREEN
// =============================================================================
void screenCloneDraw() {
    if (ui.needsFullRedraw) {
        uiDrawHeader("Clone Config");
        ui.needsFullRedraw = false;
        uiClearContent();
    }
    
    // Dynamic footer based on editing state
    const char* footerText = cloneState.editing ? 
        "Rotate:Value Click:Confirm" : 
        "Rotate:Select Click:Edit Hold:Back";
    uiDrawFooter(footerText);
    
    int16_t y = HEADER_HEIGHT + 8;
    
    // Source address
    char buf[16];
    snprintf(buf, sizeof(buf), "%d", cloneState.sourceAddr);
    uiDrawField(y, "Source", buf, cloneState.field == 0,
                cloneState.field == 0 && cloneState.editing);
    y += MENU_ITEM_HEIGHT;
    
    // Capture button/status
    snprintf(buf, sizeof(buf), "%s", cloneState.captured ? "YES" : "NO");
    uiDrawField(y, "Captured", buf, cloneState.field == 1, false);
    y += MENU_ITEM_HEIGHT;
    
    // Show captured config if available
    if (cloneState.captured) {
        tft.setTextSize(CONTENT_TEXT_SIZE);
        tft.setTextColor(COLOR_HIGHLIGHT);
        tft.setCursor(16, y);
        tft.printf("Max:%d Min:%d", 
                   cloneState.config.maxLevel, 
                   cloneState.config.minLevel);
        y += MENU_ITEM_HEIGHT;
        tft.setCursor(16, y);
        tft.printf("PwrOn:%d Sys:%d",
                   cloneState.config.powerOnLevel,
                   cloneState.config.sysFailLevel);
        y += MENU_ITEM_HEIGHT;
    } else {
        y += MENU_ITEM_HEIGHT * 2;
    }
    
    // Target address
    snprintf(buf, sizeof(buf), "%d", cloneState.targetAddr);
    uiDrawField(y, "Target", buf, cloneState.field == 2,
                cloneState.field == 2 && cloneState.editing);
    y += MENU_ITEM_HEIGHT + 4;
    
    // Send button
    uiDrawButton(y, "SEND CONFIG", cloneState.field == 3);
}

void screenCloneInput(int16_t delta, ButtonEvent btn) {
    if (btn == BTN_LONG_PRESS) {
        if (cloneState.editing) {
            cloneState.editing = false;
        } else {
            uiGoBack();
            return;
        }
        ui.needsPartialRedraw = true;
        return;
    }
    
    if (delta != 0) {
        if (cloneState.editing) {
            // Change value
            switch(cloneState.field) {
                case 0:  // Source addr
                    cloneState.sourceAddr = uiClamp(cloneState.sourceAddr + delta, 0, 63);
                    break;
                case 2:  // Target addr
                    cloneState.targetAddr = uiClamp(cloneState.targetAddr + delta, 0, 63);
                    break;
            }
        } else {
            // Navigate fields
            cloneState.field = uiClamp(cloneState.field + delta, 0, 3);
        }
        ui.needsPartialRedraw = true;
    }
    
    if (btn == BTN_CLICK) {
        if (cloneState.field == 1) {
            // Capture config (action button, no editing)
            cloneState.captured = daliReadConfig(cloneState.sourceAddr, &cloneState.config);
            ui.needsFullRedraw = true;
        } else if (cloneState.field == 3) {
            // Send config (action button)
            if (cloneState.captured) {
                bool success = daliWriteConfig(cloneState.targetAddr, &cloneState.config);
                daliSetStatus(success ? DALI_STATUS_OK : DALI_STATUS_FAILED);
            }
        } else {
            // Toggle editing mode for address fields
            cloneState.editing = !cloneState.editing;
        }
        ui.needsPartialRedraw = true;
    }
}

// =============================================================================
// RESET DEVICE SCREEN
// =============================================================================
void screenResetDraw() {
    if (ui.needsFullRedraw) {
        uiDrawHeader("Reset Device");
        ui.needsFullRedraw = false;
        uiClearContent();
    }
    
    // Dynamic footer based on editing state
    const char* footerText = resetState.editing ? 
        "Rotate:Value Click:Confirm" : 
        "Rotate:Select Click:Edit Hold:Back";
    uiDrawFooter(footerText);
    
    int16_t y = HEADER_HEIGHT + 15;
    
    char buf[8];
    snprintf(buf, sizeof(buf), "%d", resetState.addr);
    uiDrawField(y, "Short Addr", buf, resetState.field == 0,
                resetState.field == 0 && resetState.editing);
    y += 28;
    
    // Delete address option
    const char* delAddrStr = resetState.deleteAddress ? "YES" : "NO";
    uiDrawField(y, "Delete Addr", delAddrStr, resetState.field == 1,
                resetState.field == 1 && resetState.editing);
    y += 28;
    
    uiDrawButton(y, "RESET DEVICE", resetState.field == 2);
    y += 35;
    
    // Clear text area before drawing to prevent overlap when switching
    tft.fillRect(0, y, TFT_WIDTH, 20, COLOR_BG);
    
    // Warning text (short to fit one line)
    if (resetState.deleteAddress) {
        uiDrawTextCentered(y, "Config + Adresse loeschen", COLOR_WARNING, CONTENT_TEXT_SIZE);
    } else {
        uiDrawTextCentered(y, "Nur Config zuruecksetzen", COLOR_WARNING, CONTENT_TEXT_SIZE);
    }
}

void screenResetInput(int16_t delta, ButtonEvent btn) {
    if (btn == BTN_LONG_PRESS) {
        if (resetState.editing) {
            resetState.editing = false;
        } else {
            uiGoBack();
            return;
        }
        ui.needsPartialRedraw = true;
        return;
    }
    
    if (delta != 0) {
        if (resetState.editing) {
            if (resetState.field == 0) {
                resetState.addr = uiClamp(resetState.addr + delta, 0, 63);
            } else if (resetState.field == 1) {
                resetState.deleteAddress = !resetState.deleteAddress;
            }
        } else {
            resetState.field = uiClamp(resetState.field + delta, 0, 2);
        }
        ui.needsPartialRedraw = true;
    }
    
    if (btn == BTN_CLICK) {
        if (resetState.field == 2) {
            // Execute reset with optional address deletion
            bool success = daliReset(resetState.addr, resetState.deleteAddress);
            daliSetStatus(success ? DALI_STATUS_OK : DALI_STATUS_FAILED);
        } else {
            // Toggle editing mode
            resetState.editing = !resetState.editing;
        }
        ui.needsPartialRedraw = true;
    }
}

// =============================================================================
// DALI PSU SCREEN
// =============================================================================
static void psuPopupCallback(bool result);

void screenPsuDraw() {
    if (ui.needsFullRedraw) {
        uiDrawHeader("DALI PSU");
        ui.needsFullRedraw = false;
        uiClearContent();
    }
    
    // Dynamic footer based on editing state
    const char* footerText = psuState.editing ? 
        "Rotate:Value Click:Confirm" : 
        "Rotate:Select Click:Edit Hold:Back";
    uiDrawFooter(footerText);
    
    int16_t y = HEADER_HEIGHT + 30;
    
    const char* modeStr = psuState.internal ? "INTERNAL" : "EXTERNAL";
    uiDrawField(y, "PSU Mode", modeStr, psuState.field == 0,
                psuState.field == 0 && psuState.editing);
    y += 40;
    
    uiDrawButton(y, "APPLY", psuState.field == 1);
    y += 40;
    
    // Clear the text area before drawing to prevent overlap when switching
    tft.fillRect(0, y, TFT_WIDTH, 20, COLOR_BG);
    
    if (psuState.internal) {
        uiDrawTextCentered(y, "INT: ESP32 controls PSU", COLOR_WARNING, CONTENT_TEXT_SIZE);
    } else {
        uiDrawTextCentered(y, "EXT: External PSU", COLOR_TEXT, CONTENT_TEXT_SIZE);
    }
}

static void psuPopupCallback(bool result) {
    if (result) {
        daliPsuSet(true);
    } else {
        psuState.internal = false;
    }
    ui.needsFullRedraw = true;
}

void screenPsuInput(int16_t delta, ButtonEvent btn) {
    if (btn == BTN_LONG_PRESS) {
        if (psuState.editing) {
            psuState.editing = false;
        } else {
            uiGoBack();
            return;
        }
        ui.needsPartialRedraw = true;
        return;
    }
    
    if (delta != 0) {
        if (psuState.editing && psuState.field == 0) {
            psuState.internal = !psuState.internal;
        } else if (!psuState.editing) {
            psuState.field = uiClamp(psuState.field + delta, 0, 1);
        }
        ui.needsPartialRedraw = true;
    }
    
    if (btn == BTN_CLICK) {
        if (psuState.field == 1) {
            // Apply
            if (psuState.internal && !daliPsuGet()) {
                // Enabling internal PSU - show warning
                uiShowPopup("WARNING", "Enable internal PSU?", psuPopupCallback);
            } else {
                daliPsuSet(psuState.internal);
            }
        } else {
            // Toggle editing mode
            psuState.editing = !psuState.editing;
        }
        ui.needsPartialRedraw = true;
    }
}

// =============================================================================
// SEND CMD SCREEN
// =============================================================================
void screenSendCmdDraw() {
    if (ui.needsFullRedraw) {
        uiDrawHeader("Send CMD");
        ui.needsFullRedraw = false;
        uiClearContent();
    }
    
    // Dynamic footer based on editing state
    const char* footerText = sendCmdState.editing ? 
        "Rotate:Value Click:Confirm" : 
        "Rotate:Select Click:Edit Hold:Back";
    uiDrawFooter(footerText);
    
    int16_t y = HEADER_HEIGHT + 8;
    
    // Target type
    uiDrawField(y, "Target", getTargetTypeString(sendCmdState.targetType), 
                sendCmdState.field == 0, sendCmdState.field == 0 && sendCmdState.editing);
    y += 20;
    
    // Address (only for Group/Short)
    char buf[16];
    if (sendCmdState.targetType == 0) {
        strcpy(buf, "N/A");
    } else if (sendCmdState.targetType == 1) {
        snprintf(buf, sizeof(buf), "G%d", sendCmdState.addr);
    } else {
        snprintf(buf, sizeof(buf), "SA%d", sendCmdState.addr);
    }
    uiDrawField(y, "Address", buf, sendCmdState.field == 1,
                sendCmdState.field == 1 && sendCmdState.editing);
    y += 20;
    
    // Command
    uiDrawField(y, "Command", getCmdString(sendCmdState.cmd), 
                sendCmdState.field == 2, sendCmdState.field == 2 && sendCmdState.editing);
    y += 20;
    
    // Scene number (always reserve space, show only for SCENE command)
    if (sendCmdState.cmd == 4) {
        char sceneBuf[8];
        snprintf(sceneBuf, sizeof(sceneBuf), "%d", sendCmdState.sceneNum);
        uiDrawField(y, "Scene#", sceneBuf, sendCmdState.field == 3,
                    sendCmdState.field == 3 && sendCmdState.editing);
    }
    y += 20;  // Always advance past scene row
    
    y += 10;
    // Execute button (fixed position)
    uint8_t execField = (sendCmdState.cmd == 4) ? 4 : 3;
    uiDrawButton(y, "EXECUTE", sendCmdState.field == execField);
}

void screenSendCmdInput(int16_t delta, ButtonEvent btn) {
    if (btn == BTN_LONG_PRESS) {
        if (sendCmdState.editing) {
            // Exit editing mode
            sendCmdState.editing = false;
        } else {
            uiGoBack();
            return;
        }
        ui.needsPartialRedraw = true;
        return;
    }
    
    if (delta != 0) {
        if (sendCmdState.editing) {
            // Change value of current field
            switch(sendCmdState.field) {
                case 0:  // Target type
                    sendCmdState.targetType = uiWrap(sendCmdState.targetType + delta, 0, 2);
                    break;
                case 1:  // Address
                    if (sendCmdState.targetType == 1) {  // Group
                        sendCmdState.addr = uiClamp(sendCmdState.addr + delta, 0, 15);
                    } else if (sendCmdState.targetType == 2) {  // Short
                        sendCmdState.addr = uiClamp(sendCmdState.addr + delta, 0, 63);
                    }
                    break;
                case 2:  // Command
                    sendCmdState.cmd = uiWrap(sendCmdState.cmd + delta, 0, 4);
                    break;
                case 3:  // Scene number (only reachable when cmd==SCENE)
                    sendCmdState.sceneNum = uiClamp(sendCmdState.sceneNum + delta, 0, 15);
                    break;
            }
        } else {
            // Navigate between fields
            uint8_t execField = (sendCmdState.cmd == 4) ? 4 : 3;
            int8_t maxField = execField;
            sendCmdState.field = uiClamp(sendCmdState.field + delta, 0, maxField);
            
            // Skip address field for broadcast
            if (sendCmdState.field == 1 && sendCmdState.targetType == 0) {
                sendCmdState.field = (delta > 0) ? 2 : 0;
            }
            // Skip scene field if not SCENE command
            if (sendCmdState.field == 3 && sendCmdState.cmd != 4) {
                sendCmdState.field = (delta > 0) ? execField : 2;
            }
        }
        ui.needsPartialRedraw = true;
    }
    
    if (btn == BTN_CLICK) {
        uint8_t execField = (sendCmdState.cmd == 4) ? 4 : 3;
        if (sendCmdState.field == execField) {
            // Execute command
            uint8_t addrParam;
            if (sendCmdState.targetType == 0) {
                addrParam = 0xFF;  // Broadcast
            } else if (sendCmdState.targetType == 1) {
                addrParam = 0x80 | (sendCmdState.addr << 1);  // Group
            } else {
                addrParam = (sendCmdState.addr << 1);  // Short address
            }
            
            uint8_t cmdByte;
            switch(sendCmdState.cmd) {
                case 0: cmdByte = DALI_CMD_OFF; break;
                case 1: cmdByte = DALI_CMD_RECALL_MAX; break;  // ON = MAX
                case 2: cmdByte = DALI_CMD_RECALL_MIN; break;
                case 3: cmdByte = DALI_CMD_RECALL_MAX; break;
                case 4: cmdByte = DALI_CMD_GOTO_SCENE + sendCmdState.sceneNum; break;
                default: cmdByte = DALI_CMD_OFF; break;
            }
            
            daliCmdUi(addrParam | 0x01, cmdByte, false);  // Set S bit for command
        } else {
            // Toggle editing mode for current field
            sendCmdState.editing = !sendCmdState.editing;
        }
        ui.needsPartialRedraw = true;
    }
}

// =============================================================================
// BROADCAST CONTROL SCREEN
// =============================================================================
void screenBroadcastCtrlDraw() {
    if (ui.needsFullRedraw) {
        uiDrawHeader("Broadcast Control");
        uiClearContent();
        uiDrawFooter("Rotate:Dim Click:ON/OFF Hold:Back");
        ui.needsFullRedraw = false;
    }
    
    // Always clear content area for partial redraws to prevent text overlap
    if (ui.needsPartialRedraw) {
        uiClearContent();
        ui.needsPartialRedraw = false;
    }
    
    int16_t y = HEADER_HEIGHT + 30;
    char buf[32];
    
    // On/Off state
    const char* stateStr = broadcastCtrlState.isOn ? "ON" : "OFF";
    uint16_t stateColor = broadcastCtrlState.isOn ? COLOR_SUCCESS : COLOR_ERROR;
    snprintf(buf, sizeof(buf), "State: %s", stateStr);
    uiDrawTextCentered(y, buf, stateColor, CONTENT_TEXT_SIZE);
    y += MENU_ITEM_HEIGHT + 10;
    
    // Level display - clear area first then draw
    int16_t levelY = y;
    snprintf(buf, sizeof(buf), "Level: %d", broadcastCtrlState.level);
    uiDrawTextCentered(levelY, buf, COLOR_HIGHLIGHT, CONTENT_TEXT_SIZE);
    y += MENU_ITEM_HEIGHT + 20;
    
    // Visual level bar
    int16_t barX = 30;
    int16_t barW = TFT_WIDTH - 60;
    int16_t barH = 20;
    tft.fillRect(barX, y, barW, barH, COLOR_BADGE_IDLE);
    int16_t fillW = (barW * broadcastCtrlState.level) / 254;
    tft.fillRect(barX, y, fillW, barH, COLOR_HIGHLIGHT);
    tft.drawRect(barX, y, barW, barH, COLOR_TEXT);
    y += barH + 20;
    
    // Instructions
    uiDrawTextCentered(y, "Drehen = Dimmen", COLOR_TEXT, 1);
    y += 14;
    uiDrawTextCentered(y, "Click = AN/AUS", COLOR_TEXT, 1);
}

void screenBroadcastCtrlInput(int16_t delta, ButtonEvent btn) {
    if (btn == BTN_LONG_PRESS) {
        uiGoBack();
        return;
    }
    
    // Rotate = dim level (in steps of 5 for faster control)
    if (delta != 0) {
        int16_t newLevel = broadcastCtrlState.level + (delta * 5);
        broadcastCtrlState.level = uiClamp(newLevel, 0, 254);
        
        // Send directly to broadcast (direct arc power control)
        daliDirectArc(0x7F, broadcastCtrlState.level);  // 0x7F = broadcast short addr format
        
        // Update state
        broadcastCtrlState.isOn = (broadcastCtrlState.level > 0);
        ui.needsPartialRedraw = true;
    }
    
    // Click = toggle ON/OFF
    if (btn == BTN_CLICK) {
        broadcastCtrlState.isOn = !broadcastCtrlState.isOn;
        
        if (broadcastCtrlState.isOn) {
            // Turn on to current level (or default to 128 if was 0)
            if (broadcastCtrlState.level == 0) {
                broadcastCtrlState.level = 128;
            }
            daliDirectArc(0x7F, broadcastCtrlState.level);
        } else {
            // Turn off
            daliCmdUi(0xFF, DALI_CMD_OFF, false);  // Broadcast OFF
        }
        
        ui.needsFullRedraw = true;
    }
}

// =============================================================================
// LOCATE SCREEN
// =============================================================================
void screenLocateDraw() {
    if (ui.needsFullRedraw) {
        uiDrawHeader("Locate Device");
        ui.needsFullRedraw = false;
        uiClearContent();
    }
    
    const char* footerText;
    if (locateState.running) {
        footerText = "Click:Stop Hold:Stop+Back";
    } else if (locateState.editing) {
        footerText = "Rotate:Value Click:Confirm";
    } else {
        footerText = "Rotate:Select Click:Edit Hold:Back";
    }
    uiDrawFooter(footerText);
    
    int16_t y = HEADER_HEIGHT + 8;
    
    // Target type
    uiDrawField(y, "Target", getTargetTypeString(locateState.targetType), 
                locateState.field == 0, locateState.field == 0 && locateState.editing);
    y += 20;
    
    // Address
    char buf[16];
    if (locateState.targetType == 0) {
        strcpy(buf, "N/A");
    } else if (locateState.targetType == 1) {
        snprintf(buf, sizeof(buf), "G%d", locateState.addr);
    } else {
        snprintf(buf, sizeof(buf), "SA%d", locateState.addr);
    }
    uiDrawField(y, "Address", buf, locateState.field == 1,
                locateState.field == 1 && locateState.editing);
    y += 20;
    
    // Interval
    snprintf(buf, sizeof(buf), "%dms", locateState.interval);
    uiDrawField(y, "Interval", buf, locateState.field == 2,
                locateState.field == 2 && locateState.editing);
    y += 30;
    
    // Start/Stop button
    const char* btnText = locateState.running ? "STOP" : "START";
    uiDrawButton(y, btnText, locateState.field == 3);
    
    // Status
    if (locateState.running) {
        y += 40;
        uiDrawTextCentered(y, locateState.isMax ? ">>> MAX <<<" : ">>> MIN <<<", COLOR_HIGHLIGHT, CONTENT_TEXT_SIZE);
    }
}

void screenLocateUpdate() {
    if (!locateState.running) return;
    
    uint32_t now = millis();
    if (now - locateState.lastToggle >= locateState.interval) {
        locateState.lastToggle = now;
        locateState.isMax = !locateState.isMax;
        
        // Build address
        uint8_t addrParam;
        if (locateState.targetType == 0) {
            addrParam = 0xFF;  // Broadcast
        } else if (locateState.targetType == 1) {
            addrParam = 0x80 | (locateState.addr << 1);  // Group
        } else {
            addrParam = (locateState.addr << 1);  // Short address
        }
        
        uint8_t cmdByte = locateState.isMax ? DALI_CMD_RECALL_MAX : DALI_CMD_RECALL_MIN;
        daliCmdUi(addrParam | 0x01, cmdByte, false);
        
        ui.needsPartialRedraw = true;
    }
}

void screenLocateInput(int16_t delta, ButtonEvent btn) {
    if (btn == BTN_LONG_PRESS) {
        if (locateState.editing) {
            locateState.editing = false;
            ui.needsPartialRedraw = true;
            return;
        }
        locateState.running = false;
        uiGoBack();
        return;
    }
    
    if (locateState.running) {
        if (btn == BTN_CLICK) {
            locateState.running = false;
            ui.needsFullRedraw = true;
        }
        return;
    }
    
    if (delta != 0) {
        if (locateState.editing) {
            // Change value
            switch(locateState.field) {
                case 0:  // Target type
                    locateState.targetType = uiWrap(locateState.targetType + delta, 0, 2);
                    break;
                case 1:  // Address
                    if (locateState.targetType == 1) {
                        locateState.addr = uiClamp(locateState.addr + delta, 0, 15);
                    } else if (locateState.targetType == 2) {
                        locateState.addr = uiClamp(locateState.addr + delta, 0, 63);
                    }
                    break;
                case 2:  // Interval
                    locateState.interval = uiClamp(locateState.interval + delta * LOCATE_INTERVAL_STEP, 
                                                   LOCATE_INTERVAL_MIN, LOCATE_INTERVAL_MAX);
                    break;
            }
        } else {
            // Navigate fields
            locateState.field = uiClamp(locateState.field + delta, 0, 3);
            
            // Skip address for broadcast
            if (locateState.field == 1 && locateState.targetType == 0) {
                locateState.field = (delta > 0) ? 2 : 0;
            }
        }
        ui.needsPartialRedraw = true;
    }
    
    if (btn == BTN_CLICK) {
        if (locateState.field == 3) {
            // Start locate
            locateState.running = true;
            locateState.isMax = false;
            locateState.lastToggle = millis();
            ui.needsFullRedraw = true;
        } else {
            // Toggle editing mode
            locateState.editing = !locateState.editing;
        }
        ui.needsPartialRedraw = true;
    }
}

// =============================================================================
// BUS MONITOR SCREEN
// =============================================================================
void screenBusMonDraw() {
    uint8_t count = daliMonitorGetCount();
    bool forceFullRedraw = ui.needsFullRedraw;
    
    if (ui.needsFullRedraw) {
        uiDrawHeader("Bus Monitor");
        uiClearContent();
        ui.needsFullRedraw = false;
        busMonState.needsRedraw = true;  // Force redraw of all entries
    }
    
    // Only redraw footer on full redraw or mode change
    static bool lastFrozen = false;
    if (busMonState.frozen != lastFrozen || forceFullRedraw) {
        const char* footerText = busMonState.frozen ? 
            "FROZEN - Click:Unfreeze Rotate:Scroll" : 
            "LIVE - Click:Freeze Hold:Back";
        uiDrawFooter(footerText);
        lastFrozen = busMonState.frozen;
    }
    
    // Redraw content when data changed or forced
    if (busMonState.needsRedraw || forceFullRedraw) {
        busMonState.needsRedraw = false;
        
        // Status line
        tft.fillRect(0, HEADER_HEIGHT, TFT_WIDTH, 14, COLOR_BG);
        const char* modeStr = busMonState.frozen ? "[FROZEN]" : "[LIVE]";
        uint16_t modeColor = busMonState.frozen ? COLOR_WARNING : COLOR_HIGHLIGHT;
        uiDrawText(8, HEADER_HEIGHT + 3, modeStr, modeColor, 1);
        
        char countBuf[16];
        snprintf(countBuf, sizeof(countBuf), "Entries:%d", count);
        uiDrawTextRight(HEADER_HEIGHT + 3, countBuf, COLOR_TEXT, 1);
        
        // Entry list
        int16_t y = HEADER_HEIGHT + 16;
        int16_t lineHeight = 14;
        int16_t maxLines = (uiGetContentHeight() - 16) / lineHeight;
        
        tft.fillRect(0, y, TFT_WIDTH, maxLines * lineHeight, COLOR_BG);
        
        BusMonitorEntry entry;
        char frameBuf[24];
        
        for (int i = 0; i < maxLines && i < count; i++) {
            int16_t idx = i + busMonState.scrollOffset;
            if (idx >= count) break;
            
            if (daliMonitorGetEntry(idx, &entry)) {
                formatBusFrame(&entry, frameBuf, sizeof(frameBuf));
                
                uint32_t age = millis() - entry.timestamp;
                char timeBuf[8];
                if (age < 1000) {
                    snprintf(timeBuf, sizeof(timeBuf), "%3dms", (int)age);
                } else if (age < 60000) {
                    snprintf(timeBuf, sizeof(timeBuf), "%4ds", (int)(age / 1000));
                } else {
                    snprintf(timeBuf, sizeof(timeBuf), "%3dm", (int)(age / 60000));
                }
                
                tft.setTextSize(1);
                tft.setTextColor(COLOR_TEXT);
                tft.setCursor(4, y + 3);
                tft.print(timeBuf);
                tft.setCursor(42, y + 3);
                tft.print(frameBuf);
                
                y += lineHeight;
            }
        }
        
        if (count == 0) {
            uiDrawTextCentered(100, "No frames captured", COLOR_TEXT, 1);
            uiDrawTextCentered(118, "Waiting for DALI traffic...", COLOR_HIGHLIGHT, 1);
        }
    }
}

void screenBusMonUpdate() {
    if (busMonState.frozen) return;
    
    // Update bus monitor data
    daliMonitorUpdate();
    
    // Redraw when new frames arrive (generation changes even when buffer is full)
    uint16_t gen = daliMonitorGetGeneration();
    if (gen != busMonState.lastGeneration) {
        busMonState.needsRedraw = true;
        busMonState.lastGeneration = gen;
        busMonState.lastRefresh = millis();
        ui.needsPartialRedraw = true;
    }
    // No periodic refresh - only draw when data changes
}

void screenBusMonInput(int16_t delta, ButtonEvent btn) {
    if (btn == BTN_LONG_PRESS) {
        uiGoBack();
        return;
    }
    
    if (btn == BTN_CLICK) {
        busMonState.frozen = !busMonState.frozen;
        busMonState.scrollOffset = 0;
        ui.needsFullRedraw = true;
    }
    
    if (busMonState.frozen && delta != 0) {
        busMonState.scrollOffset += delta;
        uint8_t count = daliMonitorGetCount();
        int16_t lineHeight = 14;  // Match the draw function
        int16_t maxLines = (uiGetContentHeight() - 16) / lineHeight;
        busMonState.scrollOffset = uiClamp(busMonState.scrollOffset, 0, 
                                           count > maxLines ? count - maxLines : 0);
        ui.needsPartialRedraw = true;
    }
}

// =============================================================================
// POPUP SCREEN
// =============================================================================

// Helper function to draw only popup buttons
static void drawPopupButtons() {
    int16_t popupW = 200;
    int16_t popupH = 80;
    int16_t popupX = (TFT_WIDTH - popupW) / 2;
    int16_t popupY = (TFT_HEIGHT - popupH) / 2;
    
    int16_t btnY = popupY + 50;
    int16_t btnW = 50;
    int16_t btnSpacing = 20;
    int16_t yesX = popupX + (popupW / 2) - btnW - (btnSpacing / 2);
    int16_t noX = popupX + (popupW / 2) + (btnSpacing / 2);
    
    uint16_t yesBg = (popupState.selected == 0) ? COLOR_HIGHLIGHT : COLOR_BADGE_IDLE;
    uint16_t noBg = (popupState.selected == 1) ? COLOR_ERROR : COLOR_BADGE_IDLE;
    
    tft.fillRoundRect(yesX, btnY, btnW, 20, 4, yesBg);
    tft.fillRoundRect(noX, btnY, btnW, 20, 4, noBg);
    
    tft.setTextColor(COLOR_TEXT);
    tft.setCursor(yesX + 15, btnY + 6);
    tft.print("YES");
    tft.setCursor(noX + 18, btnY + 6);
    tft.print("NO");
    
    popupState.lastSelected = popupState.selected;
}

void screenPopupDraw() {
    // Check if only buttons need redraw
    if (!ui.needsFullRedraw && popupState.needsRedraw) {
        drawPopupButtons();
        popupState.needsRedraw = false;
        return;
    }
    
    // Full redraw
    int16_t popupW = 200;
    int16_t popupH = 80;
    int16_t popupX = (TFT_WIDTH - popupW) / 2;
    int16_t popupY = (TFT_HEIGHT - popupH) / 2;
    
    // Red border for warning
    tft.fillRect(popupX - 3, popupY - 3, popupW + 6, popupH + 6, COLOR_ERROR);
    tft.fillRect(popupX, popupY, popupW, popupH, COLOR_BG);
    
    // Set small text size for popup content
    tft.setTextSize(1);
    
    // Title
    if (ui.popupTitle) {
        int16_t titleW = strlen(ui.popupTitle) * 6;
        tft.setTextColor(COLOR_WARNING);
        tft.setCursor(popupX + (popupW - titleW) / 2, popupY + 10);
        tft.print(ui.popupTitle);
    }
    
    // Message
    if (ui.popupMessage) {
        int16_t msgW = strlen(ui.popupMessage) * 6;
        tft.setTextColor(COLOR_TEXT);
        tft.setCursor(popupX + (popupW - msgW) / 2, popupY + 28);
        tft.print(ui.popupMessage);
    }
    
    // Buttons
    drawPopupButtons();
    
    ui.needsFullRedraw = false;
    popupState.needsRedraw = false;
}

// =============================================================================
// DEVICE PARAMETERS SCREEN
// =============================================================================
// Field indices for device parameters
#define DEVPARAM_FIELD_ADDR         0
#define DEVPARAM_FIELD_LOAD         1
#define DEVPARAM_FIELD_WRITE        2
#define DEVPARAM_FIELD_MAX_LEVEL    3
#define DEVPARAM_FIELD_MIN_LEVEL    4
#define DEVPARAM_FIELD_POWER_ON     5
#define DEVPARAM_FIELD_SYS_FAIL     6
#define DEVPARAM_FIELD_FADE_TIME    7
#define DEVPARAM_FIELD_FADE_RATE    8
#define DEVPARAM_FIELD_GROUP_START  9
#define DEVPARAM_FIELD_SCENE_START  (DEVPARAM_FIELD_GROUP_START + 16)
#define DEVPARAM_FIELD_COUNT        (DEVPARAM_FIELD_SCENE_START + 16)

static void deviceParamsLoad() {
    uint8_t addr = deviceParamsState.addr;
    
    // Query basic parameters
    int16_t val;
    val = daliQueryMaxLevel(addr);
    deviceParamsState.maxLevel = (val >= 0) ? val : 0;
    
    val = daliQueryMinLevel(addr);
    deviceParamsState.minLevel = (val >= 0) ? val : 0;
    
    val = daliQueryPowerOnLevel(addr);
    deviceParamsState.powerOnLevel = (val >= 0) ? val : 0;
    
    val = daliQuerySysFailLevel(addr);
    deviceParamsState.sysFailLevel = (val >= 0) ? val : 0;
    
    val = daliQueryFadeTime(addr);
    deviceParamsState.fadeTime = (val >= 0) ? val : 0;
    
    val = daliQueryFadeRate(addr);
    deviceParamsState.fadeRate = (val >= 0) ? val : 0;
    
    val = daliQueryActualLevel(addr);
    deviceParamsState.actualLevel = (val >= 0) ? val : 0;
    
    val = daliQueryDeviceType(addr);
    deviceParamsState.deviceType = (val >= 0) ? val : 255;
    
    // Query groups
    deviceParamsState.groups = daliQueryGroups(addr);
    
    // Query scene levels
    for (int i = 0; i < 16; i++) {
        val = daliQuerySceneLevel(addr, i);
        deviceParamsState.sceneLevels[i] = (val >= 0) ? val : 255;
    }
    
    deviceParamsState.loaded = true;
}

static bool deviceParamsWrite() {
    uint8_t addr = deviceParamsState.writeAddr;
    bool success = true;
    
    // Write all parameters to target address
    success = daliSetMaxLevel(addr, deviceParamsState.maxLevel) && success;
    success = daliSetMinLevel(addr, deviceParamsState.minLevel) && success;
    success = daliSetPowerOnLevel(addr, deviceParamsState.powerOnLevel) && success;
    success = daliSetSysFailLevel(addr, deviceParamsState.sysFailLevel) && success;
    success = daliSetFadeTime(addr, deviceParamsState.fadeTime) && success;
    success = daliSetFadeRate(addr, deviceParamsState.fadeRate) && success;
    
    // Write groups - first remove from all, then add to selected
    for (int g = 0; g < 16; g++) {
        if (deviceParamsState.groups & (1 << g)) {
            success = daliAddToGroup(addr, g) && success;
        } else {
            success = daliRemoveFromGroup(addr, g) && success;
        }
    }
    
    // Write scene levels
    for (int s = 0; s < 16; s++) {
        if (deviceParamsState.sceneLevels[s] != 255) {
            success = daliSetSceneLevel(addr, s, deviceParamsState.sceneLevels[s]) && success;
        }
    }

    return success;
}

static const char* getFieldName(int16_t field) {
    switch(field) {
        case DEVPARAM_FIELD_ADDR: return "Address";
        case DEVPARAM_FIELD_LOAD: return "> Load Data";
        case DEVPARAM_FIELD_WRITE: return "> Write to Addr";
        case DEVPARAM_FIELD_MAX_LEVEL: return "Max Level";
        case DEVPARAM_FIELD_MIN_LEVEL: return "Min Level";
        case DEVPARAM_FIELD_POWER_ON: return "Power On Level";
        case DEVPARAM_FIELD_SYS_FAIL: return "Sys Fail Level";
        case DEVPARAM_FIELD_FADE_TIME: return "Fade Time";
        case DEVPARAM_FIELD_FADE_RATE: return "Fade Rate";
        default:
            if (field >= DEVPARAM_FIELD_GROUP_START && field < DEVPARAM_FIELD_SCENE_START) {
                static char buf[16];
                snprintf(buf, sizeof(buf), "Group %d", field - DEVPARAM_FIELD_GROUP_START);
                return buf;
            }
            if (field >= DEVPARAM_FIELD_SCENE_START && field < DEVPARAM_FIELD_COUNT) {
                static char buf[16];
                snprintf(buf, sizeof(buf), "Scene %d", field - DEVPARAM_FIELD_SCENE_START);
                return buf;
            }
            return "???";
    }
}

static void getFieldValueStr(int16_t field, char* buf, size_t bufSize) {
    if (!deviceParamsState.loaded && field >= DEVPARAM_FIELD_MAX_LEVEL) {
        snprintf(buf, bufSize, "---");
        return;
    }
    
    switch(field) {
        case DEVPARAM_FIELD_ADDR:
            snprintf(buf, bufSize, "%d", deviceParamsState.addr);
            break;
        case DEVPARAM_FIELD_LOAD:
            snprintf(buf, bufSize, deviceParamsState.loaded ? "(Loaded)" : "(Click)");
            break;
        case DEVPARAM_FIELD_WRITE:
            if (deviceParamsState.writingAddr) {
                snprintf(buf, bufSize, "[%d]", deviceParamsState.writeAddr);
            } else {
                snprintf(buf, bufSize, "-> SA %d", deviceParamsState.writeAddr);
            }
            break;
        case DEVPARAM_FIELD_MAX_LEVEL:
            snprintf(buf, bufSize, "%d", deviceParamsState.maxLevel);
            break;
        case DEVPARAM_FIELD_MIN_LEVEL:
            snprintf(buf, bufSize, "%d", deviceParamsState.minLevel);
            break;
        case DEVPARAM_FIELD_POWER_ON:
            snprintf(buf, bufSize, "%d", deviceParamsState.powerOnLevel);
            break;
        case DEVPARAM_FIELD_SYS_FAIL:
            snprintf(buf, bufSize, "%d", deviceParamsState.sysFailLevel);
            break;
        case DEVPARAM_FIELD_FADE_TIME:
            snprintf(buf, bufSize, "%d", deviceParamsState.fadeTime);
            break;
        case DEVPARAM_FIELD_FADE_RATE:
            snprintf(buf, bufSize, "%d", deviceParamsState.fadeRate);
            break;
        default:
            // Groups (16 individual fields)
            if (field >= DEVPARAM_FIELD_GROUP_START && field < DEVPARAM_FIELD_SCENE_START) {
                uint8_t grp = field - DEVPARAM_FIELD_GROUP_START;
                bool inGroup = (deviceParamsState.groups & (1 << grp)) != 0;
                snprintf(buf, bufSize, inGroup ? "Yes" : "No");
            }
            // Scenes (16 individual fields)
            else if (field >= DEVPARAM_FIELD_SCENE_START && field < DEVPARAM_FIELD_COUNT) {
                uint8_t level = deviceParamsState.sceneLevels[field - DEVPARAM_FIELD_SCENE_START];
                if (level == 255) {
                    snprintf(buf, bufSize, "MASK");
                } else {
                    snprintf(buf, bufSize, "%d", level);
                }
            } else {
                snprintf(buf, bufSize, "?");
            }
            break;
    }
}

void screenDeviceParamsDraw() {
    if (ui.needsFullRedraw) {
        uiDrawHeader("Device Params");
        uiClearContent();
        ui.needsFullRedraw = false;
    }
    
    // Clear content for redraw
    uiClearContent();
    
    // Footer based on editing state
    if (deviceParamsState.writingAddr) {
        uiDrawFooter("Rotate:Addr Click:Write Hold:Cancel");
    } else if (deviceParamsState.editing) {
        uiDrawFooter("Rotate:Value Click:Save Hold:Cancel");
    } else {
        uiDrawFooter("Rotate:Nav Click:Edit Hold:Back");
    }
    
    int16_t y = HEADER_HEIGHT + 4;
    char nameBuf[20];
    char valBuf[24];
    
    // Show current device info in header area
    if (deviceParamsState.loaded) {
        snprintf(nameBuf, sizeof(nameBuf), "SA %d - %s", 
                 deviceParamsState.addr, 
                 daliGetDeviceTypeName(deviceParamsState.deviceType));
        uiDrawText(8, y, nameBuf, COLOR_HIGHLIGHT, 1);
        y += 12;
    }
    
    // Calculate visible items
    int16_t itemHeight = MENU_ITEM_HEIGHT;
    int16_t visible = (uiGetContentHeight() - 16) / itemHeight;
    if (visible < 1) visible = 1;
    
    // Draw scrollable list
    for (int i = 0; i < visible && (i + deviceParamsState.scrollOffset) < DEVPARAM_FIELD_COUNT; i++) {
        int16_t fieldIdx = i + deviceParamsState.scrollOffset;
        bool selected = (fieldIdx == deviceParamsState.field);
        
        int16_t itemY = y + i * itemHeight;
        
        // Background
        if (selected) {
            tft.fillRect(0, itemY, TFT_WIDTH, itemHeight, COLOR_SELECTED);
            tft.setTextColor(COLOR_BG);
        } else {
            tft.fillRect(0, itemY, TFT_WIDTH, itemHeight, COLOR_BG);
            tft.setTextColor(COLOR_TEXT);
        }
        
        // Field name
        const char* fieldName = getFieldName(fieldIdx);
        tft.setTextSize(CONTENT_TEXT_SIZE);
        tft.setCursor(8, itemY + (itemHeight - 8 * CONTENT_TEXT_SIZE) / 2);
        tft.print(fieldName);
        
        // Field value (right-aligned)
        getFieldValueStr(fieldIdx, valBuf, sizeof(valBuf));
        
        // If editing this field, show edit value instead
        if (deviceParamsState.editing && selected) {
            snprintf(valBuf, sizeof(valBuf), "[%d]", deviceParamsState.editValue);
            tft.setTextColor(selected ? COLOR_BG : COLOR_HIGHLIGHT);
        }
        
        int16_t valW = strlen(valBuf) * 6 * CONTENT_TEXT_SIZE;
        tft.setCursor(TFT_WIDTH - valW - 8, itemY + (itemHeight - 8 * CONTENT_TEXT_SIZE) / 2);
        tft.print(valBuf);
    }
    
    // Scrollbar
    uiDrawScrollbar(DEVPARAM_FIELD_COUNT, visible, deviceParamsState.scrollOffset);
}

void screenDeviceParamsInput(int16_t delta, ButtonEvent btn) {
    if (btn == BTN_LONG_PRESS) {
        if (deviceParamsState.editing) {
            // Cancel edit
            deviceParamsState.editing = false;
            ui.needsPartialRedraw = true;
        } else if (deviceParamsState.writingAddr) {
            // Cancel write address selection
            deviceParamsState.writingAddr = false;
            ui.needsPartialRedraw = true;
        } else {
            uiGoBack();
        }
        return;
    }
    
    // Write address selection mode
    if (deviceParamsState.writingAddr) {
        if (delta != 0) {
            int16_t newVal = (int16_t)deviceParamsState.writeAddr + delta;
            deviceParamsState.writeAddr = uiClamp(newVal, 0, 63);
            ui.needsPartialRedraw = true;
        }
        
        if (btn == BTN_CLICK) {
            // Execute write operation
            bool success = deviceParamsWrite();
            daliSetStatus(success ? DALI_STATUS_OK : DALI_STATUS_FAILED);
            deviceParamsState.writingAddr = false;
            ui.needsFullRedraw = true;
        }
        return;
    }
    
    if (deviceParamsState.editing) {
        // In edit mode
        if (delta != 0) {
            int16_t newVal = (int16_t)deviceParamsState.editValue + delta;
            
            // Clamp based on field type
            if (deviceParamsState.field == DEVPARAM_FIELD_ADDR) {
                newVal = uiClamp(newVal, 0, 63);
            } else {
                newVal = uiClamp(newVal, 0, 255);
            }
            
            deviceParamsState.editValue = newVal;
            ui.needsPartialRedraw = true;
        }
        
        if (btn == BTN_CLICK) {
            // Save the edited value
            uint8_t addr = deviceParamsState.addr;
            bool success = true;
            
            switch(deviceParamsState.field) {
                case DEVPARAM_FIELD_ADDR:
                    deviceParamsState.addr = deviceParamsState.editValue;
                    deviceParamsState.writeAddr = deviceParamsState.editValue;  // Sync write addr
                    deviceParamsState.loaded = false;  // Need to reload for new address
                    break;
                case DEVPARAM_FIELD_MAX_LEVEL:
                    success = daliSetMaxLevel(addr, deviceParamsState.editValue);
                    if (success) deviceParamsState.maxLevel = deviceParamsState.editValue;
                    break;
                case DEVPARAM_FIELD_MIN_LEVEL:
                    success = daliSetMinLevel(addr, deviceParamsState.editValue);
                    if (success) deviceParamsState.minLevel = deviceParamsState.editValue;
                    break;
                case DEVPARAM_FIELD_POWER_ON:
                    success = daliSetPowerOnLevel(addr, deviceParamsState.editValue);
                    if (success) deviceParamsState.powerOnLevel = deviceParamsState.editValue;
                    break;
                case DEVPARAM_FIELD_SYS_FAIL:
                    success = daliSetSysFailLevel(addr, deviceParamsState.editValue);
                    if (success) deviceParamsState.sysFailLevel = deviceParamsState.editValue;
                    break;
                case DEVPARAM_FIELD_FADE_TIME:
                    success = daliSetFadeTime(addr, deviceParamsState.editValue);
                    if (success) deviceParamsState.fadeTime = deviceParamsState.editValue;
                    break;
                case DEVPARAM_FIELD_FADE_RATE:
                    success = daliSetFadeRate(addr, deviceParamsState.editValue);
                    if (success) deviceParamsState.fadeRate = deviceParamsState.editValue;
                    break;
                default:
                    // Scene levels
                    if (deviceParamsState.field >= DEVPARAM_FIELD_SCENE_START && 
                        deviceParamsState.field < DEVPARAM_FIELD_COUNT) {
                        uint8_t scene = deviceParamsState.field - DEVPARAM_FIELD_SCENE_START;
                        success = daliSetSceneLevel(addr, scene, deviceParamsState.editValue);
                        if (success) deviceParamsState.sceneLevels[scene] = deviceParamsState.editValue;
                    }
                    break;
            }
            
            deviceParamsState.editing = false;
            ui.needsFullRedraw = true;
        }
    } else {
        // Navigation mode
        if (delta != 0) {
            deviceParamsState.field += delta;
            deviceParamsState.field = uiClamp(deviceParamsState.field, 0, DEVPARAM_FIELD_COUNT - 1);
            
            // Adjust scroll
            int16_t visible = (uiGetContentHeight() - 16) / MENU_ITEM_HEIGHT;
            if (visible < 1) visible = 1;
            
            if (deviceParamsState.field < deviceParamsState.scrollOffset) {
                deviceParamsState.scrollOffset = deviceParamsState.field;
            } else if (deviceParamsState.field >= deviceParamsState.scrollOffset + visible) {
                deviceParamsState.scrollOffset = deviceParamsState.field - visible + 1;
            }
            ui.needsPartialRedraw = true;
        }
        
        if (btn == BTN_CLICK) {
            if (deviceParamsState.field == DEVPARAM_FIELD_LOAD) {
                // Load data from device
                deviceParamsLoad();
                ui.needsFullRedraw = true;
            } else if (deviceParamsState.field == DEVPARAM_FIELD_WRITE) {
                // Enter write address selection mode
                if (deviceParamsState.loaded) {
                    deviceParamsState.writingAddr = true;
                    ui.needsPartialRedraw = true;
                }
            } else if (deviceParamsState.field == DEVPARAM_FIELD_ADDR) {
                // Edit source address
                deviceParamsState.editing = true;
                deviceParamsState.editValue = deviceParamsState.addr;
                ui.needsPartialRedraw = true;
            } else if (deviceParamsState.loaded) {
                // Handle groups - toggle on click
                if (deviceParamsState.field >= DEVPARAM_FIELD_GROUP_START && 
                    deviceParamsState.field < DEVPARAM_FIELD_SCENE_START) {
                    uint8_t grp = deviceParamsState.field - DEVPARAM_FIELD_GROUP_START;
                    uint8_t addr = deviceParamsState.addr;
                    bool success;
                    
                    if (deviceParamsState.groups & (1 << grp)) {
                        // Remove from group
                        success = daliRemoveFromGroup(addr, grp);
                        if (success) deviceParamsState.groups &= ~(1 << grp);
                    } else {
                        // Add to group
                        success = daliAddToGroup(addr, grp);
                        if (success) deviceParamsState.groups |= (1 << grp);
                    }
                    ui.needsPartialRedraw = true;
                }
                // Handle numeric fields - enter edit mode
                else if (deviceParamsState.field >= DEVPARAM_FIELD_MAX_LEVEL) {
                    deviceParamsState.editing = true;
                    
                    // Initialize edit value based on field
                    switch(deviceParamsState.field) {
                        case DEVPARAM_FIELD_MAX_LEVEL:
                            deviceParamsState.editValue = deviceParamsState.maxLevel;
                            break;
                        case DEVPARAM_FIELD_MIN_LEVEL:
                            deviceParamsState.editValue = deviceParamsState.minLevel;
                            break;
                        case DEVPARAM_FIELD_POWER_ON:
                            deviceParamsState.editValue = deviceParamsState.powerOnLevel;
                            break;
                        case DEVPARAM_FIELD_SYS_FAIL:
                            deviceParamsState.editValue = deviceParamsState.sysFailLevel;
                            break;
                        case DEVPARAM_FIELD_FADE_TIME:
                            deviceParamsState.editValue = deviceParamsState.fadeTime;
                            break;
                        case DEVPARAM_FIELD_FADE_RATE:
                            deviceParamsState.editValue = deviceParamsState.fadeRate;
                            break;
                        default:
                            // Scene levels
                            if (deviceParamsState.field >= DEVPARAM_FIELD_SCENE_START && 
                                deviceParamsState.field < DEVPARAM_FIELD_COUNT) {
                                uint8_t scene = deviceParamsState.field - DEVPARAM_FIELD_SCENE_START;
                                deviceParamsState.editValue = deviceParamsState.sceneLevels[scene];
                            }
                            break;
                    }
                    ui.needsPartialRedraw = true;
                }
            }
        }
    }
}

void screenPopupInput(int16_t delta, ButtonEvent btn) {
    if (delta != 0) {
        popupState.selected = (popupState.selected == 0) ? 1 : 0;
        popupState.needsRedraw = true;
        ui.needsPartialRedraw = true;
    }
    
    if (btn == BTN_CLICK || btn == BTN_LONG_PRESS) {
        bool result = (popupState.selected == 0);
        
        // Return to previous screen and restore saved previousScreen
        ui.currentScreen = ui.previousScreen;
        ui.previousScreen = ui.popupReturnPrevious;
        ui.needsFullRedraw = true;
        
        // Call callback
        if (ui.popupCallback) {
            ui.popupCallback(result);
        }
    }
}

// =============================================================================
// PONG EASTER EGG
// =============================================================================

#define PONG_PADDLE_H    36
#define PONG_PADDLE_W    4
#define PONG_PADDLE_X    (TFT_WIDTH - 12)
#define PONG_BALL_SIZE   4
#define PONG_WALL_W      4
#define PONG_BALL_SPEED  2.0f
#define PONG_UPDATE_MS   25
#define PONG_FIELD_TOP   HEADER_HEIGHT
#define PONG_FIELD_BOT   (TFT_HEIGHT - FOOTER_HEIGHT)

static void pongDrawFooter() {
    char buf[44];
    if (pongState.gameOver) {
        snprintf(buf, sizeof(buf), "GAME OVER! Score:%d Click:Retry", pongState.score);
    } else if (!pongState.started) {
        snprintf(buf, sizeof(buf), "Score:%d  Click:Start Hold:Back", pongState.score);
    } else {
        snprintf(buf, sizeof(buf), "Score:%d               Hold:Back", pongState.score);
    }
    uiDrawFooter(buf);
}

static void pongResetBall() {
    int16_t fieldH = PONG_FIELD_BOT - PONG_FIELD_TOP;
    pongState.ballX = TFT_WIDTH / 2.0f;
    pongState.ballY = PONG_FIELD_TOP + fieldH / 2.0f;
    pongState.ballVX = -PONG_BALL_SPEED;
    pongState.ballVY = (random(0, 2) == 0 ? 1.0f : -1.0f) * (0.8f + random(0, 10) * 0.1f);
}

void screenPongDraw() {
    if (!ui.needsFullRedraw) return;

    uiDrawHeader("PONG");
    uiClearContent();

    int16_t fieldH = PONG_FIELD_BOT - PONG_FIELD_TOP;
    pongState.paddleY = PONG_FIELD_TOP + (fieldH - PONG_PADDLE_H) / 2.0f;
    pongState.score = 0;
    pongState.gameOver = false;
    pongState.started = false;
    pongState.lastUpdate = millis();
    pongResetBall();

    // Draw left wall
    tft.fillRect(0, PONG_FIELD_TOP, PONG_WALL_W, fieldH, COLOR_BADGE_IDLE);

    // Top and bottom borders
    tft.drawFastHLine(0, PONG_FIELD_TOP, TFT_WIDTH, COLOR_BADGE_IDLE);
    tft.drawFastHLine(0, PONG_FIELD_BOT - 1, TFT_WIDTH, COLOR_BADGE_IDLE);

    // Paddle
    tft.fillRect(PONG_PADDLE_X, (int)pongState.paddleY, PONG_PADDLE_W, PONG_PADDLE_H, COLOR_HIGHLIGHT);

    // Ball
    tft.fillRect((int)pongState.ballX, (int)pongState.ballY, PONG_BALL_SIZE, PONG_BALL_SIZE, COLOR_TEXT);

    // Start hint
    uiDrawTextCentered(PONG_FIELD_TOP + fieldH / 2 - 4, "Click to Start!", COLOR_WARNING, 1);

    pongDrawFooter();
    ui.needsFullRedraw = false;
}

void screenPongInput(int16_t delta, ButtonEvent btn) {
    if (btn == BTN_LONG_PRESS) {
        uiSetScreen(SCREEN_HOME);
        return;
    }

    if (pongState.gameOver) {
        if (btn == BTN_CLICK) {
            ui.needsFullRedraw = true;
        }
        return;
    }

    if (!pongState.started) {
        if (btn == BTN_CLICK) {
            pongState.started = true;
            pongState.lastUpdate = millis();
            // Clear start message
            int16_t fieldH = PONG_FIELD_BOT - PONG_FIELD_TOP;
            int16_t msgY = PONG_FIELD_TOP + fieldH / 2 - 4;
            tft.fillRect(PONG_WALL_W, msgY, TFT_WIDTH - PONG_WALL_W, 10, COLOR_BG);
            pongDrawFooter();
        }
        return;
    }

    if (delta != 0) {
        // Erase old paddle
        tft.fillRect(PONG_PADDLE_X, (int)pongState.paddleY, PONG_PADDLE_W, PONG_PADDLE_H, COLOR_BG);

        pongState.paddleY += delta * 8;

        if (pongState.paddleY < PONG_FIELD_TOP + 1)
            pongState.paddleY = PONG_FIELD_TOP + 1;
        if (pongState.paddleY + PONG_PADDLE_H > PONG_FIELD_BOT - 1)
            pongState.paddleY = PONG_FIELD_BOT - 1 - PONG_PADDLE_H;

        // Draw new paddle
        tft.fillRect(PONG_PADDLE_X, (int)pongState.paddleY, PONG_PADDLE_W, PONG_PADDLE_H, COLOR_HIGHLIGHT);
    }
}

void screenPongUpdate() {
    if (!pongState.started || pongState.gameOver) return;

    uint32_t now = millis();
    if (now - pongState.lastUpdate < PONG_UPDATE_MS) return;
    pongState.lastUpdate = now;

    // Erase old ball
    tft.fillRect((int)pongState.ballX, (int)pongState.ballY, PONG_BALL_SIZE, PONG_BALL_SIZE, COLOR_BG);

    // Move ball
    pongState.ballX += pongState.ballVX;
    pongState.ballY += pongState.ballVY;

    // Top bounce
    if (pongState.ballY <= PONG_FIELD_TOP + 1) {
        pongState.ballY = PONG_FIELD_TOP + 1;
        if (pongState.ballVY < 0) pongState.ballVY = -pongState.ballVY;
    }

    // Bottom bounce
    if (pongState.ballY + PONG_BALL_SIZE >= PONG_FIELD_BOT - 1) {
        pongState.ballY = PONG_FIELD_BOT - 1 - PONG_BALL_SIZE;
        if (pongState.ballVY > 0) pongState.ballVY = -pongState.ballVY;
    }

    // Left wall bounce
    if (pongState.ballX <= PONG_WALL_W) {
        pongState.ballX = PONG_WALL_W;
        if (pongState.ballVX < 0) pongState.ballVX = -pongState.ballVX;
    }

    // Right side - paddle collision
    if (pongState.ballX + PONG_BALL_SIZE >= PONG_PADDLE_X) {
        if (pongState.ballY + PONG_BALL_SIZE >= pongState.paddleY &&
            pongState.ballY <= pongState.paddleY + PONG_PADDLE_H) {
            // Hit paddle
            pongState.ballX = PONG_PADDLE_X - PONG_BALL_SIZE;
            if (pongState.ballVX > 0) pongState.ballVX = -pongState.ballVX;

            // Adjust VY based on hit position on paddle
            float hitPos = (pongState.ballY + PONG_BALL_SIZE / 2.0f - pongState.paddleY) / PONG_PADDLE_H;
            pongState.ballVY = (hitPos - 0.5f) * 3.0f;

            pongState.score++;
            pongDrawFooter();
        } else if (pongState.ballX > TFT_WIDTH) {
            // Missed paddle - game over
            pongState.gameOver = true;

            int16_t fieldH = PONG_FIELD_BOT - PONG_FIELD_TOP;
            uiDrawTextCentered(PONG_FIELD_TOP + fieldH / 2 - 12, "GAME OVER", COLOR_ERROR, 2);
            char scoreBuf[20];
            snprintf(scoreBuf, sizeof(scoreBuf), "Score: %d", pongState.score);
            uiDrawTextCentered(PONG_FIELD_TOP + fieldH / 2 + 12, scoreBuf, COLOR_HIGHLIGHT, 2);

            pongDrawFooter();
            return;
        }
    }

    // Draw new ball
    tft.fillRect((int)pongState.ballX, (int)pongState.ballY, PONG_BALL_SIZE, PONG_BALL_SIZE, COLOR_TEXT);
}

// =============================================================================
// Screen Handler Table
// =============================================================================
const ScreenHandler screenHandlers[] = {
    { screenHomeDraw, screenHomeInput },                    // SCREEN_HOME
    { screenScanDraw, screenScanInput },                    // SCREEN_SCAN_DEVICES
    { screenScanDeviceInfoDraw, screenScanDeviceInfoInput },// SCREEN_SCAN_DEVICE_INFO
    { screenAddressDraw, screenAddressInput },              // SCREEN_ADDRESS_DEVICE
    { screenCloneDraw, screenCloneInput },                  // SCREEN_CLONE_CONFIG
    { screenResetDraw, screenResetInput },                  // SCREEN_RESET_DEVICE
    { screenPsuDraw, screenPsuInput },                      // SCREEN_DALI_PSU
    { screenSendCmdDraw, screenSendCmdInput },              // SCREEN_SEND_CMD
    { screenBroadcastCtrlDraw, screenBroadcastCtrlInput },  // SCREEN_BROADCAST_CTRL
    { screenLocateDraw, screenLocateInput },                // SCREEN_LOCATE
    { screenBusMonDraw, screenBusMonInput },                // SCREEN_BUS_MONITOR
    { screenDeviceParamsDraw, screenDeviceParamsInput },    // SCREEN_DEVICE_PARAMS
    { screenPopupDraw, screenPopupInput },                  // SCREEN_CONFIRM_POPUP
    { screenPongDraw, screenPongInput }                     // SCREEN_PONG
};
