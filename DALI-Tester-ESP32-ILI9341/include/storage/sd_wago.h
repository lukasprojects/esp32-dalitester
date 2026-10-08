/**
 * @file sd_wago.h
 * @brief SD card browsing and read-only WAGO project helpers
 */

#ifndef SD_WAGO_H
#define SD_WAGO_H

#include <Arduino.h>
#include <FS.h>
#include "dali/dali_interface.h"

#define STORAGE_MAX_NAME_LEN      40
#define STORAGE_MAX_PATH_LEN      128
#define STORAGE_MAX_SD_ENTRIES    32
#define STORAGE_MAX_WAGO_PROJECTS 8
#define STORAGE_MAX_WAGO_ITEMS    48
#define STORAGE_MAX_WAGO_LINE_LEN 52
#define STORAGE_MAX_WAGO_GEARS    64
#define STORAGE_MAX_WAGO_CTRLDEVS 16
#define STORAGE_ERROR_LEN         64

struct SdBrowserEntry {
    char name[STORAGE_MAX_NAME_LEN];
    char path[STORAGE_MAX_PATH_LEN];
    bool isDir;
    uint32_t size;
};

struct WagoProjectInfo {
    char name[STORAGE_MAX_NAME_LEN];
    char path[STORAGE_MAX_PATH_LEN];
    bool packedFile;
};

struct WagoViewItem {
    char line1[STORAGE_MAX_WAGO_LINE_LEN];
    char line2[STORAGE_MAX_WAGO_LINE_LEN];
};

struct WagoGearConfig {
    char name[STORAGE_MAX_NAME_LEN];
    uint8_t shortAddress;
    DaliDeviceConfig config;
    uint16_t groups;
    uint8_t sceneLevels[16];
    bool valid;
    bool presentInFile;
};

struct WagoControlDeviceConfig {
    char name[STORAGE_MAX_NAME_LEN];
    uint8_t shortAddress;
    uint8_t sensorCount;
    bool isMasterModule;
    bool valid;
    bool presentInFile;
};

struct WagoModuleConfig {
    char projectName[STORAGE_MAX_NAME_LEN];
    char moduleName[STORAGE_MAX_NAME_LEN];
    uint8_t gearCount;
    uint8_t ctrlDeviceCount;
    WagoGearConfig gears[STORAGE_MAX_WAGO_GEARS];
    WagoControlDeviceConfig ctrlDevices[STORAGE_MAX_WAGO_CTRLDEVS];
    bool valid;
};

bool storageSdEnsureMounted();
const char* storageSdGetLastError();

bool storageListDirectory(const char* path, SdBrowserEntry* entries, uint8_t maxEntries, uint8_t* outCount);
bool storageDirectoryUp(char* path, size_t pathSize);
bool storageJoinPath(const char* basePath, const char* name, char* outPath, size_t outPathSize);

bool storageFindWagoProjects(WagoProjectInfo* projects, uint8_t maxProjects, uint8_t* outCount);
bool storageLoadWagoProjectView(const char* projectPath,
                                bool packedFile,
                                WagoViewItem* items,
                                uint8_t maxItems,
                                uint8_t* outCount,
                                char* outTitle,
                                size_t outTitleSize);
bool storageLoadWagoModuleConfig(const char* projectPath, uint8_t moduleIndex, WagoModuleConfig* outModule);
bool storageSaveWagoModuleConfig(const char* projectPath, uint8_t moduleIndex, const WagoModuleConfig* moduleConfig);

#endif // SD_WAGO_H