/**
 * @file dali_interface.cpp
 * @brief DALI Master interface implementation using qqqDALI library
 */

#include "dali/dali_interface.h"
#include <qqqDALI.h>

// =============================================================================
// qqqDALI Instance
// =============================================================================
Dali dali;

// =============================================================================
// State Variables
// =============================================================================
static volatile DaliStatus g_daliStatus = DALI_STATUS_IDLE;
static volatile bool g_daliSending = false;
static bool g_psuEnabled = false;
static volatile uint32_t g_lastActivityTime = 0;

// Auto-reset to IDLE after this timeout (ms)
#define DALI_STATUS_TIMEOUT_MS 200

// Hardware timer
static hw_timer_t* daliTimer = nullptr;
static portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

// =============================================================================
// Bus Monitor Ring Buffer
// =============================================================================
static BusMonitorEntry g_busMonBuffer[BUSMON_MAX_ENTRIES];
static volatile uint8_t g_busMonHead = 0;
static volatile uint8_t g_busMonCount = 0;
static volatile uint16_t g_busMonGeneration = 0;  // Increments on every new frame

// =============================================================================
// Bus Interface Functions (called by qqqDALI)
// =============================================================================
uint8_t bus_is_high() {
    uint8_t level = digitalRead(DALI_RX);
    #if DALI_RX_INVERT
        level = !level;
    #endif
    return level;
}

void bus_set_low() {
    #if DALI_TX_INVERT
        digitalWrite(DALI_TX, HIGH);
    #else
        digitalWrite(DALI_TX, LOW);
    #endif
}

void bus_set_high() {
    #if DALI_TX_INVERT
        digitalWrite(DALI_TX, LOW);
    #else
        digitalWrite(DALI_TX, HIGH);
    #endif
}

// =============================================================================
// Timer ISR
// =============================================================================
void IRAM_ATTR daliTimerISR() {
    dali.timer();
}

// =============================================================================
// Initialization
// =============================================================================
void daliTimerInit() {
    // ESP32 hardware timer for DALI - 9600 Hz (104.17µs period)
    // Arduino ESP32 Core 3.x new API: timerBegin(frequency_Hz)
    #if ESP_ARDUINO_VERSION_MAJOR >= 3
        // New API (ESP32 Arduino Core 3.x / ESP-IDF 5.x)
        daliTimer = timerBegin(9600);  // 9600 Hz = 1200 baud * 8 oversampling
        timerAttachInterrupt(daliTimer, &daliTimerISR);
        timerAlarm(daliTimer, 1, true, 0);  // alarm at count 1, auto-reload
    #else
        // Old API (ESP32 Arduino Core 2.x / ESP-IDF 4.x)
        daliTimer = timerBegin(0, 80, true);  // Timer 0, prescaler 80 (1µs per tick)
        timerAttachInterrupt(daliTimer, &daliTimerISR, true);
        timerAlarmWrite(daliTimer, DALI_TIMER_US, true);  // 104µs period
        timerAlarmEnable(daliTimer);
    #endif
}

void daliInit() {
    Serial.println(F("DALI: Initializing..."));
    
    // Configure TX pin as push-pull output (Waveshare Pico-DALI2 has internal driver)
    pinMode(DALI_TX, OUTPUT);
    
    // Test TX pin polarity
    Serial.println(F("DALI: Testing TX pin..."));
    bus_set_high();
    Serial.printf("DALI: bus_set_high() -> GPIO%d = %d (should be idle/HIGH)\n", DALI_TX, digitalRead(DALI_TX));
    delay(5);
    bus_set_low();
    Serial.printf("DALI: bus_set_low()  -> GPIO%d = %d (should be active/LOW)\n", DALI_TX, digitalRead(DALI_TX));
    delay(5);
    bus_set_high();  // Return to idle
    Serial.printf("DALI: bus_set_high() -> GPIO%d = %d (back to idle)\n", DALI_TX, digitalRead(DALI_TX));
    
    // Configure RX pin as input
    pinMode(DALI_RX, INPUT);
    Serial.printf("DALI: RX pin %d state=%d (1=bus idle/high)\n", DALI_RX, digitalRead(DALI_RX));
    
    // Configure PSU control pin
    pinMode(DALI_PSU_EN, OUTPUT);
    daliPsuSet(false);  // Start with PSU off
    
    // Initialize qqqDALI
    // Signature: begin(bus_is_high, bus_set_low, bus_set_high)
    dali.begin(bus_is_high, bus_set_low, bus_set_high);
    Serial.println(F("DALI: qqqDALI initialized"));
    
    // Setup hardware timer for DALI bit timing
    daliTimerInit();
    Serial.println(F("DALI: Timer initialized (9600 Hz)"));
    
    // Test if timer is running by checking milli() after short delay
    delay(50);  // Should increment milli significantly
    Serial.printf("DALI: Timer test - milli()=%d (should be ~48)\n", dali.milli());
    
    // Clear bus monitor
    daliMonitorClear();
    
    g_daliStatus = DALI_STATUS_IDLE;
    Serial.println(F("DALI: Ready!"));
}

// =============================================================================
// PSU Control
// =============================================================================
void daliPsuSet(bool enable) {
    g_psuEnabled = enable;
    #if PSU_EN_ACTIVE_HIGH
        digitalWrite(DALI_PSU_EN, enable ? HIGH : LOW);
    #else
        digitalWrite(DALI_PSU_EN, enable ? LOW : HIGH);
    #endif
}

bool daliPsuGet() {
    return g_psuEnabled;
}

// =============================================================================
// Status Management
// =============================================================================
DaliStatus daliGetStatus() {
    return g_daliStatus;
}

void daliSetStatus(DaliStatus status) {
    g_daliStatus = status;
}

const char* daliStatusToString(DaliStatus status) {
    switch(status) {
        case DALI_STATUS_IDLE:      return "IDLE";
        case DALI_STATUS_ACT:       return "RX";
        case DALI_STATUS_SEND:      return "TX";
        case DALI_STATUS_OK:        return "OK";
        case DALI_STATUS_NO_REPLY:  return "NACK";
        case DALI_STATUS_BUS_ERROR: return "ERR";
        case DALI_STATUS_FAILED:    return "FAIL";
        default:                    return "?";
    }
}

void daliStatusUpdate() {
    // Auto-reset ACT/OK status to IDLE after timeout
    if (g_daliStatus == DALI_STATUS_ACT || g_daliStatus == DALI_STATUS_OK) {
        if (millis() - g_lastActivityTime > DALI_STATUS_TIMEOUT_MS) {
            g_daliStatus = DALI_STATUS_IDLE;
        }
    }
}

// =============================================================================
// DALI Command Wrapper with UI Status
// =============================================================================
int16_t daliCmdUi(uint8_t addr, uint8_t cmd, bool expectReply) {
    g_daliSending = true;
    g_daliStatus = DALI_STATUS_SEND;
    
    // Debug output
    Serial.printf("DALI TX: addr=0x%02X cmd=0x%02X reply=%d\n", addr, cmd, expectReply);
    
    int16_t result;
    
    // No critical section - tx_wait_rx handles timing internally
    // Critical sections cause watchdog timeouts during transmission
    if (expectReply) {
        result = dali.tx_wait_rx(addr, cmd);
    } else {
        dali.tx_wait_rx(addr, cmd);
        result = 0;
    }
    
    Serial.printf("DALI TX result: %d\n", result);
    
    // Wait for transmission to complete
    delay(20);
    
    if (expectReply) {
        if (result >= 0) {
            g_daliStatus = DALI_STATUS_OK;
        } else if (result == -1) {
            g_daliStatus = DALI_STATUS_NO_REPLY;
        } else if (result == -2) {
            g_daliStatus = DALI_STATUS_BUS_ERROR;
        } else {
            g_daliStatus = DALI_STATUS_FAILED;
        }
    } else {
        g_daliStatus = DALI_STATUS_OK;
    }
    
    g_lastActivityTime = millis();
    g_daliSending = false;
    return result;
}

int16_t daliDirectArc(uint8_t addr, uint8_t level) {
    g_daliSending = true;
    g_daliStatus = DALI_STATUS_SEND;
    
    // Direct arc power control (address byte even, S bit = 0)
    uint8_t addrByte = (addr << 1) & 0xFE;  // Short address with S=0
    
    dali.tx_wait_rx(addrByte, level);
    
    delay(10);
    g_daliStatus = DALI_STATUS_OK;
    g_lastActivityTime = millis();
    g_daliSending = false;
    
    return 0;
}

// =============================================================================
// Query Commands
// =============================================================================
static int16_t daliQuery(uint8_t addr, uint8_t cmd) {
    // Query commands use address byte with S=1 (command mode, control gear)
    uint8_t addrByte = ((addr << 1) | 0x01) & 0x7F;  // Short address + S=1
    return daliCmdUi(addrByte, cmd, true);
}

static int16_t daliQueryDevice(uint8_t addr, uint8_t cmd) {
    // DALI-2 Control Device queries use address byte with S=0
    uint8_t addrByte = (addr << 1) & 0x7E;  // Short address + S=0
    return daliCmdUi(addrByte, cmd, true);
}

int16_t daliQueryStatus(uint8_t addr) {
    return daliQuery(addr, DALI_CMD_QUERY_STATUS);
}

int16_t daliQueryActualLevel(uint8_t addr) {
    return daliQuery(addr, DALI_CMD_QUERY_ACTUAL_LEVEL);
}

int16_t daliQueryDeviceType(uint8_t addr) {
    return daliQuery(addr, DALI_CMD_QUERY_DEVICE_TYPE);
}

int16_t daliQueryMaxLevel(uint8_t addr) {
    return daliQuery(addr, DALI_CMD_QUERY_MAX_LEVEL);
}

int16_t daliQueryMinLevel(uint8_t addr) {
    return daliQuery(addr, DALI_CMD_QUERY_MIN_LEVEL);
}

int16_t daliQueryPowerOnLevel(uint8_t addr) {
    return daliQuery(addr, DALI_CMD_QUERY_POWER_ON);
}

int16_t daliQuerySysFailLevel(uint8_t addr) {
    return daliQuery(addr, DALI_CMD_QUERY_SYS_FAIL);
}

static int16_t daliQueryFadeTimeRateRaw(uint8_t addr) {
    return daliQuery(addr, DALI_CMD_QUERY_FADE_TIME);
}

int16_t daliQueryFadeTime(uint8_t addr) {
    int16_t raw = daliQueryFadeTimeRateRaw(addr);
    if (raw < 0) return raw;
    return (raw >> 4) & 0x0F;
}

int16_t daliQueryFadeRate(uint8_t addr) {
    int16_t raw = daliQueryFadeTimeRateRaw(addr);
    if (raw < 0) return raw;
    return raw & 0x0F;
}

int32_t daliQueryRandomAddress(uint8_t addr) {
    // Query the 24-bit random address (used as serial number)
    int16_t h = daliQuery(addr, DALI_CMD_QUERY_RANDOM_ADDR_H);
    if (h < 0) return -1;
    
    int16_t m = daliQuery(addr, DALI_CMD_QUERY_RANDOM_ADDR_M);
    if (m < 0) return -1;
    
    int16_t l = daliQuery(addr, DALI_CMD_QUERY_RANDOM_ADDR_L);
    if (l < 0) return -1;
    
    return ((int32_t)h << 16) | ((int32_t)m << 8) | l;
}

uint16_t daliQueryGroups(uint8_t addr) {
    // Query group membership (returns 16-bit bitmask)
    int16_t groupsL = daliQuery(addr, DALI_CMD_QUERY_GROUPS_L);  // Groups 0-7
    int16_t groupsH = daliQuery(addr, DALI_CMD_QUERY_GROUPS_H);  // Groups 8-15
    
    if (groupsL < 0) groupsL = 0;
    if (groupsH < 0) groupsH = 0;
    
    return ((uint16_t)groupsH << 8) | (uint16_t)groupsL;
}

int16_t daliQuerySceneLevel(uint8_t addr, uint8_t scene) {
    if (scene > 15) return -1;
    return daliQuery(addr, DALI_CMD_QUERY_SCENE_LEVEL + scene);
}

const char* daliGetDeviceTypeName(uint8_t deviceType) {
    switch (deviceType) {
        case 0:   return "Fluorescent";
        case 1:   return "Emergency";
        case 2:   return "HID";
        case 3:   return "LV Halogen";
        case 4:   return "Incandescent";
        case 5:   return "DC Converter";
        case 6:   return "LED Module";
        case 7:   return "Relay";
        case 8:   return "Colour (DT8)";
        // DALI-2 Control Device types (IEC 62386-103)
        case 128: return "CD Unknown";
        case 129: return "CD Switch";
        case 130: return "CD Dimmer";
        case 131: return "CD OccSensor";
        case 132: return "CD LightCtrl";
        case 133: return "CD LightCtrl2";
        case 134: return "CD Scheduler";
        case 135: return "CD Gateway";
        case 136: return "CD Sequencer";
        case 137: return "CD PowerSupply";
        case 138: return "CD EmergCtrl";
        case 139: return "CD AnalogIn";
        case 140: return "CD DataLogger";
        case 252: return "Bus Power";
        case 253: return "Controller";
        case 254: return "Multi-Type";
        case 255: return "Unknown";
        default:  return "Reserved";
    }
}

// =============================================================================
// Configuration Commands
// =============================================================================
bool daliSetDTR0(uint8_t value) {
    // Special command: DTR0 (0xA3)
    g_daliSending = true;
    g_daliStatus = DALI_STATUS_SEND;
    
    dali.tx_wait_rx(0xA3, value);  // Set DTR0
    
    delay(10);
    g_daliStatus = DALI_STATUS_OK;
    g_lastActivityTime = millis();
    g_daliSending = false;
    return true;
}

static bool daliConfigCmd(uint8_t addr, uint8_t cmd, uint8_t value) {
    // First set DTR0
    if (!daliSetDTR0(value)) return false;
    
    // Then send config command (needs to be sent twice)
    uint8_t addrByte = ((addr << 1) | 0x01);  // Command mode
    
    g_daliSending = true;
    g_daliStatus = DALI_STATUS_SEND;
    
    dali.tx_wait_rx(addrByte, cmd);
    delay(10);
    
    dali.tx_wait_rx(addrByte, cmd);  // Send twice for config commands
    delay(10);
    
    g_daliStatus = DALI_STATUS_OK;
    g_lastActivityTime = millis();
    g_daliSending = false;
    return true;
}

static bool daliSetFadeNibbleWithVerify(uint8_t addr, uint8_t cmd, uint8_t value, bool highNibble) {
    value &= 0x0F;

    for (uint8_t attempt = 0; attempt < 3; attempt++) {
        if (!daliConfigCmd(addr, cmd, value)) continue;

        int16_t raw = daliQueryFadeTimeRateRaw(addr);
        if (raw < 0) continue;

        uint8_t readback = highNibble ? ((raw >> 4) & 0x0F) : (raw & 0x0F);
        if (readback == value) {
            return true;
        }
    }

    return false;
}

bool daliSetMaxLevel(uint8_t addr, uint8_t value) {
    return (dali.set_max_level(value, addr) == 0);
}

bool daliSetMinLevel(uint8_t addr, uint8_t value) {
    return (dali.set_min_level(value, addr) == 0);
}

bool daliSetPowerOnLevel(uint8_t addr, uint8_t value) {
    return (dali.set_power_on_level(value, addr) == 0);
}

bool daliSetSysFailLevel(uint8_t addr, uint8_t value) {
    return (dali.set_system_failure_level(value, addr) == 0);
}

bool daliSetFadeTime(uint8_t addr, uint8_t value) {
    return daliSetFadeNibbleWithVerify(addr, DALI_CMD_SET_FADE_TIME, value, true);
}

bool daliSetFadeRate(uint8_t addr, uint8_t value) {
    return daliSetFadeNibbleWithVerify(addr, DALI_CMD_SET_FADE_RATE, value, false);
}

// =============================================================================
// Group Commands
// =============================================================================
bool daliAddToGroup(uint8_t addr, uint8_t group) {
    if (group > 15) return false;
    
    uint8_t addrByte = ((addr << 1) | 0x01);  // Command mode
    uint8_t cmd = DALI_CMD_ADD_TO_GROUP + group;
    
    g_daliSending = true;
    g_daliStatus = DALI_STATUS_SEND;
    
    // Config commands must be sent twice
    dali.tx_wait_rx(addrByte, cmd);
    delay(10);
    
    dali.tx_wait_rx(addrByte, cmd);
    delay(10);
    
    g_daliStatus = DALI_STATUS_OK;
    g_lastActivityTime = millis();
    g_daliSending = false;
    return true;
}

bool daliRemoveFromGroup(uint8_t addr, uint8_t group) {
    if (group > 15) return false;
    
    uint8_t addrByte = ((addr << 1) | 0x01);  // Command mode
    uint8_t cmd = DALI_CMD_REMOVE_FROM_GROUP + group;
    
    g_daliSending = true;
    g_daliStatus = DALI_STATUS_SEND;
    
    // Config commands must be sent twice
    dali.tx_wait_rx(addrByte, cmd);
    delay(10);
    
    dali.tx_wait_rx(addrByte, cmd);
    delay(10);
    
    g_daliStatus = DALI_STATUS_OK;
    g_lastActivityTime = millis();
    g_daliSending = false;
    return true;
}

// =============================================================================
// Scene Commands
// =============================================================================
bool daliSetSceneLevel(uint8_t addr, uint8_t scene, uint8_t level) {
    if (scene > 15) return false;
    
    // First set DTR0 to the desired level
    if (!daliSetDTR0(level)) return false;
    
    uint8_t addrByte = ((addr << 1) | 0x01);  // Command mode
    uint8_t cmd = DALI_CMD_SET_SCENE + scene;
    
    g_daliSending = true;
    g_daliStatus = DALI_STATUS_SEND;
    
    // Config commands must be sent twice
    dali.tx_wait_rx(addrByte, cmd);
    delay(10);
    
    dali.tx_wait_rx(addrByte, cmd);
    delay(10);
    
    g_daliStatus = DALI_STATUS_OK;
    g_lastActivityTime = millis();
    g_daliSending = false;
    return true;
}

// =============================================================================
// Reset Command
// =============================================================================
bool daliReset(uint8_t addr, bool deleteAddress) {
    g_daliSending = true;
    g_daliStatus = DALI_STATUS_SEND;

    // qqqDALI cmd() handles required repeats for commands marked as REPEAT.
    int16_t resetCmd = dali.cmd(DALI_RESET, addr);
    bool resetSent = (resetCmd >= 0 || resetCmd == -DALI_RESULT_NO_REPLY);
    delay(20);

    int16_t resetState = daliQuery(addr, DALI_CMD_QUERY_RESET_STATE);
    bool inResetState = (resetState > 0);

    bool addressDeleted = true;
    if (deleteAddress) {
        // INITIALISE for this specific device - required for configuration commands
        // init_arg = (addr << 1) | 1 targets only this short address
        dali.cmd(DALI_INITIALISE, (addr << 1) | 1);
        delay(100);

        // Set DTR0 = 0xFF (removes short address)
        bool dtrOk = (dali.set_dtr0(0xFF, addr) == 0);
        // Send SET SHORT ADDRESS (REPEAT) - device stores DTR0 as new address (0xFF = delete)
        int16_t setSaCmd = dtrOk ? dali.cmd(DALI_SET_SHORT_ADDRESS, addr) : -DALI_RESULT_INVALID_CMD;
        bool setSaSent = (setSaCmd >= 0 || setSaCmd == -DALI_RESULT_NO_REPLY);
        delay(10);

        // After deletion, device no longer responds to old address
        int16_t checkStatus = daliQueryStatus(addr);
        addressDeleted = dtrOk && setSaSent && (checkStatus < 0);

        // Terminate INITIALISE state
        dali.cmd(DALI_TERMINATE, 0x00);
    }

    bool success = resetSent && inResetState && addressDeleted;

    g_daliStatus = success ? DALI_STATUS_OK : DALI_STATUS_FAILED;
    g_lastActivityTime = millis();
    g_daliSending = false;
    return success;
}

// =============================================================================
// Addressing / Commissioning
// =============================================================================
bool daliCommissionAll() {
    // INITIALISE with 0x00 = all devices
    g_daliSending = true;
    g_daliStatus = DALI_STATUS_SEND;
    
    // Send INITIALISE(0x00) twice
    dali.tx_wait_rx(DALI_CMD_INITIALISE, 0x00);
    delay(10);
    
    dali.tx_wait_rx(DALI_CMD_INITIALISE, 0x00);
    delay(100);
    
    // Use qqqDALI commission function
    int result = dali.commission(0x00);
    
    // TERMINATE
    dali.tx_wait_rx(DALI_CMD_TERMINATE, 0x00);
    delay(10);
    
    g_daliStatus = (result >= 0) ? DALI_STATUS_OK : DALI_STATUS_FAILED;
    g_lastActivityTime = millis();
    g_daliSending = false;
    return (result >= 0);
}

bool daliCommissionUnaddressed() {
    // INITIALISE with 0xFF = unaddressed devices only
    g_daliSending = true;
    g_daliStatus = DALI_STATUS_SEND;
    
    // Send INITIALISE(0xFF) twice
    dali.tx_wait_rx(DALI_CMD_INITIALISE, 0xFF);
    delay(10);
    
    dali.tx_wait_rx(DALI_CMD_INITIALISE, 0xFF);
    delay(100);
    
    // Use qqqDALI commission function
    int result = dali.commission(0xFF);
    
    // TERMINATE
    dali.tx_wait_rx(DALI_CMD_TERMINATE, 0x00);
    delay(10);
    
    g_daliStatus = (result >= 0) ? DALI_STATUS_OK : DALI_STATUS_FAILED;
    g_lastActivityTime = millis();
    g_daliSending = false;
    return (result >= 0);
}

bool daliChangeAddress(uint8_t oldAddr, uint8_t newAddr) {
    g_daliSending = true;
    g_daliStatus = DALI_STATUS_SEND;
    
    // INITIALISE with specific address (oldAddr << 1 | 1)
    uint8_t initParam = (oldAddr << 1) | 0x01;
    
    dali.tx_wait_rx(DALI_CMD_INITIALISE, initParam);
    delay(10);
    
    dali.tx_wait_rx(DALI_CMD_INITIALISE, initParam);
    delay(100);
    
    // Set DTR0 to new address
    daliSetDTR0((newAddr << 1) | 0x01);
    delay(10);
    
    // PROGRAM SHORT ADDRESS
    dali.tx_wait_rx(DALI_CMD_PROGRAM_SA, ((newAddr << 1) | 0x01));
    delay(10);
    
    // Verify with QUERY SHORT ADDRESS
    int16_t result = dali.tx_wait_rx(0xFF, DALI_CMD_QUERY_SA);
    delay(10);
    
    // TERMINATE
    dali.tx_wait_rx(DALI_CMD_TERMINATE, 0x00);
    delay(10);
    
    bool success = (result == ((newAddr << 1) | 0x01));
    g_daliStatus = success ? DALI_STATUS_OK : DALI_STATUS_FAILED;
    g_lastActivityTime = millis();
    g_daliSending = false;
    return success;
}

// =============================================================================
// Full Configuration Read/Write
// =============================================================================
bool daliReadConfig(uint8_t addr, DaliDeviceConfig* config) {
    config->valid = false;
    
    int16_t maxLevel = daliQueryMaxLevel(addr);
    if (maxLevel < 0) return false;
    config->maxLevel = (uint8_t)maxLevel;
    
    int16_t minLevel = daliQueryMinLevel(addr);
    if (minLevel < 0) return false;
    config->minLevel = (uint8_t)minLevel;
    
    int16_t powerOn = daliQueryPowerOnLevel(addr);
    if (powerOn < 0) return false;
    config->powerOnLevel = (uint8_t)powerOn;
    
    int16_t sysFail = daliQuerySysFailLevel(addr);
    if (sysFail < 0) return false;
    config->sysFailLevel = (uint8_t)sysFail;
    
    int16_t fadeTime = daliQueryFadeTime(addr);
    if (fadeTime < 0) return false;
    config->fadeTime = (uint8_t)fadeTime;
    
    int16_t fadeRate = daliQueryFadeRate(addr);
    if (fadeRate < 0) return false;
    config->fadeRate = (uint8_t)fadeRate;
    
    config->valid = true;
    return true;
}

bool daliWriteConfig(uint8_t addr, const DaliDeviceConfig* config) {
    if (!config->valid) return false;

    if (!daliSetMaxLevel(addr, config->maxLevel)) return false;
    if (!daliSetMinLevel(addr, config->minLevel)) return false;
    if (!daliSetPowerOnLevel(addr, config->powerOnLevel)) return false;
    if (!daliSetSysFailLevel(addr, config->sysFailLevel)) return false;
    if (!daliSetFadeTime(addr, config->fadeTime)) return false;
    if (!daliSetFadeRate(addr, config->fadeRate)) return false;

    // Final readback to avoid reporting false positives on gear that ignores writes.
    DaliDeviceConfig verify;
    if (!daliReadConfig(addr, &verify) || !verify.valid) return false;

    return (verify.maxLevel == config->maxLevel) &&
           (verify.minLevel == config->minLevel) &&
           (verify.powerOnLevel == config->powerOnLevel) &&
           (verify.sysFailLevel == config->sysFailLevel) &&
           (verify.fadeTime == (config->fadeTime & 0x0F)) &&
           (verify.fadeRate == (config->fadeRate & 0x0F));
}

// =============================================================================
// Device Scanning
// =============================================================================
uint8_t daliScanDevices(uint8_t* foundAddrs, uint8_t maxDevices) {
    uint8_t count = 0;
    
    for (uint8_t addr = 0; addr < 64 && count < maxDevices; addr++) {
        int16_t status = daliQueryStatus(addr);
        
        if (status >= 0) {
            foundAddrs[count++] = addr;
        }
        
        // Small delay between queries
        delay(5);
    }
    
    return count;
}

uint8_t daliScanControlDevices(uint8_t* foundAddrs, uint8_t* gearAddrs, uint8_t gearCount, uint8_t maxDevices) {
    // Scan for DALI-2 Control Devices (sensors, buttons, etc.)
    // Uses S=0 addressing. Only probes addresses NOT already found as control gear
    // to avoid accidentally sending DAPC to gear devices.
    uint8_t count = 0;
    
    for (uint8_t addr = 0; addr < 64 && count < maxDevices; addr++) {
        // Skip addresses already occupied by control gear
        bool isGear = false;
        for (uint8_t g = 0; g < gearCount; g++) {
            if (gearAddrs[g] == addr) { isGear = true; break; }
        }
        if (isGear) continue;
        
        // Query device status with S=0 (control device addressing)
        int16_t status = daliQueryDeviceStatus(addr);
        
        if (status >= 0) {
            foundAddrs[count++] = addr;
        }
        
        delay(5);
    }
    
    return count;
}

int16_t daliQueryDeviceStatus(uint8_t addr) {
    return daliQueryDevice(addr, DALI_CMD_QUERY_STATUS);
}

int16_t daliQueryDeviceDeviceType(uint8_t addr) {
    return daliQueryDevice(addr, DALI_CMD_QUERY_DEVICE_TYPE);
}

uint8_t daliCountUnaddressed() {
    // Count unaddressed devices on the bus using binary search + withdraw
    // Does NOT assign short addresses - just discovers and counts
    
    g_daliSending = true;
    g_daliStatus = DALI_STATUS_SEND;
    
    // INITIALISE(0xFF) - only unaddressed devices will participate
    dali.cmd(DALI_INITIALISE, 0xFF);
    // RANDOMISE - generate random addresses for search
    dali.cmd(DALI_RANDOMISE, 0x00);
    delay(100);  // Wait for random address generation
    
    uint8_t count = 0;
    while (count < 64) {
        uint32_t addr = dali.find_addr();
        if (addr > 0xFFFFFF) break;  // No more devices found
        count++;
        // Withdraw this device from search (do not assign address)
        dali.cmd(DALI_WITHDRAW, 0x00);
    }
    
    // Terminate the INITIALISE state
    dali.cmd(DALI_TERMINATE, 0x00);
    
    g_daliSending = false;
    g_daliStatus = DALI_STATUS_IDLE;
    g_lastActivityTime = millis();
    
    return count;
}

// =============================================================================
// Bus Monitor
// =============================================================================
void daliMonitorUpdate() {
    // Check for received frames via qqqDALI rx()
    if (!g_daliSending) {
        uint8_t rxBuffer[4];
        uint8_t rxBits = dali.rx(rxBuffer);
        
        if (rxBits > 0) {
            // Frame received
            g_daliStatus = DALI_STATUS_ACT;
            g_lastActivityTime = millis();
            
            BusMonitorEntry entry;
            entry.timestamp = millis();
            entry.valid = true;
            entry.frameLength = rxBits;
            entry.eventByte = 0;
            
            // Determine frame type based on bit count
            if (rxBits == 8) {
                // 8-bit backward frame (reply)
                entry.type = FRAME_BACKWARD;
                entry.dataByte = rxBuffer[0];
                entry.addrByte = 0;
            } else if (rxBits == 16) {
                // 16-bit forward frame (command)
                entry.type = FRAME_FORWARD;
                entry.addrByte = rxBuffer[0];
                entry.dataByte = rxBuffer[1];
            } else if (rxBits == 24) {
                // 24-bit DALI-2 event frame
                entry.type = FRAME_EVENT;
                entry.addrByte = rxBuffer[0];
                entry.dataByte = rxBuffer[1];
                entry.eventByte = rxBuffer[2];
            } else {
                // Unknown frame type - store what we can
                entry.type = FRAME_BACKWARD;
                entry.dataByte = rxBuffer[0];
                entry.addrByte = 0;
            }
            
            // Add to ring buffer
            portENTER_CRITICAL(&timerMux);
            g_busMonBuffer[g_busMonHead] = entry;
            g_busMonHead = (g_busMonHead + 1) % BUSMON_MAX_ENTRIES;
            if (g_busMonCount < BUSMON_MAX_ENTRIES) {
                g_busMonCount++;
            }
            g_busMonGeneration++;
            portEXIT_CRITICAL(&timerMux);
        }
    }
}

bool daliMonitorGetEntry(uint8_t index, BusMonitorEntry* entry) {
    if (index >= g_busMonCount) return false;
    
    portENTER_CRITICAL(&timerMux);
    // Calculate actual index (newest first)
    uint8_t actualIdx = (g_busMonHead - 1 - index + BUSMON_MAX_ENTRIES) % BUSMON_MAX_ENTRIES;
    *entry = g_busMonBuffer[actualIdx];
    portEXIT_CRITICAL(&timerMux);
    
    return entry->valid;
}

uint8_t daliMonitorGetCount() {
    return g_busMonCount;
}

uint16_t daliMonitorGetGeneration() {
    return g_busMonGeneration;
}

void daliMonitorClear() {
    portENTER_CRITICAL(&timerMux);
    g_busMonHead = 0;
    g_busMonCount = 0;
    g_busMonGeneration = 0;
    for (uint8_t i = 0; i < BUSMON_MAX_ENTRIES; i++) {
        g_busMonBuffer[i].valid = false;
    }
    portEXIT_CRITICAL(&timerMux);
}
