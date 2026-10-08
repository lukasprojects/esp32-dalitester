/**
 * @file screens.h
 * @brief Screen handler declarations for all UI screens
 */

#ifndef SCREENS_H
#define SCREENS_H

#include "ui/ui.h"
#include "dali/dali_interface.h"
#include "storage/sd_wago.h"

// =============================================================================
// Screen Handler Function Type
// =============================================================================
typedef void (*ScreenDrawFunc)();
typedef void (*ScreenInputFunc)(int16_t delta, ButtonEvent btn);

// =============================================================================
// Screen Handler Structure
// =============================================================================
struct ScreenHandler {
    ScreenDrawFunc draw;
    ScreenInputFunc input;
};

// =============================================================================
// Screen State Structures
// =============================================================================

// Scan Devices Screen State
struct ScanState {
    uint8_t foundAddrs[64];         // Addressed control gear
    uint8_t deviceTypes[64];        // Gear device types
    uint8_t deviceCount;            // Number of control gear found
    uint8_t ctrlDevAddrs[64];       // DALI-2 control devices (sensors etc.)
    uint8_t ctrlDevTypes[64];       // Control device types
    uint8_t ctrlDevInstCount[64];   // Number of enabled instances per control device
    uint8_t ctrlDevOccCount[64];    // Occupancy sensor instances (type 3)
    uint8_t ctrlDevLightCount[64];  // Light sensor instances (type 4)
    uint8_t ctrlDevCount;           // Number of control devices found
    bool scanComplete;
    bool scanning;
    int16_t selectedDevice;
    uint8_t unaddressedCount;       // Number of unaddressed devices found on bus
    bool checkedUnaddressed;        // Unaddressed check completed
    bool noBusVoltage;              // Bus voltage check failed
};

// Scan Device Info Screen State (context menu)
struct ScanDeviceInfoState {
    uint8_t addr;
    uint8_t deviceType;
    uint32_t serialNumber;      // 24-bit random address
    bool serialValid;
    int16_t actualLevel;
    int16_t status;
    uint8_t field;              // 0=Info, 1=Locate
    bool locating;              // Locate active
    bool locateIsMax;           // Current locate state
    uint32_t locateLastToggle;  // Last toggle time
};

// Broadcast Control Screen State
struct BroadcastCtrlState {
    uint8_t level;              // Current dim level (0-254)
    bool isOn;                  // Current on/off state
};

// Address Device Screen State
struct AddressState {
    uint8_t mode;           // 0=All, 1=Unaddressed, 2=Change
    uint8_t currentAddr;    // For change mode
    uint8_t newAddr;
    uint8_t field;          // 0=Mode, 1=Current, 2=New, 3=Execute
    bool editing;           // true when editing current field value
};

// Clone Config Screen State
struct CloneState {
    DaliDeviceConfig config;
    uint8_t sourceAddr;
    uint8_t targetAddr;
    bool captured;
    uint8_t field;          // 0=Source, 1=Capture, 2=Target, 3=Send
    bool editing;           // true when editing current field value
};

// Reset Device Screen State
struct ResetState {
    uint8_t addr;
    uint8_t field;          // 0=Addr, 1=DeleteAddr, 2=Execute
    bool editing;           // true when editing current field value
    bool deleteAddress;     // true = also delete short address
};

// PSU Screen State
struct PsuState {
    bool internal;
    uint8_t field;          // 0=Mode, 1=Apply
    bool editing;           // true when editing current field value
};

// Send CMD Screen State
struct SendCmdState {
    uint8_t targetType;     // 0=Broadcast, 1=Group, 2=Short
    uint8_t addr;           // Group 0-15 or Short 0-63
    uint8_t cmd;            // 0=OFF, 1=ON, 2=MIN, 3=MAX, 4=SCENE
    uint8_t sceneNum;       // 0-15 for GOTO SCENE
    uint8_t field;          // 0=Target, 1=Addr, 2=CMD, 3=Scene/Execute, 4=Execute
    bool editing;           // true when editing current field value
};

// Locate Screen State
struct LocateState {
    uint8_t targetType;     // 0=Broadcast, 1=Group, 2=Short
    uint8_t addr;
    uint16_t interval;
    bool running;
    bool isMax;             // Current state for toggle
    uint32_t lastToggle;
    uint8_t field;          // 0=Target, 1=Addr, 2=Interval, 3=Start/Stop
    bool editing;           // true when editing current field value
};

// Bus Monitor Screen State
struct BusMonState {
    bool frozen;
    int16_t scrollOffset;
    uint32_t lastRefresh;
    uint16_t lastGeneration; // Track generation for change detection
    bool needsRedraw;        // Flag for content update
};

// Popup State
struct PopupState {
    uint8_t selected;       // 0=YES, 1=NO
    uint8_t lastSelected;   // For change detection
    bool needsRedraw;       // Partial redraw flag
};

// Device Parameters Screen State
struct DeviceParamsState {
    uint8_t addr;               // Device short address (source)
    uint8_t writeAddr;          // Target address for write operation
    bool loaded;                // Data loaded from device
    bool hasExpectedConfig;     // Data sourced from loaded WAGO config
    bool editing;               // Currently editing a value
    bool writingAddr;           // Editing write target address
    bool compareDone;           // Live-vs-config check done
    bool compareReadOk;         // Live device could be queried
    bool compareMatch;          // Live data matches expected config
    uint8_t diffCount;          // Number of differing fields
    int16_t scrollOffset;       // Scroll position
    int16_t field;              // Selected field (0-based)
    char expectedName[STORAGE_MAX_NAME_LEN];
    
    // Basic parameters
    uint8_t maxLevel;
    uint8_t minLevel;
    uint8_t powerOnLevel;
    uint8_t sysFailLevel;
    uint8_t fadeTime;
    uint8_t fadeRate;
    uint8_t actualLevel;
    uint8_t deviceType;
    
    // Groups (bitmask)
    uint16_t groups;
    
    // Scene levels (0-15)
    uint8_t sceneLevels[16];
    
    // Edit state
    uint8_t editValue;          // Value being edited
};

// SD Browser Screen State
struct SdBrowserState {
    bool mountAttempted;
    bool mounted;
    char currentPath[STORAGE_MAX_PATH_LEN];
    char error[STORAGE_ERROR_LEN];
    SdBrowserEntry entries[STORAGE_MAX_SD_ENTRIES];
    uint8_t entryCount;
};

// WAGO Viewer Screen State
struct WagoViewerState {
    bool mountAttempted;
    bool mounted;
    bool projectListLoaded;
    bool inProjectView;
    char title[STORAGE_MAX_NAME_LEN];
    char currentProjectPath[STORAGE_MAX_PATH_LEN];
    char error[STORAGE_ERROR_LEN];
    WagoProjectInfo projects[STORAGE_MAX_WAGO_PROJECTS];
    uint8_t projectCount;
    WagoViewItem items[STORAGE_MAX_WAGO_ITEMS];
    uint8_t itemCount;
    uint8_t moduleCount;
    bool currentProjectPacked;
};

struct LoadedWagoState {
    bool loaded;
    bool dirty;
    char projectPath[STORAGE_MAX_PATH_LEN];
    uint8_t moduleIndex;
    WagoModuleConfig module;
};

// Pong Easter Egg State
struct PongState {
    float ballX, ballY;
    float ballVX, ballVY;
    float paddleY;
    uint16_t score;
    bool gameOver;
    bool started;
    uint32_t lastUpdate;
};

// =============================================================================
// External State Variables
// =============================================================================
extern ScanState scanState;
extern ScanDeviceInfoState scanDeviceInfoState;
extern BroadcastCtrlState broadcastCtrlState;
extern AddressState addressState;
extern CloneState cloneState;
extern ResetState resetState;
extern PsuState psuState;
extern SendCmdState sendCmdState;
extern LocateState locateState;
extern BusMonState busMonState;
extern PopupState popupState;
extern DeviceParamsState deviceParamsState;
extern SdBrowserState sdBrowserState;
extern WagoViewerState wagoViewerState;
extern LoadedWagoState loadedWagoState;
extern PongState pongState;

// =============================================================================
// Screen Handler Functions
// =============================================================================

// Home Screen
void screenHomeDraw();
void screenHomeInput(int16_t delta, ButtonEvent btn);

// Scan Devices Screen
void screenScanDraw();
void screenScanInput(int16_t delta, ButtonEvent btn);

// Scan Device Info Screen (context menu)
void screenScanDeviceInfoDraw();
void screenScanDeviceInfoInput(int16_t delta, ButtonEvent btn);
void screenScanDeviceInfoUpdate();

// Address Device Screen
void screenAddressDraw();
void screenAddressInput(int16_t delta, ButtonEvent btn);

// Clone Config Screen
void screenCloneDraw();
void screenCloneInput(int16_t delta, ButtonEvent btn);

// Reset Device Screen
void screenResetDraw();
void screenResetInput(int16_t delta, ButtonEvent btn);

// DALI PSU Screen
void screenPsuDraw();
void screenPsuInput(int16_t delta, ButtonEvent btn);

// Send CMD Screen
void screenSendCmdDraw();
void screenSendCmdInput(int16_t delta, ButtonEvent btn);

// Broadcast Control Screen
void screenBroadcastCtrlDraw();
void screenBroadcastCtrlInput(int16_t delta, ButtonEvent btn);

// Locate Screen
void screenLocateDraw();
void screenLocateInput(int16_t delta, ButtonEvent btn);
void screenLocateUpdate();

// Bus Monitor Screen
void screenBusMonDraw();
void screenBusMonInput(int16_t delta, ButtonEvent btn);
void screenBusMonUpdate();

// SD Browser Screen
void screenSdBrowserDraw();
void screenSdBrowserInput(int16_t delta, ButtonEvent btn);

// WAGO Viewer Screen
void screenWagoViewerDraw();
void screenWagoViewerInput(int16_t delta, ButtonEvent btn);

// Device Parameters Screen
void screenDeviceParamsDraw();
void screenDeviceParamsInput(int16_t delta, ButtonEvent btn);

// Popup Screen
void screenPopupDraw();
void screenPopupInput(int16_t delta, ButtonEvent btn);

// Pong Easter Egg
void screenPongDraw();
void screenPongInput(int16_t delta, ButtonEvent btn);
void screenPongUpdate();

// =============================================================================
// Screen Handler Array
// =============================================================================
extern const ScreenHandler screenHandlers[];

// =============================================================================
// Utility Functions
// =============================================================================
const char* getTargetTypeString(uint8_t type);
const char* getCmdString(uint8_t cmd);
const char* getAddressModeString(uint8_t mode);
void formatBusFrame(const BusMonitorEntry* entry, char* buf, size_t bufSize);

#endif // SCREENS_H
