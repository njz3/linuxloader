#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "config.h"
#include "gameData.h"
#include "gameOptions.h"
#include "../graphics/gpuVendor.h"
#include "../log/log.h"
#include "iniParser.h"
#include "../mainShared.h"
#include "../version.h"

EmulatorConfig config = {0};

extern uint32_t partialElfCrc;

FILE *configFile = NULL;

#define CONFIG_PATH "linuxloader.ini"
#define MAX_LINE_LENGTH 1024

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

const char *LindbergColourStrings[] = {"Lindbergh Yellow", "Lindbergh Red", "Lindbergh Blue", "Lindbergh Silver", "Lindbergh RedEX"};

const char *GameRegionStrings[] = {"Japan", "US", "Export"};

const char *GpuTypeStrings[] = {"Auto Detection", "NVIDIA", "AMD", "ATI", "INTEL", "Unknown", "ERROR_GPU"};

static int detectGame(uint32_t elf_crc)
{
    const GameData *gameData = getGameData(elf_crc);

    if (gameData)
    {
        config.gameTitle = (char *)gameData->gameTitle;
        config.gameShortTitle = (char *)gameData->gameShortTitle;
        config.gameDVP = (char *)gameData->gameDVP;
        config.gameID = (char *)gameData->gameID;
        config.gameReleaseYear = (char *)gameData->gameReleaseYear;
        config.gameNativeResolutions = (char *)gameData->gameNativeResolutions;
        config.gameStatus = gameData->gameStatus;
        config.jvsIOType = gameData->jvsIOType;
        config.gameType = gameData->gameType;
        config.width = gameData->width;
        config.height = gameData->height;
        config.gameGroup = gameData->gameGroup;
        config.emulateRideboard = gameData->emulateRideboard;
        config.emulateDriveboard = gameData->emulateDriveboard;
        config.emulateMotionboard = gameData->emulateMotionboard;
        config.emulateHW210CardReader = gameData->emulateHW210CardReader;
        config.emulateIDCardReader = gameData->emulateIDCardReader;
        config.emulateTouchscreen = gameData->emulateTouchscreen;
        config.gameLindberghColour = gameData->gameLindberghColour;
        return 0;
    }

    config.crc32 = UNKNOWN;
    return 1;
}

char *getGameName()
{
    return config.gameTitle;
}

// A release's rank in its day, as its package is named: none for the first, "-2" for the second... The release build
// writes it in version.h with the version, the rest of the time it stays empty
#ifndef RELEASE_SUFFIX
#define RELEASE_SUFFIX ""
#endif

// This branch's build, named as its release package (linuxloader-v<version.h>-ffbedition-<date><suffix>-njz3), then
// "FFB Edition". The date is the day config.c was compiled: a release's, as it is built from clean
const char *buildEdition(void)
{
    static char edition[96];
    if (!edition[0])
    {
        static const char months[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
        const char *date = __DATE__; // "Oct 25 2026"
        char month[4] = {date[0], date[1], date[2], '\0'};
        const char *found = strstr(months, month);
        snprintf(edition, sizeof(edition), "linuxloader-v%d.%d.%d-ffbedition-%.4s%02d%02d%s-njz3 FFB Edition", MAJOR_VERSION,
                 MINOR_VERSION, UPDATE_VERSION, date + 7, found ? (int)(found - months) / 3 + 1 : 0, atoi(date + 4),
                 RELEASE_SUFFIX);
    }
    return edition;
}

// The window's title: the game's name, then its short name in brackets, by which BackForceFeeder recognises the game
// (no space nor bracket, kept from one version to the next), the build, and the frame rate when fps is not negative.
// An unknown game has no short name, nor brackets
void formatWindowTitle(char *buffer, size_t size, double fps)
{
    char name[192];
    if (config.gameShortTitle && config.gameShortTitle[0])
        snprintf(name, sizeof(name), "%s [%s]", config.gameTitle, config.gameShortTitle);
    else
        snprintf(name, sizeof(name), "%s", config.gameTitle);
    if (fps >= 0.0)
        snprintf(buffer, size, "%s - %s - FPS: %.2f", name, buildEdition(), fps);
    else
        snprintf(buffer, size, "%s - %s", name, buildEdition());
}

char *getDvpName()
{
    return config.gameDVP;
}

char *getGameId()
{
    return config.gameID;
}

int getGameLindberghColour()
{
    return config.gameLindberghColour;
}

char *getGameReleaseYear()
{
    return config.gameReleaseYear;
}

char *getGameNativeResolutions()
{
    return config.gameNativeResolutions;
}

const char *getLindberghColourString(Colour lindberghColour)
{
    return LindbergColourStrings[lindberghColour];
}

const char *getGameRegionString(GameRegion region)
{
    return GameRegionStrings[region];
}

const char *getGpuTypeString(GpuType gpuType)
{
    return GpuTypeStrings[gpuType];
}

void toLowerCase(char *str)
{
    while (*str)
    {
        *str = tolower((unsigned char)*str);
        str++;
    }
}

void setDefaultValues(EmulatorConfig *cfg)
{
    cfg->emulateRideboard = 0;
    cfg->emulateDriveboard = 0;
    cfg->emulateMotionboard = 0;
    cfg->emulateHW210CardReader = 0;
    cfg->emulateIDCardReader = 0;
    cfg->idCardFileAutoload = 1;
    cfg->emulateTouchscreen = 0;
    strcpy(cfg->cardFile1, "Card_01.crd");
    strcpy(cfg->cardFile2, "Card_02.crd");
    strcpy(cfg->idCardFolder, "");
    cfg->emulateJVS = 1;
    cfg->fullscreen = 0;
    cfg->aspectRatio = ASPECT_ORIGINAL;
    cfg->cropPosition = 0;
    cfg->cropBarrel = 0;
    cfg->barrelCenter = 60;
    cfg->barrelRadius = 50;
    cfg->displayMode = -1;
    cfg->lindberghColour = YELLOW;
    strcpy(cfg->eepromPath, "eeprom.bin");
    strcpy(cfg->sramPath, "sram.bin");
    strcpy(cfg->libCgPath, "");
    strcpy(cfg->jvsPath, "/dev/ttyUSB0");
    strcpy(cfg->serial1Path, "/dev/ttyS0");
    strcpy(cfg->serial2Path, "/dev/ttyS1");
    cfg->width = 640;
    cfg->height = 480;
    cfg->boostRenderRes = 1;
    cfg->region = EX;
    cfg->freeplay = -1;
    cfg->showDebugMessages = 0;
    cfg->useAltJvsPassthrough = 0;
    cfg->hummerFlickerFix = 0;
    cfg->keepAspectRatio = 1;
    cfg->ramboGunsSwitch = 0;
    cfg->harleyAnalogRemap = 0;
    cfg->id5ChineseLanguage = 0;
    cfg->idSteeringPercentageReduction = 0.0f;
    cfg->lgjRenderWithMesa = 1;
    cfg->gameTitle = "Unknown game";
    cfg->gameID = "XXXX";
    cfg->gameDVP = "DVP-XXXX";
    cfg->gameType = SHOOTING;
    cfg->gameLindberghColour = YELLOW;
    cfg->gameReleaseYear = "";
    cfg->gameNativeResolutions = "";
    cfg->jvsIOType = SEGA_TYPE_3;
    cfg->GPUVendor = AUTO_DETECT_GPU;
    cfg->fpsLimiter = 1;
    cfg->fpsTarget = 60.0f;
    cfg->phScreenMode = 2;
    cfg->phTestScreenSingle = 1;
    cfg->phUseZink = 1;
    cfg->disableBuiltinFont = 0;
    cfg->disableBuiltinLogos = 0;
    cfg->hideCursor = 1;
    cfg->customCursorEnabled = 0;
    strcpy(cfg->customCursor, "");
    cfg->customCursorWidth = 32;
    cfg->customCursorHeight = 32;
    strcpy(cfg->touchCursor, "");
    cfg->touchCursorWidth = 32;
    cfg->touchCursorHeight = 32;
    cfg->mj4EnabledAtT = 0;
    cfg->enableNetworkPatches = 1;
    cfg->outputsNetwork = 0;
    cfg->outputsNetworkPort = 8000;
    cfg->outputsNetworkUdpPort = 8001;
    cfg->ffbStrength = 100;
    cfg->ffbInvert = 0;
    strcpy(cfg->gamesDir, "");
    strcpy(cfg->language, "auto");
    cfg->settingsMenu = 1;
    strcpy(cfg->idIpSeat1, "");
    strcpy(cfg->idIpSeat2, "");
    strcpy(cfg->nicName, "");
    strcpy(cfg->or2IP, "");
    strcpy(cfg->or2Netmask, "");
    strcpy(cfg->IpCab1, "");
    strcpy(cfg->IpCab2, "");
    strcpy(cfg->IpCab3, "");
    strcpy(cfg->IpCab4, "");
    strcpy(cfg->tooSpicyIpCab1, "");
    strcpy(cfg->tooSpicyIpCab2, "");
    strcpy(cfg->srtvIP, "");
    cfg->cpuFreqGhz = 0.0f;
    memset(&cfg->arcadeInputs, 0, sizeof(cfg->arcadeInputs));
    cfg->crc32 = 0;
    cfg->gameGroup = GROUP_UNKNOWN;
    cfg->skipOutrunCabinetCheck = 0;
    cfg->borderEnabled = 0;
    cfg->whiteBorderPercentage = 0.02f;
    cfg->blackBorderPercentage = 0.0f;
    cfg->inputMode = 1;
    cfg->enableCrosshairs = 0;
    cfg->gsevoCrosshairAlwaysOn = 0;
    cfg->gsevoCrosshairAlwaysOff = 0;
    cfg->noCardReader = 1;
    strcpy(cfg->p1CrossHairPath, "");
    strcpy(cfg->p2CrossHairPath, "");
    cfg->customCrossHairWidth = 64;
    cfg->customCrossHairHeight = 64;
    cfg->fpsOverlayEnabled = 0;
    cfg->fpsOverlayPosition = 0;
}

static const char *getValue(const IniConfig *ini, const char *sectionName, const char *key)
{
    IniSection *section = iniGetSection(ini, sectionName);
    if (!section)
    {
        return NULL;
    }
    for (int i = 0; i < section->numPairs; i++)
    {
        if (strcmp(section->pairs[i].key, key) == 0)
        {
            return section->pairs[i].value;
        }
    }
    return NULL;
}

static char *cleanValue(const char *rawValue, char *buffer, size_t bufferSize)
{
    if (!rawValue || bufferSize == 0)
        return NULL;

    strncpy(buffer, rawValue, bufferSize - 1);
    buffer[bufferSize - 1] = '\0';

    char *start = buffer;
    char *end = buffer + strlen(buffer) - 1;

    while (isspace((unsigned char)*start))
        start++;
    while (end > start && isspace((unsigned char)*end))
        end--;
    *(end + 1) = '\0';

    if (*start == '"' && *end == '"')
    {
        start++;
        *end = '\0';
    }
    return start;
}

static int getInt(const IniConfig *ini, const char *section, const char *key, int defaultValue)
{
    const char *rawValue = getValue(ini, section, key);
    if (!rawValue)
        return defaultValue;

    char cleanBuffer[256];
    char *valueStr = cleanValue(rawValue, cleanBuffer, sizeof(cleanBuffer));
    if (!valueStr)
        return defaultValue;

    char tempStr[256];
    strncpy(tempStr, valueStr, sizeof(tempStr) - 1);
    tempStr[sizeof(tempStr) - 1] = '\0';
    toLowerCase(tempStr);

    if (strcmp(tempStr, "auto") == 0)
    {
        return defaultValue;
    }
    if (strcmp(tempStr, "true") == 0)
    {
        return 1;
    }
    if (strcmp(tempStr, "false") == 0)
    {
        return 0;
    }
    if (strcmp(tempStr, "none") == 0)
    {
        return -1;
    }
    return atoi(valueStr);
}

static float getFloat(const IniConfig *ini, const char *section, const char *key, float defaultValue)
{
    const char *rawValue = getValue(ini, section, key);
    if (!rawValue)
        return defaultValue;
    char cleanBuffer[256];
    char *valueStr = cleanValue(rawValue, cleanBuffer, sizeof(cleanBuffer));
    return valueStr ? atof(valueStr) : defaultValue;
}

static void getString(const IniConfig *ini, const char *section, const char *key, char *dest, int dest_size)
{
    const char *rawValue = getValue(ini, section, key);
    if (rawValue)
    {
        char cleanBuffer[MAX_PATH_LENGTH];
        char *valueStr = cleanValue(rawValue, cleanBuffer, sizeof(cleanBuffer));
        if (valueStr)
        {
            strncpy(dest, valueStr, dest_size - 1);
            dest[dest_size - 1] = '\0';
        }
    }
}

void applyIniConfig(EmulatorConfig *config, const IniConfig *ini)
{
    // [Display]
    config->width = getInt(ini, "Display", "WIDTH", config->width);
    config->height = getInt(ini, "Display", "HEIGHT", config->height);
    config->internalWidth = getInt(ini, "Display", "WIDTH", 0); // AUTO stays 0
    config->internalHeight = getInt(ini, "Display", "HEIGHT", 0);
    config->boostRenderRes = getInt(ini, "Display", "BOOST_RENDER_RES", config->boostRenderRes);
    config->fullscreen = getInt(ini, "Display", "FULLSCREEN", config->fullscreen);
    char displayValue[32] = "";
    getString(ini, "Display", "ASPECT_RATIO", displayValue, sizeof(displayValue));
    toLowerCase(displayValue);
    if (strcmp(displayValue, "4:3") == 0)
        config->aspectRatio = ASPECT_4_3;
    else if (strcmp(displayValue, "4:3_letterbox") == 0 || strcmp(displayValue, "4:3 letterbox") == 0)
        config->aspectRatio = ASPECT_4_3_LETTERBOX;
    else if (strcmp(displayValue, "16:9") == 0)
        config->aspectRatio = ASPECT_16_9;
    else if (strcmp(displayValue, "4:3_stretch") == 0 || strcmp(displayValue, "4:3 stretch") == 0)
        config->aspectRatio = ASPECT_4_3_STRETCH;
    else if (strcmp(displayValue, "16:9_stretch") == 0 || strcmp(displayValue, "16:9 stretch") == 0)
        config->aspectRatio = ASPECT_16_9_STRETCH;
    else if (strcmp(displayValue, "4:3_crop") == 0 || strcmp(displayValue, "4:3 crop") == 0)
        config->aspectRatio = ASPECT_4_3_CROP;
    else if (strcmp(displayValue, "16:9_crop") == 0 || strcmp(displayValue, "16:9 crop") == 0)
        config->aspectRatio = ASPECT_16_9_CROP;
    else if (strcmp(displayValue, "original") == 0)
        config->aspectRatio = ASPECT_ORIGINAL;
    config->cropPosition = getInt(ini, "Display", "CROP_POSITION", config->cropPosition);
    config->cropPosition = config->cropPosition < -100 ? -100 : config->cropPosition > 100 ? 100 : config->cropPosition;
    config->cropBarrel = getInt(ini, "Display", "CROP_BARREL", config->cropBarrel);
    config->barrelCenter = getInt(ini, "Display", "BARREL_CENTER", config->barrelCenter);
    config->barrelCenter = config->barrelCenter < 0 ? 0 : config->barrelCenter > 100 ? 100 : config->barrelCenter;
    config->barrelRadius = getInt(ini, "Display", "BARREL_RADIUS", config->barrelRadius);
    config->barrelRadius = config->barrelRadius < 1 ? 1 : config->barrelRadius > 1000 ? 1000 : config->barrelRadius;
    displayValue[0] = '\0';
    getString(ini, "Display", "DISPLAY_MODE", displayValue, sizeof(displayValue));
    toLowerCase(displayValue);
    if (strcmp(displayValue, "window") == 0)
        config->displayMode = DISPLAY_WINDOW;
    else if (strcmp(displayValue, "window_stretch") == 0)
        config->displayMode = DISPLAY_WINDOW_STRETCH;
    else if (strcmp(displayValue, "fullscreen") == 0)
        config->displayMode = DISPLAY_FULLSCREEN;
    else if (strcmp(displayValue, "fullscreen_stretch") == 0)
        config->displayMode = DISPLAY_FULLSCREEN_STRETCH;
    else if (strcmp(displayValue, "borderless") == 0)
        config->displayMode = DISPLAY_BORDERLESS;
    else if (strcmp(displayValue, "borderless_stretch") == 0)
        config->displayMode = DISPLAY_BORDERLESS_STRETCH;
    if (config->displayMode >= 0) // DISPLAY_MODE takes over FULLSCREEN
        config->fullscreen = config->displayMode >= DISPLAY_FULLSCREEN;
    config->borderEnabled = getInt(ini, "Display", "BORDER_ENABLED", config->borderEnabled);
	config->whiteBorderPercentage = getFloat(ini, "Display", "WHITE_BORDER_PERCENTAGE", config->whiteBorderPercentage * 100.0f) / 100.0f;
	config->blackBorderPercentage = getFloat(ini, "Display", "BLACK_BORDER_PERCENTAGE", config->blackBorderPercentage * 100.0f) / 100.0f;
    config->keepAspectRatio = getInt(ini, "Display", "KEEP_ASPECT_RATIO", config->keepAspectRatio);
    config->hideCursor = getInt(ini, "Display", "HIDE_CURSOR", config->hideCursor);
    config->fpsLimiter = getInt(ini, "Display", "FPS_LIMITER_ENABLED", config->fpsLimiter);
    config->fpsTarget = getFloat(ini, "Display", "FPS_TARGET", config->fpsTarget);
    config->fpsOverlayEnabled = getInt(ini, "Display", "FPS_OVERLAY_ENABLED", config->fpsOverlayEnabled);
    config->fpsOverlayPosition = getInt(ini, "Display", "FPS_OVERLAY_POSITION", config->fpsOverlayPosition);

    // [Input]
    config->inputMode = getInt(ini, "Input", "INPUT_MODE", config->inputMode);

    // [Emulation]
    const char *regionRaw = getValue(ini, "Emulation", "REGION");
    if (regionRaw)
    {
        char cleanBuffer[16];
        char *regionStr = cleanValue(regionRaw, cleanBuffer, sizeof(cleanBuffer));
        if (strcmp(regionStr, "JP") == 0)
            config->region = JP;
        else if (strcmp(regionStr, "US") == 0)
            config->region = US;
        else if (strcmp(regionStr, "EX") == 0)
            config->region = EX;
    }
    config->freeplay = getInt(ini, "Emulation", "FREEPLAY", config->freeplay);
    config->emulateJVS = getInt(ini, "Emulation", "EMULATE_JVS", config->emulateJVS);
    config->emulateRideboard = getInt(ini, "Emulation", "EMULATE_RIDEBOARD", config->emulateRideboard);
    config->emulateDriveboard = getInt(ini, "Emulation", "EMULATE_DRIVEBOARD", config->emulateDriveboard);
    config->emulateMotionboard = getInt(ini, "Emulation", "EMULATE_MOTIONBOARD", config->emulateMotionboard);
    config->emulateHW210CardReader = getInt(ini, "Emulation", "EMULATE_HW210_CARDREADER", config->emulateHW210CardReader);
    config->emulateIDCardReader = getInt(ini, "Emulation", "EMULATE_ID_CARDREADER", config->emulateIDCardReader);
    config->emulateTouchscreen = getInt(ini, "Emulation", "EMULATE_TOUCHSCREEN", config->emulateTouchscreen);

    // [Cards]
    config->idCardFileAutoload = getInt(ini, "Cards", "ID_CARDFILE_AUTOLOAD", config->idCardFileAutoload);
    getString(ini, "Cards", "CARDFILE_01", config->cardFile1, MAX_PATH_LENGTH);
    getString(ini, "Cards", "CARDFILE_02", config->cardFile2, MAX_PATH_LENGTH);
    getString(ini, "Cards", "ID_CARDFOLDER", config->idCardFolder, MAX_PATH_LENGTH);

    // [Paths]
    getString(ini, "Paths", "JVS_PATH", config->jvsPath, MAX_PATH_LENGTH);
    getString(ini, "Paths", "SERIAL_1_PATH", config->serial1Path, MAX_PATH_LENGTH);
    getString(ini, "Paths", "SERIAL_2_PATH", config->serial2Path, MAX_PATH_LENGTH);
    getString(ini, "Paths", "SRAM_PATH", config->sramPath, MAX_PATH_LENGTH);
    getString(ini, "Paths", "EEPROM_PATH", config->eepromPath, MAX_PATH_LENGTH);
    getString(ini, "Paths", "LIBCG_PATH", config->libCgPath, MAX_PATH_LENGTH);

    // [Graphics]
    config->hummerFlickerFix = getInt(ini, "Graphics", "HUMMER_FLICKER_FIX", config->hummerFlickerFix);
    config->lgjRenderWithMesa = getInt(ini, "Graphics", "LGJ_RENDER_WITH_MESA", config->lgjRenderWithMesa);
    config->disableBuiltinFont = getInt(ini, "Graphics", "DISABLE_BUILTIN_FONT", config->disableBuiltinFont);
    config->disableBuiltinLogos = getInt(ini, "Graphics", "DISABLE_BUILTIN_LOGOS", config->disableBuiltinLogos);

    // [Cursor]
    config->customCursorEnabled = getInt(ini, "Cursor", "CUSTOM_CURSOR_ENABLED", config->customCursorEnabled);
    getString(ini, "Cursor", "CUSTOM_CURSOR", config->customCursor, MAX_PATH_LENGTH);
    config->customCursorWidth = getInt(ini, "Cursor", "CUSTOM_CURSOR_WIDTH", config->customCursorWidth);
    config->customCursorHeight = getInt(ini, "Cursor", "CUSTOM_CURSOR_HEIGHT", config->customCursorHeight);
    getString(ini, "Cursor", "TOUCH_CURSOR", config->touchCursor, MAX_PATH_LENGTH);
    config->touchCursorWidth = getInt(ini, "Cursor", "TOUCH_CURSOR_WIDTH", config->touchCursorWidth);
    config->touchCursorHeight = getInt(ini, "Cursor", "TOUCH_CURSOR_HEIGHT", config->touchCursorHeight);

    // [GameSpecific]
    config->phScreenMode = getInt(ini, "GameSpecific", "PRIMEVAL_HUNT_SCREEN_MODE", config->phScreenMode);
    config->phTestScreenSingle = getInt(ini, "GameSpecific", "PRIMEVAL_HUNT_TEST_SCREEN_SINGLE", config->phTestScreenSingle);
    config->phUseZink = getInt(ini, "GameSpecific", "PRIMEVAL_HUNT_USE_ZINK", config->phUseZink);
    config->skipOutrunCabinetCheck = getInt(ini, "GameSpecific", "SKIP_OUTRUN_CABINET_CHECK", config->skipOutrunCabinetCheck);
    config->mj4EnabledAtT = getInt(ini, "GameSpecific", "MJ4_ENABLED_ALL_THE_TIME", config->mj4EnabledAtT);
    config->cpuFreqGhz = getFloat(ini, "GameSpecific", "CPU_FREQ_GHZ", config->cpuFreqGhz);
    config->ramboGunsSwitch = getInt(ini, "GameSpecific", "RAMBO_GUNS_SWITCH", config->ramboGunsSwitch);
    config->harleyAnalogRemap = getInt(ini, "GameSpecific", "HARLEY_ANALOG_REMAP", config->harleyAnalogRemap);
    config->id5ChineseLanguage = getInt(ini, "GameSpecific", "ID5_CHINESE_LANGUAGE", config->id5ChineseLanguage);
    config->idSteeringPercentageReduction =
        getFloat(ini, "GameSpecific", "ID_STEERING_REDUCTION_PERCENTAGE", config->idSteeringPercentageReduction);

    // [CrossHairs]
    config->enableCrosshairs = getInt(ini, "CrossHairs", "ENABLE_CROSSHAIRS", config->enableCrosshairs);
    getString(ini, "CrossHairs", "P1_CROSSHAIR_PATH", config->p1CrossHairPath, MAX_PATH_LENGTH);
    getString(ini, "CrossHairs", "P2_CROSSHAIR_PATH", config->p2CrossHairPath, MAX_PATH_LENGTH);
    config->customCrossHairWidth = getInt(ini, "CrossHairs", "CUSTOM_CROSSHAIRS_WIDTH", config->customCrossHairWidth);
    config->customCrossHairHeight = getInt(ini, "CrossHairs", "CUSTOM_CROSSHAIRS_HEIGHT", config->customCrossHairHeight);
    config->gsevoCrosshairAlwaysOn = getInt(ini, "CrossHairs", "GSEVO_CROSSHAIR_ALWAYS_ON", config->gsevoCrosshairAlwaysOn);
    config->gsevoCrosshairAlwaysOff = getInt(ini, "CrossHairs", "GSEVO_CROSSHAIR_ALWAYS_OFF", config->gsevoCrosshairAlwaysOff);

    config->showDebugMessages = getInt(ini, "System", "DEBUG_MSGS", config->showDebugMessages);
    config->useAltJvsPassthrough = getInt(ini, "System", "USE_ALT_JVS_PASSTHROUGH", config->useAltJvsPassthrough);
    const char *colourRaw = getValue(ini, "System", "LINDBERGH_COLOUR");
    if (colourRaw)
    {
        char cleanBuffer[32];
        char *colourStr = cleanValue(colourRaw, cleanBuffer, sizeof(cleanBuffer));
        for (char *p = colourStr; *p; ++p)
            *p = toupper(*p);
        if (strcmp(colourStr, "RED") == 0)
            config->lindberghColour = RED;
        else if (strcmp(colourStr, "YELLOW") == 0)
            config->lindberghColour = YELLOW;
        else if (strcmp(colourStr, "BLUE") == 0)
            config->lindberghColour = BLUE;
        else if (strcmp(colourStr, "SILVER") == 0)
            config->lindberghColour = SILVER;
        else if (strcmp(colourStr, "REDEX") == 0)
            config->lindberghColour = REDEX;
    }

    // [Network]
    config->enableNetworkPatches = getInt(ini, "Network", "ENABLE_NETWORK_PATCHES", config->enableNetworkPatches);
    config->outputsNetwork = getInt(ini, "Outputs", "OUTPUTS_NETWORK", config->outputsNetwork);
    config->outputsNetworkPort = getInt(ini, "Outputs", "OUTPUTS_NETWORK_PORT", config->outputsNetworkPort);
    config->outputsNetworkUdpPort = getInt(ini, "Outputs", "OUTPUTS_NETWORK_UDP_PORT", config->outputsNetworkUdpPort);
    config->ffbStrength = getInt(ini, "ForceFeedback", "FFB_STRENGTH", config->ffbStrength);
    config->ffbInvert = getInt(ini, "ForceFeedback", "FFB_INVERT", config->ffbInvert);
    getString(ini, "Launcher", "GAMES_DIR", config->gamesDir, MAX_PATH_LENGTH);
    getString(ini, "Launcher", "LANGUAGE", config->language, sizeof(config->language));
    config->settingsMenu = getInt(ini, "Launcher", "SETTINGS_MENU", config->settingsMenu);
    getString(ini, "Network", "ID_IP_SEAT_1", config->idIpSeat1, 16);
    getString(ini, "Network", "ID_IP_SEAT_2", config->idIpSeat2, 16);
    getString(ini, "Network", "NIC_NAME", config->nicName, 20);
    getString(ini, "Network", "OR2_IPADDRESS", config->or2IP, 16);
    getString(ini, "Network", "OR2_NETMASK", config->or2Netmask, 16);
    getString(ini, "Network", "IP_CAB1", config->IpCab1, 16);
    getString(ini, "Network", "IP_CAB2", config->IpCab2, 16);
    getString(ini, "Network", "IP_CAB3", config->IpCab3, 16);
    getString(ini, "Network", "IP_CAB4", config->IpCab4, 16);
    getString(ini, "Network", "2SPICY_IP_CAB1", config->tooSpicyIpCab1, 16);
    getString(ini, "Network", "2SPICY_IP_CAB2", config->tooSpicyIpCab2, 16);
    getString(ini, "Network", "SRTV_IPADDRESS", config->srtvIP, 16);

    // [EVDEV]
    getString(ini, "EVDEV", "TEST_BUTTON", config->arcadeInputs.test, INPUT_STRING_LENGTH);
    getString(ini, "EVDEV", "EXIT_GAME", config->arcadeInputs.exit_game, INPUT_STRING_LENGTH);
    getString(ini, "EVDEV", "PLAYER_1_COIN", config->arcadeInputs.player1_coin, INPUT_STRING_LENGTH);
    getString(ini, "EVDEV", "PLAYER_1_BUTTON_START", config->arcadeInputs.player1_button_start, INPUT_STRING_LENGTH);
    getString(ini, "EVDEV", "PLAYER_1_BUTTON_SERVICE", config->arcadeInputs.player1_button_service, INPUT_STRING_LENGTH);
    getString(ini, "EVDEV", "PLAYER_1_BUTTON_UP", config->arcadeInputs.player1_button_up, INPUT_STRING_LENGTH);
    getString(ini, "EVDEV", "PLAYER_1_BUTTON_DOWN", config->arcadeInputs.player1_button_down, INPUT_STRING_LENGTH);
    getString(ini, "EVDEV", "PLAYER_1_BUTTON_LEFT", config->arcadeInputs.player1_button_left, INPUT_STRING_LENGTH);
    getString(ini, "EVDEV", "PLAYER_1_BUTTON_RIGHT", config->arcadeInputs.player1_button_right, INPUT_STRING_LENGTH);
    getString(ini, "EVDEV", "PLAYER_1_BUTTON_1", config->arcadeInputs.player1_button_1, INPUT_STRING_LENGTH);
    getString(ini, "EVDEV", "PLAYER_1_BUTTON_2", config->arcadeInputs.player1_button_2, INPUT_STRING_LENGTH);
    getString(ini, "EVDEV", "PLAYER_1_BUTTON_3", config->arcadeInputs.player1_button_3, INPUT_STRING_LENGTH);
    getString(ini, "EVDEV", "PLAYER_1_BUTTON_4", config->arcadeInputs.player1_button_4, INPUT_STRING_LENGTH);
    getString(ini, "EVDEV", "PLAYER_1_BUTTON_5", config->arcadeInputs.player1_button_5, INPUT_STRING_LENGTH);
    getString(ini, "EVDEV", "PLAYER_1_BUTTON_6", config->arcadeInputs.player1_button_6, INPUT_STRING_LENGTH);
    getString(ini, "EVDEV", "PLAYER_1_BUTTON_7", config->arcadeInputs.player1_button_7, INPUT_STRING_LENGTH);
    getString(ini, "EVDEV", "PLAYER_1_BUTTON_8", config->arcadeInputs.player1_button_8, INPUT_STRING_LENGTH);
    getString(ini, "EVDEV", "PLAYER_2_COIN", config->arcadeInputs.player2_coin, INPUT_STRING_LENGTH);
    getString(ini, "EVDEV", "PLAYER_2_BUTTON_START", config->arcadeInputs.player2_button_start, INPUT_STRING_LENGTH);
    getString(ini, "EVDEV", "PLAYER_2_BUTTON_SERVICE", config->arcadeInputs.player2_button_service, INPUT_STRING_LENGTH);
    getString(ini, "EVDEV", "PLAYER_2_BUTTON_UP", config->arcadeInputs.player2_button_up, INPUT_STRING_LENGTH);
    getString(ini, "EVDEV", "PLAYER_2_BUTTON_DOWN", config->arcadeInputs.player2_button_down, INPUT_STRING_LENGTH);
    getString(ini, "EVDEV", "PLAYER_2_BUTTON_LEFT", config->arcadeInputs.player2_button_left, INPUT_STRING_LENGTH);
    getString(ini, "EVDEV", "PLAYER_2_BUTTON_RIGHT", config->arcadeInputs.player2_button_right, INPUT_STRING_LENGTH);
    getString(ini, "EVDEV", "PLAYER_2_BUTTON_1", config->arcadeInputs.player2_button_1, INPUT_STRING_LENGTH);
    getString(ini, "EVDEV", "PLAYER_2_BUTTON_2", config->arcadeInputs.player2_button_2, INPUT_STRING_LENGTH);
    getString(ini, "EVDEV", "PLAYER_2_BUTTON_3", config->arcadeInputs.player2_button_3, INPUT_STRING_LENGTH);
    getString(ini, "EVDEV", "PLAYER_2_BUTTON_4", config->arcadeInputs.player2_button_4, INPUT_STRING_LENGTH);
    getString(ini, "EVDEV", "PLAYER_2_BUTTON_5", config->arcadeInputs.player2_button_5, INPUT_STRING_LENGTH);
    getString(ini, "EVDEV", "PLAYER_2_BUTTON_6", config->arcadeInputs.player2_button_6, INPUT_STRING_LENGTH);
    getString(ini, "EVDEV", "PLAYER_2_BUTTON_7", config->arcadeInputs.player2_button_7, INPUT_STRING_LENGTH);
    getString(ini, "EVDEV", "PLAYER_2_BUTTON_8", config->arcadeInputs.player2_button_8, INPUT_STRING_LENGTH);

    getString(ini, "EVDEV", "ANALOGUE_1", config->arcadeInputs.analogue_1, INPUT_STRING_LENGTH);
    getString(ini, "EVDEV", "ANALOGUE_2", config->arcadeInputs.analogue_2, INPUT_STRING_LENGTH);
    getString(ini, "EVDEV", "ANALOGUE_3", config->arcadeInputs.analogue_3, INPUT_STRING_LENGTH);
    getString(ini, "EVDEV", "ANALOGUE_4", config->arcadeInputs.analogue_4, INPUT_STRING_LENGTH);
    getString(ini, "EVDEV", "ANALOGUE_5", config->arcadeInputs.analogue_5, INPUT_STRING_LENGTH);
    getString(ini, "EVDEV", "ANALOGUE_6", config->arcadeInputs.analogue_6, INPUT_STRING_LENGTH);
    getString(ini, "EVDEV", "ANALOGUE_7", config->arcadeInputs.analogue_7, INPUT_STRING_LENGTH);
    getString(ini, "EVDEV", "ANALOGUE_8", config->arcadeInputs.analogue_8, INPUT_STRING_LENGTH);

    for (int i = 0; i < 8; i++)
    {
        char key[32];
        sprintf(key, "ANALOGUE_DEADZONE_%d", i + 1);
        const char *rawDz = getValue(ini, "EVDEV", key);
        if (rawDz)
        {
            char cleanBuffer[64];
            char *dzStr = cleanValue(rawDz, cleanBuffer, sizeof(cleanBuffer));
            if (dzStr)
            {
                sscanf(dzStr, "%d %d %d", &config->arcadeInputs.analogue_deadzone_start[i],
                       &config->arcadeInputs.analogue_deadzone_middle[i], &config->arcadeInputs.analogue_deadzone_end[i]);
            }
        }
    }
}

// --set SECTION.KEY=VALUE: the section matched without case, the key in capitals, as the ini files write them
#define MAX_OVERRIDES 32
static char *overrides[MAX_OVERRIDES];
static int overrideCount;
static char configFileUsed[PATH_MAX];

void configAddOverride(const char *assignment)
{
    if (overrideCount < MAX_OVERRIDES)
        overrides[overrideCount++] = strdup(assignment);
    else
        log_warn("--set: more than %d settings, %s ignored", MAX_OVERRIDES, assignment);
}

static bool sameText(const char *a, const char *b)
{
    while (*a && tolower((unsigned char)*a) == tolower((unsigned char)*b))
        a++, b++;
    return tolower((unsigned char)*a) == tolower((unsigned char)*b);
}

static void applyOverrides(IniConfig *ini)
{
    static const char *sections[] = {"Cards",    "CrossHairs", "Cursor",  "Display", "EVDEV",   "Emulation", "ForceFeedback", "GameSpecific",
                                     "Graphics", "Input",      "Launcher", "Network", "Outputs", "Paths",     "System"};
    for (int i = 0; i < overrideCount; i++)
    {
        char assignment[512];
        snprintf(assignment, sizeof(assignment), "%s", overrides[i]);
        char *equals = strchr(assignment, '=');
        char *dot = strchr(assignment, '.');
        if (!equals || !dot || dot > equals)
        {
            log_warn("--set %s: expected SECTION.KEY=VALUE", overrides[i]);
            continue;
        }
        *dot = '\0';
        *equals = '\0';
        const char *section = assignment;
        for (size_t s = 0; s < sizeof(sections) / sizeof(sections[0]); s++)
            if (sameText(section, sections[s]))
                section = sections[s];
        if (sameText(section, "Game")) // the options of the game being run, [Game.<short name>]
            section = gameOptionSection();
        char *key = dot + 1;
        for (char *c = key; *c; c++)
            *c = (char)toupper((unsigned char)*c);
        iniSetValue(ini, section, key, equals + 1);
        log_info("--set %s.%s = %s", section, key, equals + 1);
    }
}

// The options of the game being run, from its [Game.<short name>] section, over the general keys (gameOptions.c)
static void applyGameOptions(const IniConfig *ini)
{
    const char *section = gameOptionSection();
    for (int i = 0; section[0] && i < gameOptionCount; i++)
    {
        const GameOption *option = &gameOptionTable[i];
        if (!option->applies())
            continue;
        const char *raw = getValue(ini, section, option->key);
        char clean[256];
        const char *value = raw ? cleanValue(raw, clean, sizeof(clean)) : NULL;
        if (value && !gameOptionParse(option, value))
            log_warn("[%s] %s = %s: not a value of this option", section, option->key, value);
    }
    for (int i = 0; section[0] && i < gameBoardCount; i++)
    {
        const GameBoard *board = &gameBoardTable[i];
        if (!board->applies())
            continue;
        const char *rawEmulate = getValue(ini, section, board->emulateKey);
        const char *rawPath = getValue(ini, section, board->pathKey);
        char emulate[64], path[MAX_PATH_LENGTH];
        gameBoardLoad(board, rawEmulate ? cleanValue(rawEmulate, emulate, sizeof(emulate)) : NULL,
                      rawPath ? cleanValue(rawPath, path, sizeof(path)) : NULL);
    }
    gameOptionsRemember();
}

// <short name>.ini in the folder of linuxloader.ini, the game's own settings (Tab's "Save for this game", the picker)
static void useGameConfigFile(char *filePath)
{
    const char *name = strrchr(filePath, '/');
    const char *backslash = strrchr(filePath, '\\');
    if (!name || (backslash && backslash > name))
        name = backslash;
    name = name ? name + 1 : filePath;
    if (!config.gameShortTitle || !config.gameShortTitle[0] || !sameText(name, CONFIG_PATH))
        return;
    char gameFile[PATH_MAX];
    snprintf(gameFile, sizeof(gameFile), "%.*s%s.ini", (int)(name - filePath), filePath, config.gameShortTitle);
    if (!fileExists(gameFile))
        return;
    log_info("Settings of this game: %s", gameFile);
    snprintf(filePath, PATH_MAX, "%s", gameFile);
}

const char *getConfigFilePath(void)
{
    return configFileUsed;
}

int initConfig(const char *configFilePath)
{
    setDefaultValues(&config);

    config.crc32 = partialElfCrc;
    if (detectGame(config.crc32) != 0)
    {
        log_error("Unsure what game with CRC 0x%X is. Please submit this new game to the GitHub repository: "
                 "https://github.com/lindbergh-loader/lindbergh-loader/issues/"
                 "new?title=Please+add+new+game+0x%X&body=I+tried+to+launch+the+following+game:\n",
                 config.crc32, config.crc32);
    }

    config.inputMode = 0;

    char filePath[PATH_MAX];
    if (configFilePath != NULL && configFilePath[0] != '\0')
    {
        strncpy(filePath, configFilePath, PATH_MAX - 1);
    }
    else
    {
        if (fileExists(CONFIG_PATH))
        {
            strncpy(filePath, CONFIG_PATH, PATH_MAX - 1);
        }
#ifdef _LINUX_
        else
        {
            // Fallback to legacy path to preserve backward compatibility with the old loader
            strncpy(filePath, "lindbergh.ini", PATH_MAX - 1);
        }
#endif
    }
    filePath[PATH_MAX - 1] = '\0';
    useGameConfigFile(filePath);
    snprintf(configFileUsed, sizeof(configFileUsed), "%s", filePath);
    IniConfig *ini = iniLoad(filePath);

    if (ini == NULL && overrideCount > 0)
        ini = calloc(1, sizeof(IniConfig)); // the defaults, with the command line's settings over them
    if (ini == NULL)
    {
        log_warn("Cannot open or parse %s, using default values.", filePath);
        gameOptionsRemember();
        return 1;
    }

    applyOverrides(ini);
    applyIniConfig(&config, ini);
    applyGameOptions(ini);

    if (config.customCursorEnabled && (strcmp(config.customCursor, "") != 0 || strcmp(config.touchCursor, "") != 0))
        config.hideCursor = 0;

    iniFree(ini);

    return 0;
}

// The settings alone, for the game picker: no game to detect.
int loadConfigFile(const char *configFilePath)
{
    setDefaultValues(&config);
    IniConfig *ini = iniLoad(configFilePath);
    if (ini == NULL)
        return 1;
    applyIniConfig(&config, ini);
    iniFree(ini);
    return 0;
}

EmulatorConfig *getConfig()
{
    return &config;
}
