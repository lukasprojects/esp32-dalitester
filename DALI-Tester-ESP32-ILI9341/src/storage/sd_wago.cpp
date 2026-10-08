/**
 * @file sd_wago.cpp
 * @brief SD card browsing and read-only WAGO project helpers
 */

#include "storage/sd_wago.h"

#include <SD.h>
#include <SPI.h>
#include <stdlib.h>
#include <stdarg.h>

#include "config.h"

static bool g_sdInitAttempted = false;
static bool g_sdMounted = false;
static char g_sdLastError[STORAGE_ERROR_LEN] = "SD not initialized";
static uint32_t g_sdLastAttemptMs = 0;

static void copyText(char* dst, size_t dstSize, const char* src) {
    if (dstSize == 0) return;
    if (!src) {
        dst[0] = '\0';
        return;
    }
    strncpy(dst, src, dstSize - 1);
    dst[dstSize - 1] = '\0';
}

static char toLowerAscii(char c) {
    if (c >= 'A' && c <= 'Z') return (char)(c - 'A' + 'a');
    return c;
}

static int compareIgnoreCase(const char* a, const char* b) {
    while (*a && *b) {
        char ca = toLowerAscii(*a++);
        char cb = toLowerAscii(*b++);
        if (ca != cb) return (int)((unsigned char)ca) - (int)((unsigned char)cb);
    }
    return (int)((unsigned char)toLowerAscii(*a)) - (int)((unsigned char)toLowerAscii(*b));
}

static const char* findIgnoreCase(const char* text, const char* needle) {
    if (!text || !needle || !needle[0]) return nullptr;

    size_t needleLen = strlen(needle);
    for (const char* p = text; *p; ++p) {
        size_t i = 0;
        while (i < needleLen && p[i] && toLowerAscii(p[i]) == toLowerAscii(needle[i])) {
            i++;
        }
        if (i == needleLen) return p;
    }
    return nullptr;
}

static bool endsWithIgnoreCase(const char* text, const char* suffix) {
    if (!text || !suffix) return false;
    size_t textLen = strlen(text);
    size_t suffixLen = strlen(suffix);
    if (suffixLen > textLen) return false;
    return compareIgnoreCase(text + textLen - suffixLen, suffix) == 0;
}

static const char* pathBaseName(const char* path) {
    if (!path || !path[0]) return "/";

    const char* lastSlash = strrchr(path, '/');
    if (!lastSlash) return path;
    if (!lastSlash[1]) return "/";
    return lastSlash + 1;
}

static bool readTextLine(File& file, char* buf, size_t bufSize) {
    if (bufSize == 0) return false;
    if (!file.available()) return false;

    size_t len = file.readBytesUntil('\n', buf, bufSize - 1);
    buf[len] = '\0';
    while (len > 0 && (buf[len - 1] == '\r' || buf[len - 1] == '\n')) {
        buf[--len] = '\0';
    }
    return true;
}

static bool extractQuotedOrPlainValue(const char* start, char* out, size_t outSize) {
    if (!start || !out || outSize == 0) return false;

    while (*start == ' ' || *start == '\t' || *start == '=') start++;

    char quote = 0;
    if (*start == '"' || *start == '\'') {
        quote = *start;
        start++;
    }

    size_t len = 0;
    while (*start && len + 1 < outSize) {
        if (quote) {
            if (*start == quote) break;
        } else if (*start == '<' || *start == '>' || *start == '/' || *start == ' ' || *start == '\t') {
            break;
        }
        out[len++] = *start++;
    }
    out[len] = '\0';
    return len > 0;
}

static bool extractXmlAttributeValue(const char* line, const char* attr, char* out, size_t outSize) {
    const char* pos = findIgnoreCase(line, attr);
    if (!pos) return false;

    const char* eq = strchr(pos, '=');
    if (!eq) return false;
    return extractQuotedOrPlainValue(eq + 1, out, outSize);
}

static bool extractXmlTagValue(const char* line, const char* tag, char* out, size_t outSize) {
    char openTag[32];
    char closeTag[32];
    snprintf(openTag, sizeof(openTag), "<%s>", tag);
    snprintf(closeTag, sizeof(closeTag), "</%s>", tag);

    const char* start = findIgnoreCase(line, openTag);
    const char* end = findIgnoreCase(line, closeTag);
    if (!start || !end || end <= start) return false;

    start += strlen(openTag);
    size_t len = (size_t)(end - start);
    if (len >= outSize) len = outSize - 1;
    memcpy(out, start, len);
    out[len] = '\0';
    return len > 0;
}

static bool parseFirstUint(const char* text, uint8_t* value) {
    if (!text || !value) return false;

    while (*text && (*text < '0' || *text > '9')) text++;
    if (!*text) return false;

    uint16_t result = 0;
    while (*text >= '0' && *text <= '9') {
        result = (uint16_t)(result * 10 + (*text - '0'));
        text++;
    }

    if (result > 255) return false;
    *value = (uint8_t)result;
    return true;
}

static bool parseUint16Dec(const char* text, uint16_t* value) {
    if (!text || !value) return false;

    while (*text == ' ' || *text == '\t') text++;
    if (!*text) return false;

    char* end = nullptr;
    unsigned long parsed = strtoul(text, &end, 10);
    if (end == text || parsed > 0xFFFFu) return false;
    *value = (uint16_t)parsed;
    return true;
}

struct WagoModuleSummary {
    char name[STORAGE_MAX_NAME_LEN];
    uint8_t gearCount;
    uint8_t sensorCount;
};

static void addViewLine(WagoViewItem* items, uint8_t maxItems, uint8_t* count, const char* fmt, ...);
static void addViewLine2(WagoViewItem* items,
                         uint8_t maxItems,
                         uint8_t* count,
                         const char* line1,
                         const char* line2);
static bool extractSolutionTitle(const char* projectPath, char* outTitle, size_t outTitleSize);
static bool buildWagoModulePath(const char* projectPath, uint8_t moduleIndex, char* outPath, size_t outPathSize);

static void trimAscii(char* text) {
    if (!text) return;

    size_t len = strlen(text);
    while (len > 0 && (text[len - 1] == ' ' || text[len - 1] == '\t' ||
                       text[len - 1] == '\r' || text[len - 1] == '\n')) {
        text[--len] = '\0';
    }

    size_t start = 0;
    while (text[start] == ' ' || text[start] == '\t' || text[start] == '\r' || text[start] == '\n') {
        start++;
    }
    if (start > 0) {
        memmove(text, text + start, strlen(text + start) + 1);
    }
}

static bool extractDisplayNameAddress(const char* text, char* outName, size_t outNameSize, uint8_t* outAddr) {
    if (!text || !outName || outNameSize == 0) return false;

    copyText(outName, outNameSize, text);
    trimAscii(outName);

    size_t len = strlen(outName);
    if (len < 3 || outName[len - 1] != ')') return false;

    size_t end = len - 1;
    size_t start = end;
    while (start > 0 && outName[start] != '(') {
        start--;
    }
    if (outName[start] != '(' || start + 1 >= end) return false;

    for (size_t i = start + 1; i < end; i++) {
        if (outName[i] < '0' || outName[i] > '9') return false;
    }

    if (!parseFirstUint(outName + start + 1, outAddr)) return false;

    outName[start] = '\0';
    trimAscii(outName);
    return true;
}

static bool containsSeenName(char seenNames[][STORAGE_MAX_NAME_LEN], uint8_t seenCount, const char* name) {
    for (uint8_t i = 0; i < seenCount; i++) {
        if (compareIgnoreCase(seenNames[i], name) == 0) return true;
    }
    return false;
}

static bool appendUniqueDeviceLine(WagoViewItem* items,
                                   uint8_t maxItems,
                                   uint8_t* itemCount,
                                   bool seenAddr[64],
                                   char seenNames[][STORAGE_MAX_NAME_LEN],
                                   uint8_t* seenNameCount,
                                   const char* displayName,
                                   const char* type,
                                   uint8_t* outDeviceCount) {
    if (!displayName || !displayName[0] || !outDeviceCount) return false;

    char cleanName[STORAGE_MAX_NAME_LEN];
    uint8_t addr = 0;
    bool hasAddr = extractDisplayNameAddress(displayName, cleanName, sizeof(cleanName), &addr);
    if (!hasAddr) {
        copyText(cleanName, sizeof(cleanName), displayName);
        trimAscii(cleanName);
    }

    if (!cleanName[0]) {
        copyText(cleanName, sizeof(cleanName), displayName);
        trimAscii(cleanName);
    }

    if (hasAddr && addr < 64) {
        if (seenAddr[addr]) return false;
        seenAddr[addr] = true;
        if (type && type[0]) {
            addViewLine(items, maxItems, itemCount, "SA%u %s (%s)", addr, cleanName, type);
        } else {
            addViewLine(items, maxItems, itemCount, "SA%u %s", addr, cleanName);
        }
        (*outDeviceCount)++;
        return true;
    }

    if (containsSeenName(seenNames, *seenNameCount, cleanName)) return false;
    if (*seenNameCount < 16) {
        copyText(seenNames[*seenNameCount], STORAGE_MAX_NAME_LEN, cleanName);
        (*seenNameCount)++;
    }

    if (type && type[0]) {
        addViewLine(items, maxItems, itemCount, "%s (%s)", cleanName, type);
    } else {
        addViewLine(items, maxItems, itemCount, "%s", cleanName);
    }
    (*outDeviceCount)++;
    return true;
}

static bool extractAddressValue(const char* line, uint8_t* addr) {
    static const char* const attrs[] = {
        "shortAddress", "short_address", "daliAddress", "dali_address", "shortAddr"
    };
    static const char* const tags[] = {
        "ShortAddress", "shortAddress", "DaliAddress", "daliAddress", "ShortAddr"
    };

    char value[24];
    for (uint8_t i = 0; i < sizeof(attrs) / sizeof(attrs[0]); i++) {
        if (extractXmlAttributeValue(line, attrs[i], value, sizeof(value)) && parseFirstUint(value, addr)) {
            return true;
        }
    }
    for (uint8_t i = 0; i < sizeof(tags) / sizeof(tags[0]); i++) {
        if (extractXmlTagValue(line, tags[i], value, sizeof(value)) && parseFirstUint(value, addr)) {
            return true;
        }
    }

    const char* shortPos = findIgnoreCase(line, "shortaddress");
    if (!shortPos) shortPos = findIgnoreCase(line, "daliaddress");
    if (!shortPos) return false;

    return parseFirstUint(shortPos, addr);
}

static bool extractNameValue(const char* line, char* out, size_t outSize) {
    static const char* const attrs[] = {
        "displayName", "deviceName", "name", "label"
    };
    static const char* const tags[] = {
        "DisplayName", "DeviceName", "Name", "Label"
    };

    for (uint8_t i = 0; i < sizeof(attrs) / sizeof(attrs[0]); i++) {
        if (extractXmlAttributeValue(line, attrs[i], out, outSize)) return true;
    }
    for (uint8_t i = 0; i < sizeof(tags) / sizeof(tags[0]); i++) {
        if (extractXmlTagValue(line, tags[i], out, outSize)) return true;
    }
    return false;
}

static bool extractTypeValue(const char* line, char* out, size_t outSize) {
    static const char* const attrs[] = {
        "deviceType", "type"
    };
    static const char* const tags[] = {
        "DeviceType", "Type"
    };

    for (uint8_t i = 0; i < sizeof(attrs) / sizeof(attrs[0]); i++) {
        if (extractXmlAttributeValue(line, attrs[i], out, outSize)) return true;
    }
    for (uint8_t i = 0; i < sizeof(tags) / sizeof(tags[0]); i++) {
        if (extractXmlTagValue(line, tags[i], out, outSize)) return true;
    }
    return false;
}

static void addViewLine(WagoViewItem* items, uint8_t maxItems, uint8_t* count, const char* fmt, ...) {
    if (!items || !count || *count >= maxItems) return;

    va_list args;
    va_start(args, fmt);
    vsnprintf(items[*count].line1, STORAGE_MAX_WAGO_LINE_LEN, fmt, args);
    va_end(args);
    items[*count].line2[0] = '\0';
    (*count)++;
}

static void addViewLine2(WagoViewItem* items,
                         uint8_t maxItems,
                         uint8_t* count,
                         const char* line1,
                         const char* line2) {
    if (!items || !count || *count >= maxItems) return;

    copyText(items[*count].line1, STORAGE_MAX_WAGO_LINE_LEN, line1);
    copyText(items[*count].line2, STORAGE_MAX_WAGO_LINE_LEN, line2);
    (*count)++;
}

static void sortSdEntries(SdBrowserEntry* entries, uint8_t count) {
    for (uint8_t i = 0; i < count; i++) {
        for (uint8_t j = i + 1; j < count; j++) {
            bool swap = false;
            if (entries[j].isDir != entries[i].isDir) {
                swap = entries[j].isDir;
            } else if (compareIgnoreCase(entries[j].name, entries[i].name) < 0) {
                swap = true;
            }
            if (swap) {
                SdBrowserEntry tmp = entries[i];
                entries[i] = entries[j];
                entries[j] = tmp;
            }
        }
    }
}

static void sortWagoProjects(WagoProjectInfo* projects, uint8_t count) {
    for (uint8_t i = 0; i < count; i++) {
        for (uint8_t j = i + 1; j < count; j++) {
            if (compareIgnoreCase(projects[j].name, projects[i].name) < 0) {
                WagoProjectInfo tmp = projects[i];
                projects[i] = projects[j];
                projects[j] = tmp;
            }
        }
    }
}

static bool pathExists(const char* path) {
    return storageSdEnsureMounted() && SD.exists(path);
}

static bool directoryContainsSolution(const char* dirPath) {
    char candidate[STORAGE_MAX_PATH_LEN];
    return storageJoinPath(dirPath, "solution.xml", candidate, sizeof(candidate)) && pathExists(candidate);
}

static bool isModuleXmlName(const char* name) {
    return findIgnoreCase(name, "module_") == name && endsWithIgnoreCase(name, ".xml");
}

static bool parseModuleSummary(const char* filePath, WagoModuleSummary* summary) {
    if (!summary) return false;

    File file = SD.open(filePath, FILE_READ);
    if (!file) return false;

    char line[192];
    copyText(summary->name, sizeof(summary->name), pathBaseName(filePath));
    summary->gearCount = 0;
    summary->sensorCount = 0;

    while (readTextLine(file, line, sizeof(line))) {
        char value[STORAGE_MAX_NAME_LEN];

        if (findIgnoreCase(line, "<Project ") && extractXmlAttributeValue(line, "Name", value, sizeof(value))) {
            copyText(summary->name, sizeof(summary->name), value);
        }

        if (findIgnoreCase(line, "<ControlGear ")) {
            summary->gearCount++;
        }

        if (findIgnoreCase(line, "<ControlDeviceChildControl ")) {
            summary->sensorCount++;
        }
    }

    file.close();
    return true;
}

static void initWagoGearConfig(WagoGearConfig* gear) {
    if (!gear) return;
    memset(gear, 0, sizeof(*gear));
    gear->shortAddress = 0xFF;
    gear->groups = 0;
    gear->config.valid = true;
    gear->presentInFile = false;
    for (uint8_t i = 0; i < 16; i++) {
        gear->sceneLevels[i] = 255;
    }
}

static void initWagoControlDeviceConfig(WagoControlDeviceConfig* ctrl) {
    if (!ctrl) return;
    memset(ctrl, 0, sizeof(*ctrl));
    ctrl->shortAddress = 0xFF;
    ctrl->presentInFile = false;
}

static const WagoGearConfig* findModuleGearByAddress(const WagoModuleConfig* module, uint8_t addr) {
    if (!module) return nullptr;
    for (uint8_t i = 0; i < module->gearCount; i++) {
        const WagoGearConfig& gear = module->gears[i];
        if (gear.valid && gear.shortAddress == addr) return &gear;
    }
    return nullptr;
}

static const WagoGearConfig* findModuleGearByName(const WagoModuleConfig* module, const char* name) {
    if (!module || !name) return nullptr;
    for (uint8_t i = 0; i < module->gearCount; i++) {
        const WagoGearConfig& gear = module->gears[i];
        if (gear.valid && compareIgnoreCase(gear.name, name) == 0) return &gear;
    }
    return nullptr;
}

static bool replaceXmlAttributeUintLine(const char* src,
                                        const char* attr,
                                        uint16_t value,
                                        char* out,
                                        size_t outSize) {
    const char* attrPos = findIgnoreCase(src, attr);
    if (!attrPos || !out || outSize == 0) return false;

    const char* eq = strchr(attrPos, '=');
    if (!eq) return false;
    const char* quoteStart = eq + 1;
    while (*quoteStart == ' ' || *quoteStart == '\t') quoteStart++;
    if (*quoteStart != '"' && *quoteStart != '\'') return false;
    char quote = *quoteStart;
    const char* valueStart = quoteStart + 1;
    const char* valueEnd = strchr(valueStart, quote);
    if (!valueEnd) return false;

    size_t prefixLen = (size_t)(valueStart - src);
    size_t suffixLen = strlen(valueEnd);
    char valueBuf[16];
    snprintf(valueBuf, sizeof(valueBuf), "%u", (unsigned int)value);

    if (prefixLen + strlen(valueBuf) + suffixLen + 1 > outSize) return false;
    memcpy(out, src, prefixLen);
    strcpy(out + prefixLen, valueBuf);
    strcpy(out + prefixLen + strlen(valueBuf), valueEnd);
    return true;
}

static bool replaceXmlTagValueLine(const char* src,
                                   const char* tag,
                                   const char* value,
                                   char* out,
                                   size_t outSize) {
    if (!src || !tag || !value || !out || outSize == 0) return false;

    char openTag[32];
    char closeTag[32];
    snprintf(openTag, sizeof(openTag), "<%s>", tag);
    snprintf(closeTag, sizeof(closeTag), "</%s>", tag);

    const char* start = findIgnoreCase(src, openTag);
    const char* end = findIgnoreCase(src, closeTag);
    if (!start || !end || end <= start) return false;
    start += strlen(openTag);

    size_t prefixLen = (size_t)(start - src);
    size_t suffixLen = strlen(end);
    size_t valueLen = strlen(value);
    if (prefixLen + valueLen + suffixLen + 1 > outSize) return false;

    memcpy(out, src, prefixLen);
    memcpy(out + prefixLen, value, valueLen);
    strcpy(out + prefixLen + valueLen, end);
    return true;
}

static void writeSceneValuesBlock(File& outFile, const WagoGearConfig* gear, const char* indent) {
    if (!outFile || !gear || !indent) return;
    outFile.print(indent);
    outFile.println("<SceneValues>");
    for (uint8_t i = 0; i < 16; i++) {
        char valueBuf[16];
        if (gear->sceneLevels[i] == 255) {
            strcpy(valueBuf, "255");
        } else {
            snprintf(valueBuf, sizeof(valueBuf), "%u", gear->sceneLevels[i]);
        }
        outFile.print(indent);
        outFile.print("  <Value>");
        outFile.print(valueBuf);
        outFile.println("</Value>");
    }
    outFile.print(indent);
    outFile.println("</SceneValues>");
}

static void applyGearParamValue(WagoGearConfig* gear, const char* path, const char* value) {
    if (!gear || !path || !value) return;

    uint8_t parsed = 0;
    if (endsWithIgnoreCase(path, "MaxLevel") && parseFirstUint(value, &parsed)) {
        gear->config.maxLevel = parsed;
    } else if (endsWithIgnoreCase(path, "MinLevel") && parseFirstUint(value, &parsed)) {
        gear->config.minLevel = parsed;
    } else if (endsWithIgnoreCase(path, "PowerOnLevel") && parseFirstUint(value, &parsed)) {
        gear->config.powerOnLevel = parsed;
    } else if (endsWithIgnoreCase(path, "SystemFailureLevel") && parseFirstUint(value, &parsed)) {
        gear->config.sysFailLevel = parsed;
    } else if (endsWithIgnoreCase(path, "FadeTime") && parseFirstUint(value, &parsed)) {
        gear->config.fadeTime = parsed;
    } else if (endsWithIgnoreCase(path, "FadeRate") && parseFirstUint(value, &parsed)) {
        gear->config.fadeRate = parsed;
    }
}

static bool parseWagoModuleConfigFile(const char* projectPath, const char* filePath, WagoModuleConfig* outModule) {
    if (!filePath || !outModule) return false;

    File file = SD.open(filePath, FILE_READ);
    if (!file) return false;

    memset(outModule, 0, sizeof(*outModule));
    outModule->valid = true;
    extractSolutionTitle(projectPath, outModule->projectName, sizeof(outModule->projectName));
    copyText(outModule->moduleName, sizeof(outModule->moduleName), pathBaseName(filePath));

    char line[224];
    bool inControlGear = false;
    bool inSceneValues = false;
    bool inControlDevice = false;
    bool inControlParams = false;
    uint8_t currentSceneIndex = 0;
    char currentParamPath[48] = "";
    WagoGearConfig currentGear;
    WagoControlDeviceConfig currentCtrl;
    initWagoGearConfig(&currentGear);
    initWagoControlDeviceConfig(&currentCtrl);

    while (readTextLine(file, line, sizeof(line))) {
        char value[STORAGE_MAX_NAME_LEN];

        if (findIgnoreCase(line, "<Project ") && extractXmlAttributeValue(line, "Name", value, sizeof(value))) {
            copyText(outModule->moduleName, sizeof(outModule->moduleName), value);
        }

        if (findIgnoreCase(line, "<ControlGear ")) {
            inControlGear = true;
            inSceneValues = false;
            currentSceneIndex = 0;
            currentParamPath[0] = '\0';
            initWagoGearConfig(&currentGear);
            if (extractXmlAttributeValue(line, "Name", value, sizeof(value))) {
                copyText(currentGear.name, sizeof(currentGear.name), value);
            }
            if (extractXmlAttributeValue(line, "Address", value, sizeof(value))) {
                parseFirstUint(value, &currentGear.shortAddress);
            }
            if (extractXmlAttributeValue(line, "SerializedGroups", value, sizeof(value))) {
                uint16_t groups = 0;
                if (parseUint16Dec(value, &groups)) currentGear.groups = groups;
            }
            continue;
        }

        if (inControlGear) {
            if (findIgnoreCase(line, "<ParameterValue ") &&
                extractXmlAttributeValue(line, "DefinitionPath", currentParamPath, sizeof(currentParamPath))) {
                if (extractXmlTagValue(line, "Value", value, sizeof(value))) {
                    applyGearParamValue(&currentGear, currentParamPath, value);
                }
            } else if (currentParamPath[0] && extractXmlTagValue(line, "Value", value, sizeof(value))) {
                applyGearParamValue(&currentGear, currentParamPath, value);
            } else if (findIgnoreCase(line, "<SceneValues")) {
                inSceneValues = true;
                currentSceneIndex = 0;
            } else if (inSceneValues && extractXmlTagValue(line, "Value", value, sizeof(value))) {
                if (currentSceneIndex < 16) {
                    uint8_t sceneLevel = 255;
                    if (parseFirstUint(value, &sceneLevel)) {
                        currentGear.sceneLevels[currentSceneIndex] = sceneLevel;
                    } else if (compareIgnoreCase(value, "MASK") == 0) {
                        currentGear.sceneLevels[currentSceneIndex] = 255;
                    }
                    currentSceneIndex++;
                }
            }

            if (findIgnoreCase(line, "</SceneValues")) {
                inSceneValues = false;
            }
            if (findIgnoreCase(line, "</ParameterValue")) {
                currentParamPath[0] = '\0';
            }
            if (findIgnoreCase(line, "</ControlGear")) {
                if (currentGear.shortAddress < 64 && outModule->gearCount < STORAGE_MAX_WAGO_GEARS) {
                    currentGear.valid = true;
                    currentGear.presentInFile = true;
                    outModule->gears[outModule->gearCount++] = currentGear;
                }
                inControlGear = false;
            }
            continue;
        }

        if (findIgnoreCase(line, "<ControlDeviceMultiControl ")) {
            inControlDevice = true;
            inControlParams = false;
            initWagoControlDeviceConfig(&currentCtrl);
            if (extractXmlAttributeValue(line, "Name", value, sizeof(value))) {
                copyText(currentCtrl.name, sizeof(currentCtrl.name), value);
            }
            if (extractXmlAttributeValue(line, "Address", value, sizeof(value))) {
                parseFirstUint(value, &currentCtrl.shortAddress);
            }
            continue;
        }

        if (inControlDevice) {
            if (findIgnoreCase(line, "<Children")) {
                inControlParams = true;
            } else if (inControlParams && findIgnoreCase(line, "<ControlDeviceChildControl ")) {
                currentCtrl.sensorCount++;
            } else if (extractXmlTagValue(line, "IsDaliMasterModule", value, sizeof(value))) {
                currentCtrl.isMasterModule = (compareIgnoreCase(value, "true") == 0);
            }

            if (findIgnoreCase(line, "</Children")) {
                inControlParams = false;
            }
            if (findIgnoreCase(line, "</ControlDeviceMultiControl")) {
                if (!currentCtrl.isMasterModule && currentCtrl.shortAddress < 64 &&
                    outModule->ctrlDeviceCount < STORAGE_MAX_WAGO_CTRLDEVS) {
                    currentCtrl.valid = true;
                    currentCtrl.presentInFile = true;
                    outModule->ctrlDevices[outModule->ctrlDeviceCount++] = currentCtrl;
                }
                inControlDevice = false;
            }
        }
    }

    file.close();
    return true;
}

static bool extractSolutionTitle(const char* projectPath, char* outTitle, size_t outTitleSize) {
    char solutionPath[STORAGE_MAX_PATH_LEN];
    if (!storageJoinPath(projectPath, "solution.xml", solutionPath, sizeof(solutionPath))) {
        return false;
    }

    File file = SD.open(solutionPath, FILE_READ);
    if (!file) return false;

    char line[192];
    bool found = false;
    while (readTextLine(file, line, sizeof(line))) {
        if (extractNameValue(line, outTitle, outTitleSize)) {
            found = true;
            break;
        }
    }
    file.close();
    return found;
}

static void addPackedProjectHelp(WagoViewItem* items, uint8_t maxItems, uint8_t* outCount) {
    addViewLine(items, maxItems, outCount, "Packed .wdc2s file");
    addViewLine(items, maxItems, outCount, "Bitte am PC nach ZIP");
    addViewLine(items, maxItems, outCount, "umbenennen und");
    addViewLine(items, maxItems, outCount, "entpacken.");
    addViewLine(items, maxItems, outCount, "XML-Ordner danach");
    addViewLine(items, maxItems, outCount, "auf SD kopieren.");
}

static void findWagoProjectsRecursive(const char* dirPath,
                                      uint8_t depth,
                                      WagoProjectInfo* projects,
                                      uint8_t maxProjects,
                                      uint8_t* count) {
    if (*count >= maxProjects || depth == 0) return;

    File dir = SD.open(dirPath);
    if (!dir || !dir.isDirectory()) {
        if (dir) dir.close();
        return;
    }

    File entry = dir.openNextFile();
    while (entry && *count < maxProjects) {
        const char* rawName = pathBaseName(entry.name());

        if (entry.isDirectory()) {
            char entryPath[STORAGE_MAX_PATH_LEN];
            if (storageJoinPath(dirPath, rawName, entryPath, sizeof(entryPath))) {
                if (directoryContainsSolution(entryPath)) {
                    copyText(projects[*count].name, sizeof(projects[*count].name), rawName);
                    copyText(projects[*count].path, sizeof(projects[*count].path), entryPath);
                    projects[*count].packedFile = false;
                    (*count)++;
                } else {
                    findWagoProjectsRecursive(entryPath, depth - 1, projects, maxProjects, count);
                }
            }
        } else if (endsWithIgnoreCase(rawName, ".wdc2s")) {
            char entryPath[STORAGE_MAX_PATH_LEN];
            if (storageJoinPath(dirPath, rawName, entryPath, sizeof(entryPath))) {
                copyText(projects[*count].name, sizeof(projects[*count].name), rawName);
                copyText(projects[*count].path, sizeof(projects[*count].path), entryPath);
                projects[*count].packedFile = true;
                (*count)++;
            }
        }

        entry.close();
        entry = dir.openNextFile();
    }

    dir.close();
}

bool storageSdEnsureMounted() {
    if (g_sdMounted) return true;

    uint32_t now = millis();
    if (g_sdInitAttempted && (now - g_sdLastAttemptMs) < 1500) {
        return false;
    }

    g_sdInitAttempted = true;
    g_sdLastAttemptMs = now;
    for (uint8_t attempt = 0; attempt < 2; attempt++) {
        SPI.begin(SD_SPI_SCLK, SD_SPI_MISO, SD_SPI_MOSI, SD_CS);
        if (SD.begin(SD_CS, SPI)) {
            g_sdMounted = true;
            copyText(g_sdLastError, sizeof(g_sdLastError), "OK");
            return true;
        }

        if (attempt == 0) {
            delay(30);
        }
    }

    copyText(g_sdLastError, sizeof(g_sdLastError), "SD init failed");
    return g_sdMounted;
}

const char* storageSdGetLastError() {
    return g_sdLastError;
}

bool storageJoinPath(const char* basePath, const char* name, char* outPath, size_t outPathSize) {
    if (!basePath || !name || !outPath || outPathSize == 0) return false;

    if (strcmp(basePath, "/") == 0) {
        snprintf(outPath, outPathSize, "/%s", name);
    } else {
        snprintf(outPath, outPathSize, "%s/%s", basePath, name);
    }
    outPath[outPathSize - 1] = '\0';
    return true;
}

bool storageDirectoryUp(char* path, size_t pathSize) {
    if (!path || pathSize == 0 || strcmp(path, "/") == 0) return false;

    char* lastSlash = strrchr(path, '/');
    if (!lastSlash || lastSlash == path) {
        copyText(path, pathSize, "/");
        return true;
    }

    *lastSlash = '\0';
    return true;
}

bool storageListDirectory(const char* path, SdBrowserEntry* entries, uint8_t maxEntries, uint8_t* outCount) {
    if (outCount) *outCount = 0;
    if (!entries || !outCount) return false;
    if (!storageSdEnsureMounted()) return false;

    File dir = SD.open(path);
    if (!dir || !dir.isDirectory()) {
        copyText(g_sdLastError, sizeof(g_sdLastError), "Directory open failed");
        if (dir) dir.close();
        return false;
    }

    File entry = dir.openNextFile();
    while (entry && *outCount < maxEntries) {
        const char* rawName = pathBaseName(entry.name());
        copyText(entries[*outCount].name, sizeof(entries[*outCount].name), rawName);
        storageJoinPath(path, rawName, entries[*outCount].path, sizeof(entries[*outCount].path));
        entries[*outCount].isDir = entry.isDirectory();
        entries[*outCount].size = entry.isDirectory() ? 0 : (uint32_t)entry.size();
        (*outCount)++;
        entry.close();
        entry = dir.openNextFile();
    }

    dir.close();
    sortSdEntries(entries, *outCount);
    return true;
}

bool storageFindWagoProjects(WagoProjectInfo* projects, uint8_t maxProjects, uint8_t* outCount) {
    if (outCount) *outCount = 0;
    if (!projects || !outCount) return false;
    if (!storageSdEnsureMounted()) return false;

    findWagoProjectsRecursive("/", 4, projects, maxProjects, outCount);
    sortWagoProjects(projects, *outCount);
    return true;
}

bool storageLoadWagoProjectView(const char* projectPath,
                                bool packedFile,
                                WagoViewItem* items,
                                uint8_t maxItems,
                                uint8_t* outCount,
                                char* outTitle,
                                size_t outTitleSize) {
    if (outCount) *outCount = 0;
    if (!projectPath || !items || !outCount || !outTitle) return false;
    if (!storageSdEnsureMounted()) return false;

    copyText(outTitle, outTitleSize, pathBaseName(projectPath));

    if (packedFile) {
        addPackedProjectHelp(items, maxItems, outCount);
        return true;
    }

    char projectName[STORAGE_MAX_NAME_LEN];
    copyText(projectName, sizeof(projectName), pathBaseName(projectPath));
    if (extractSolutionTitle(projectPath, projectName, sizeof(projectName))) {
        copyText(outTitle, outTitleSize, projectName);
    }

    addViewLine(items, maxItems, outCount, "Project: %s", projectName);

    File dir = SD.open(projectPath);
    if (!dir || !dir.isDirectory()) {
        copyText(g_sdLastError, sizeof(g_sdLastError), "Project open failed");
        if (dir) dir.close();
        return false;
    }

    uint8_t xmlCount = 0;
    uint8_t moduleCount = 0;
    uint16_t totalGearCount = 0;
    uint16_t totalSensorCount = 0;

    File entry = dir.openNextFile();
    while (entry) {
        const char* rawName = pathBaseName(entry.name());
        if (!entry.isDirectory() && endsWithIgnoreCase(rawName, ".xml")) {
            xmlCount++;
            if (isModuleXmlName(rawName)) {
                char fullPath[STORAGE_MAX_PATH_LEN];
                if (storageJoinPath(projectPath, rawName, fullPath, sizeof(fullPath))) {
                    WagoModuleSummary summary;
                    if (parseModuleSummary(fullPath, &summary)) {
                        char detail[STORAGE_MAX_WAGO_LINE_LEN];
                        snprintf(detail, sizeof(detail), "Gear:%u Sensor:%u", summary.gearCount, summary.sensorCount);
                        addViewLine2(items, maxItems, outCount, summary.name, detail);
                        moduleCount++;
                        totalGearCount += summary.gearCount;
                        totalSensorCount += summary.sensorCount;
                    }
                }
            }
        }
        entry.close();
        entry = dir.openNextFile();
    }
    dir.close();

    if (*outCount > 0) {
        snprintf(items[0].line1, STORAGE_MAX_WAGO_LINE_LEN, "Project: %s", projectName);
        snprintf(items[0].line2, STORAGE_MAX_WAGO_LINE_LEN, "Cards:%u XML:%u Gear:%u Sensor:%u",
                 moduleCount,
                 xmlCount,
                 (unsigned int)totalGearCount,
                 (unsigned int)totalSensorCount);
    }

    if (moduleCount == 0) {
        addViewLine(items, maxItems, outCount, "No device model found");
        addViewLine(items, maxItems, outCount, "Browse XML via SD menu");
    }

    return true;
}

bool storageLoadWagoModuleConfig(const char* projectPath, uint8_t moduleIndex, WagoModuleConfig* outModule) {
    if (!projectPath || !outModule) return false;
    if (!storageSdEnsureMounted()) return false;

    File dir = SD.open(projectPath);
    if (!dir || !dir.isDirectory()) {
        copyText(g_sdLastError, sizeof(g_sdLastError), "Project open failed");
        if (dir) dir.close();
        return false;
    }

    uint8_t currentIndex = 0;
    bool loaded = false;

    File entry = dir.openNextFile();
    while (entry && !loaded) {
        const char* rawName = pathBaseName(entry.name());
        if (!entry.isDirectory() && isModuleXmlName(rawName)) {
            if (currentIndex == moduleIndex) {
                char fullPath[STORAGE_MAX_PATH_LEN];
                if (storageJoinPath(projectPath, rawName, fullPath, sizeof(fullPath))) {
                    loaded = parseWagoModuleConfigFile(projectPath, fullPath, outModule);
                }
                break;
            }
            currentIndex++;
        }
        entry.close();
        entry = dir.openNextFile();
    }

    if (entry) entry.close();
    dir.close();

    if (!loaded) {
        copyText(g_sdLastError, sizeof(g_sdLastError), "Module load failed");
    }
    return loaded;
}

static bool buildWagoModulePath(const char* projectPath, uint8_t moduleIndex, char* outPath, size_t outPathSize) {
    if (!projectPath || !outPath || outPathSize == 0) return false;

    File dir = SD.open(projectPath);
    if (!dir || !dir.isDirectory()) {
        if (dir) dir.close();
        return false;
    }

    uint8_t currentIndex = 0;
    bool found = false;
    File entry = dir.openNextFile();
    while (entry && !found) {
        const char* rawName = pathBaseName(entry.name());
        if (!entry.isDirectory() && isModuleXmlName(rawName)) {
            if (currentIndex == moduleIndex) {
                found = storageJoinPath(projectPath, rawName, outPath, outPathSize);
                break;
            }
            currentIndex++;
        }
        entry.close();
        entry = dir.openNextFile();
    }

    if (entry) entry.close();
    dir.close();
    return found;
}

bool storageSaveWagoModuleConfig(const char* projectPath, uint8_t moduleIndex, const WagoModuleConfig* moduleConfig) {
    if (!projectPath || !moduleConfig || !moduleConfig->valid) return false;
    if (!storageSdEnsureMounted()) return false;

    char modulePath[STORAGE_MAX_PATH_LEN];
    if (!buildWagoModulePath(projectPath, moduleIndex, modulePath, sizeof(modulePath))) {
        copyText(g_sdLastError, sizeof(g_sdLastError), "Module path failed");
        return false;
    }

    char tempPath[STORAGE_MAX_PATH_LEN];
    snprintf(tempPath, sizeof(tempPath), "%s.tmp", modulePath);
    tempPath[sizeof(tempPath) - 1] = '\0';

    File inFile = SD.open(modulePath, FILE_READ);
    if (!inFile) {
        copyText(g_sdLastError, sizeof(g_sdLastError), "Module open failed");
        return false;
    }

    SD.remove(tempPath);
    File outFile = SD.open(tempPath, FILE_WRITE);
    if (!outFile) {
        inFile.close();
        copyText(g_sdLastError, sizeof(g_sdLastError), "Temp write failed");
        return false;
    }

    char line[256];
    char patched[320];
    bool inControlGear = false;
    bool inSceneValues = false;
    bool skipOldSceneValues = false;
    char currentParamPath[48] = "";
    const WagoGearConfig* activeGear = nullptr;

    while (readTextLine(inFile, line, sizeof(line))) {
        if (!inControlGear && findIgnoreCase(line, "<ControlGear ")) {
            char name[STORAGE_MAX_NAME_LEN] = "";
            uint8_t addr = 0xFF;
            if (extractXmlAttributeValue(line, "Name", name, sizeof(name))) {
                activeGear = findModuleGearByName(moduleConfig, name);
            }
            if (!activeGear && extractXmlAttributeValue(line, "Address", patched, sizeof(patched)) && parseFirstUint(patched, &addr)) {
                activeGear = findModuleGearByAddress(moduleConfig, addr);
            }

            if (activeGear) {
                if (!replaceXmlAttributeUintLine(line, "Address", activeGear->shortAddress, patched, sizeof(patched))) {
                    copyText(patched, sizeof(patched), line);
                }
                char patched2[320];
                if (replaceXmlAttributeUintLine(patched, "PlannedAddress", activeGear->shortAddress, patched2, sizeof(patched2))) {
                    copyText(patched, sizeof(patched), patched2);
                }
                if (replaceXmlAttributeUintLine(patched, "SerializedGroups", activeGear->groups, patched2, sizeof(patched2))) {
                    copyText(patched, sizeof(patched), patched2);
                }
                outFile.println(patched);
            } else {
                outFile.println(line);
            }

            inControlGear = true;
            inSceneValues = false;
            skipOldSceneValues = false;
            currentParamPath[0] = '\0';
            continue;
        }

        if (inControlGear) {
            if (findIgnoreCase(line, "<ParameterValue ") &&
                extractXmlAttributeValue(line, "DefinitionPath", currentParamPath, sizeof(currentParamPath))) {
                if (activeGear && extractXmlTagValue(line, "Value", patched, sizeof(patched))) {
                    char valueBuf[16];
                    uint8_t value = 0;
                    bool handled = true;
                    if (endsWithIgnoreCase(currentParamPath, "MaxLevel")) value = activeGear->config.maxLevel;
                    else if (endsWithIgnoreCase(currentParamPath, "MinLevel")) value = activeGear->config.minLevel;
                    else if (endsWithIgnoreCase(currentParamPath, "PowerOnLevel")) value = activeGear->config.powerOnLevel;
                    else if (endsWithIgnoreCase(currentParamPath, "SystemFailureLevel")) value = activeGear->config.sysFailLevel;
                    else if (endsWithIgnoreCase(currentParamPath, "FadeTime")) value = activeGear->config.fadeTime;
                    else if (endsWithIgnoreCase(currentParamPath, "FadeRate")) value = activeGear->config.fadeRate;
                    else handled = false;

                    if (handled) {
                        snprintf(valueBuf, sizeof(valueBuf), "%u", value);
                        if (replaceXmlTagValueLine(line, "Value", valueBuf, patched, sizeof(patched))) {
                            outFile.println(patched);
                            continue;
                        }
                    }
                }
                outFile.println(line);
                continue;
            }

            if (activeGear && currentParamPath[0] && extractXmlTagValue(line, "Value", patched, sizeof(patched))) {
                char valueBuf[16];
                uint8_t value = 0;
                bool handled = true;
                if (endsWithIgnoreCase(currentParamPath, "MaxLevel")) value = activeGear->config.maxLevel;
                else if (endsWithIgnoreCase(currentParamPath, "MinLevel")) value = activeGear->config.minLevel;
                else if (endsWithIgnoreCase(currentParamPath, "PowerOnLevel")) value = activeGear->config.powerOnLevel;
                else if (endsWithIgnoreCase(currentParamPath, "SystemFailureLevel")) value = activeGear->config.sysFailLevel;
                else if (endsWithIgnoreCase(currentParamPath, "FadeTime")) value = activeGear->config.fadeTime;
                else if (endsWithIgnoreCase(currentParamPath, "FadeRate")) value = activeGear->config.fadeRate;
                else handled = false;

                if (handled) {
                    snprintf(valueBuf, sizeof(valueBuf), "%u", value);
                    if (replaceXmlTagValueLine(line, "Value", valueBuf, patched, sizeof(patched))) {
                        outFile.println(patched);
                        continue;
                    }
                }
            }

            if (findIgnoreCase(line, "</ParameterValue")) {
                currentParamPath[0] = '\0';
                outFile.println(line);
                continue;
            }

            if (findIgnoreCase(line, "<SceneValues")) {
                inSceneValues = true;
                skipOldSceneValues = true;
                if (activeGear) {
                    const char* indent = "      ";
                    for (const char* p = line; *p == ' ' || *p == '\t'; ++p) {
                        size_t len = (size_t)(p - line + 1);
                        if (len < sizeof(patched)) {
                            memcpy(patched, line, len);
                            patched[len] = '\0';
                            indent = patched;
                        }
                    }
                    writeSceneValuesBlock(outFile, activeGear, indent);
                } else {
                    outFile.println(line);
                    skipOldSceneValues = false;
                }
                continue;
            }

            if (inSceneValues) {
                if (findIgnoreCase(line, "</SceneValues")) {
                    inSceneValues = false;
                    if (!skipOldSceneValues) {
                        outFile.println(line);
                    }
                    skipOldSceneValues = false;
                } else if (!skipOldSceneValues) {
                    outFile.println(line);
                }
                continue;
            }

            if (findIgnoreCase(line, "</ControlGear")) {
                outFile.println(line);
                inControlGear = false;
                activeGear = nullptr;
                currentParamPath[0] = '\0';
                continue;
            }
        }

        outFile.println(line);
    }

    inFile.close();
    outFile.close();

    SD.remove(modulePath);
    if (!SD.rename(tempPath, modulePath)) {
        copyText(g_sdLastError, sizeof(g_sdLastError), "Module replace failed");
        return false;
    }

    copyText(g_sdLastError, sizeof(g_sdLastError), "OK");
    return true;
}