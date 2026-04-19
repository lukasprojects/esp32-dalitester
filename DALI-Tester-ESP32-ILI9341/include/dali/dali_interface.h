/**
 * @file dali_interface.h
 * @brief DALI Master interface using qqqDALI library
 */

#ifndef DALI_INTERFACE_H
#define DALI_INTERFACE_H

#include <Arduino.h>
#include "config.h"

// =============================================================================
// DALI Status Enum
// =============================================================================
enum DaliStatus {
    DALI_STATUS_IDLE,       // No activity
    DALI_STATUS_ACT,        // Activity detected (RX)
    DALI_STATUS_SEND,       // Sending command
    DALI_STATUS_OK,         // Command successful
    DALI_STATUS_NO_REPLY,   // No reply received
    DALI_STATUS_BUS_ERROR,  // Bus error
    DALI_STATUS_FAILED      // Command failed
};

// =============================================================================
// DALI Command Codes
// =============================================================================
// Arc power commands
#define DALI_CMD_OFF            0x00
#define DALI_CMD_UP             0x01
#define DALI_CMD_DOWN           0x02
#define DALI_CMD_STEP_UP        0x03
#define DALI_CMD_STEP_DOWN      0x04
#define DALI_CMD_RECALL_MAX     0x05
#define DALI_CMD_RECALL_MIN     0x06
#define DALI_CMD_STEP_DOWN_OFF  0x07
#define DALI_CMD_ON_STEP_UP     0x08

// Scene commands 0x10-0x1F (GOTO SCENE 0-15)
#define DALI_CMD_GOTO_SCENE     0x10

// Configuration commands
#define DALI_CMD_RESET              0x20
#define DALI_CMD_STORE_LEVEL_DTR    0x21
#define DALI_CMD_SET_SHORT_ADDR     0x80  // Set short address from DTR (DTR=0xFF removes short address)

// DTR commands
#define DALI_CMD_SET_MAX_LEVEL      0x2A
#define DALI_CMD_SET_MIN_LEVEL      0x2B
#define DALI_CMD_SET_SYS_FAIL_LEVEL 0x2C
#define DALI_CMD_SET_POWER_ON_LEVEL 0x2D
#define DALI_CMD_SET_FADE_TIME      0x2E
#define DALI_CMD_SET_FADE_RATE      0x2F

// Query commands
#define DALI_CMD_QUERY_STATUS       0x90
#define DALI_CMD_QUERY_CONTROL_GEAR 0x91
#define DALI_CMD_QUERY_LAMP_FAIL    0x92
#define DALI_CMD_QUERY_LAMP_POWER   0x93
#define DALI_CMD_QUERY_LIMIT_ERROR  0x94
#define DALI_CMD_QUERY_RESET_STATE  0x95
#define DALI_CMD_QUERY_MISSING_SA   0x96
#define DALI_CMD_QUERY_VERSION      0x97
#define DALI_CMD_QUERY_DTR          0x98
#define DALI_CMD_QUERY_DEVICE_TYPE  0x99
#define DALI_CMD_QUERY_PHYS_MIN     0x9A
#define DALI_CMD_QUERY_POWER_FAIL   0x9B
#define DALI_CMD_QUERY_ACTUAL_LEVEL 0xA0
#define DALI_CMD_QUERY_MAX_LEVEL    0xA1
#define DALI_CMD_QUERY_MIN_LEVEL    0xA2
#define DALI_CMD_QUERY_POWER_ON     0xA3
#define DALI_CMD_QUERY_SYS_FAIL     0xA4
#define DALI_CMD_QUERY_FADE_TIME    0xA5  // Upper nibble of reply byte
#define DALI_CMD_QUERY_FADE_RATE    0xA5  // Lower nibble of same reply byte
#define DALI_CMD_QUERY_SCENE_LEVEL  0xB0  // 0xB0-0xBF for scene 0-15
#define DALI_CMD_QUERY_GROUPS_L     0xC0
#define DALI_CMD_QUERY_GROUPS_H     0xC1
#define DALI_CMD_QUERY_RANDOM_ADDR_H 0xC2
#define DALI_CMD_QUERY_RANDOM_ADDR_M 0xC3
#define DALI_CMD_QUERY_RANDOM_ADDR_L 0xC4

// Group commands (configuration)
#define DALI_CMD_ADD_TO_GROUP       0x60  // 0x60-0x6F for group 0-15
#define DALI_CMD_REMOVE_FROM_GROUP  0x70  // 0x70-0x7F for group 0-15

// Scene commands (configuration)
#define DALI_CMD_SET_SCENE          0x40  // 0x40-0x4F for scene 0-15 (store DTR as scene level)

// Special commands (broadcast)
#define DALI_CMD_TERMINATE      0xA1
#define DALI_CMD_DTR0           0xA3
#define DALI_CMD_INITIALISE     0xA5
#define DALI_CMD_RANDOMISE      0xA7
#define DALI_CMD_COMPARE        0xA9
#define DALI_CMD_WITHDRAW       0xAB
#define DALI_CMD_SEARCHADDR_H   0xB1
#define DALI_CMD_SEARCHADDR_M   0xB3
#define DALI_CMD_SEARCHADDR_L   0xB5
#define DALI_CMD_PROGRAM_SA     0xB7
#define DALI_CMD_VERIFY_SA      0xB9
#define DALI_CMD_QUERY_SA       0xBB
#define DALI_CMD_PHYS_SEL       0xBD
#define DALI_CMD_ENABLE_DT      0xC1
#define DALI_CMD_DTR1           0xC3
#define DALI_CMD_DTR2           0xC5
#define DALI_CMD_WRITE_MEM_LOC  0xC7

// =============================================================================
// Device Configuration Structure
// =============================================================================
struct DaliDeviceConfig {
    uint8_t maxLevel;
    uint8_t minLevel;
    uint8_t powerOnLevel;
    uint8_t sysFailLevel;
    uint8_t fadeTime;
    uint8_t fadeRate;
    bool valid;
};

// =============================================================================
// Bus Monitor Entry
// =============================================================================
enum BusFrameType {
    FRAME_FORWARD,      // 16-bit forward frame (command)
    FRAME_BACKWARD,     // 8-bit backward frame (reply)
    FRAME_EVENT         // 24-bit DALI-2 event frame
};

struct BusMonitorEntry {
    uint32_t timestamp;
    BusFrameType type;
    uint8_t addrByte;       // For forward frames: address byte
    uint8_t dataByte;       // For forward frames: data byte / reply value
    uint8_t eventByte;      // For 24-bit event frames: third byte
    uint8_t frameLength;    // Number of bits received (8, 16, 24, etc.)
    bool valid;
};

// =============================================================================
// Function Declarations
// =============================================================================

// Initialization
void daliInit();
void daliTimerInit();

// PSU Control
void daliPsuSet(bool enable);
bool daliPsuGet();

// Low-level interface (called by qqqDALI)
uint8_t bus_is_high();
void bus_set_low();
void bus_set_high();

// DALI Commands with UI status wrapper
int16_t daliCmdUi(uint8_t addr, uint8_t cmd, bool expectReply = false);
int16_t daliDirectArc(uint8_t addr, uint8_t level);

// Query commands
int16_t daliQueryStatus(uint8_t addr);
int16_t daliQueryActualLevel(uint8_t addr);
int16_t daliQueryDeviceType(uint8_t addr);
int16_t daliQueryMaxLevel(uint8_t addr);
int16_t daliQueryMinLevel(uint8_t addr);
int16_t daliQueryPowerOnLevel(uint8_t addr);
int16_t daliQuerySysFailLevel(uint8_t addr);
int16_t daliQueryFadeTime(uint8_t addr);
int16_t daliQueryFadeRate(uint8_t addr);
int32_t daliQueryRandomAddress(uint8_t addr);  // Returns 24-bit serial number

// Group queries (returns bitmask of group membership)
uint16_t daliQueryGroups(uint8_t addr);  // Returns 16-bit bitmask (groups 0-15)

// Scene queries
int16_t daliQuerySceneLevel(uint8_t addr, uint8_t scene);  // Returns scene level (0-254, 255=MASK)

// Device type name lookup
const char* daliGetDeviceTypeName(uint8_t deviceType);

// Configuration commands
bool daliSetDTR0(uint8_t value);
bool daliSetMaxLevel(uint8_t addr, uint8_t value);
bool daliSetMinLevel(uint8_t addr, uint8_t value);
bool daliSetPowerOnLevel(uint8_t addr, uint8_t value);
bool daliSetSysFailLevel(uint8_t addr, uint8_t value);
bool daliSetFadeTime(uint8_t addr, uint8_t value);
bool daliSetFadeRate(uint8_t addr, uint8_t value);

// Group commands
bool daliAddToGroup(uint8_t addr, uint8_t group);      // Add device to group (0-15)
bool daliRemoveFromGroup(uint8_t addr, uint8_t group); // Remove device from group (0-15)

// Scene commands
bool daliSetSceneLevel(uint8_t addr, uint8_t scene, uint8_t level);  // Set scene level (0-15, level 0-254, 255=MASK)

// Addressing
bool daliReset(uint8_t addr, bool deleteAddress = false);
bool daliCommissionAll();
bool daliCommissionUnaddressed();
bool daliChangeAddress(uint8_t oldAddr, uint8_t newAddr);

// Full config read/write
bool daliReadConfig(uint8_t addr, DaliDeviceConfig* config);
bool daliWriteConfig(uint8_t addr, const DaliDeviceConfig* config);

// Scanning
uint8_t daliScanDevices(uint8_t* foundAddrs, uint8_t maxDevices);
uint8_t daliScanControlDevices(uint8_t* foundAddrs, uint8_t* gearAddrs, uint8_t gearCount, uint8_t maxDevices);
uint8_t daliCountUnaddressed();  // Count unaddressed devices on bus (without assigning addresses)

// DALI-2 Control Device queries (S=0 addressing)
int16_t daliQueryDeviceStatus(uint8_t addr);
int16_t daliQueryDeviceDeviceType(uint8_t addr);

// Status
DaliStatus daliGetStatus();
void daliSetStatus(DaliStatus status);
void daliStatusUpdate();  // Call in main loop for auto-timeout
const char* daliStatusToString(DaliStatus status);

// Bus Monitor
void daliMonitorUpdate();
bool daliMonitorGetEntry(uint8_t index, BusMonitorEntry* entry);
uint8_t daliMonitorGetCount();
uint16_t daliMonitorGetGeneration();
void daliMonitorClear();

// Timer ISR (called internally)
void IRAM_ATTR daliTimerISR();

#endif // DALI_INTERFACE_H
