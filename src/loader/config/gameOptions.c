// The options of each game, for the F10 window's "Game options" page. Each one is a field of the loader's settings,
// already read from its general key when there is one (EMULATE_DRIVEBOARD of [Emulation], [GameSpecific]...): the
// game's [Game.<short name>] section is read over it (config.c), so that the page's choice is this game's alone.

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "gameOptions.h"

static bool sameText(const char *a, const char *b)
{
    for (; *a && *b; a++, b++)
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b))
            return false;
    return *a == *b;
}

static uint32_t crc(void)
{
    return getConfig()->crc32;
}

static GameGroup group(void)
{
    return getConfig()->gameGroup;
}

static bool outrun(void)
{
    return group() == GROUP_OUTRUN || group() == GROUP_OUTRUN_TEST;
}

static bool hummer(void)
{
    return group() == GROUP_HUMMER;
}

static bool initialD(void)
{
    return group() == GROUP_ID4_EXP || group() == GROUP_ID4_JAP || group() == GROUP_ID5;
}

static bool initialD5Chinese(void)
{
    return crc() == INITIALD_5_EXP20_SBTS || crc() == INITIALD_5_EXP20_SBTS_REVA;
}

// The driving games whose drive board the loader can emulate
static bool driveBoard(void)
{
    return outrun() || hummer() || initialD() || crc() == SEGA_RACE_TV_SBPF || crc() == R_TUNED_SBQW;
}

static bool hw210CardReader(void)
{
    return group() == GROUP_VT3 || group() == GROUP_VT3_TEST || group() == GROUP_VF5 || crc() == R_TUNED_SBQW;
}

static bool letsGoJungle(void)
{
    return group() == GROUP_LGJ;
}

static bool primevalHunt(void)
{
    return crc() == PRIMEVAL_HUNT_SBPP;
}

static bool mahjong(void)
{
    return crc() == MJ4_SBPN_REVG || crc() == MJ4_EVO_SBTA || crc() == QUIZ_AXA_SBMS || crc() == QUIZ_AXA_SBUR_LIVE;
}

static bool rambo(void)
{
    return group() == GROUP_RAMBO;
}

static bool harley(void)
{
    return crc() == HARLEY_DAVIDSON_SBRG;
}

static bool ghostSquad(void)
{
    return crc() == GHOST_SQUAD_EVOLUTION_SBNJ;
}

static bool maximumTune3(void)
{
    return crc() == WANGAN_MIDNIGHT_MT3;
}

static const char *const phModes[] = {"0", "1", "2", "3", "4", NULL};
static const char *const phModeLabels[] = {"Default (no scaling)", "No touch screen", "Side by side",
                                           "Touch screen on the right", "Touch screen below", NULL};

#define FIELD(name) offsetof(EmulatorConfig, name)

const GameOption gameOptionTable[] = {
    {"NO_CARD_READER", "Play without the card reader",
     "As when its test menu's CARD R/W is OFF: no card check at start, games without a card. The card reader-printer is "
     "not emulated yet: unticked, the test menu decides, and CARD R/W ON stops the game at error E51.",
     GAME_OPTION_SWITCH, FIELD(noCardReader), false, NULL, NULL, 0, 1, true, maximumTune3},
    {"EMULATE_DRIVEBOARD", "Emulate the drive board",
     "The force feedback board on its serial port, emulated by the loader for the wheel. Unticked: the serial port is "
     "passed to a real board (Initial D: the card reader shares the serial ports).",
     GAME_OPTION_SWITCH, FIELD(emulateDriveboard), false, NULL, NULL, 0, 1, true, driveBoard},
    {"SKIP_OUTRUN_CABINET_CHECK", "Skip the cabinet checks",
     "The drive board and tower checks of the start skipped.", GAME_OPTION_SWITCH, FIELD(skipOutrunCabinetCheck), false,
     NULL, NULL, 0, 1, true, outrun},
    {"HUMMER_FLICKER_FIX", "Fix the flickering", "A shader patch for the flickering some graphics cards show.",
     GAME_OPTION_SWITCH, FIELD(hummerFlickerFix), false, NULL, NULL, 0, 1, true, hummer},
    {"EMULATE_ID_CARDREADER", "Emulate the card reader", "The players' cards kept in files (Cards section).",
     GAME_OPTION_SWITCH, FIELD(emulateIDCardReader), false, NULL, NULL, 0, 1, true, initialD},
    {"ID_STEERING_REDUCTION_PERCENTAGE", "Steering travel used",
     "In % of the wheel's travel: a smaller turn of the wheel for a full turn in the game. 0: the whole travel.",
     GAME_OPTION_RANGE, FIELD(idSteeringPercentageReduction), true, NULL, NULL, 0, 100, false, initialD},
    {"ID5_CHINESE_LANGUAGE", "Chinese language", "The game in Chinese (DVP-0084 and DVP-0084A).", GAME_OPTION_SWITCH,
     FIELD(id5ChineseLanguage), false, NULL, NULL, 0, 1, true, initialD5Chinese},
    {"EMULATE_HW210_CARDREADER", "Emulate the card reader", "The HW210 IC card reader, its cards kept in files.",
     GAME_OPTION_SWITCH, FIELD(emulateHW210CardReader), false, NULL, NULL, 0, 1, true, hw210CardReader},
    {"LGJ_RENDER_WITH_MESA", "Render with the Mesa patches", "The shaders patched as for Mesa's drivers.",
     GAME_OPTION_SWITCH, FIELD(lgjRenderWithMesa), false, NULL, NULL, 0, 1, true, letsGoJungle},
    {"PRIMEVAL_HUNT_SCREEN_MODE", "Screens", "Where the touch screen goes beside the main one.", GAME_OPTION_CHOICE,
     FIELD(phScreenMode), false, phModes, phModeLabels, 0, 4, true, primevalHunt},
    {"PRIMEVAL_HUNT_TEST_SCREEN_SINGLE", "Test menu on one screen", "The test menu on the main screen alone.",
     GAME_OPTION_SWITCH, FIELD(phTestScreenSingle), false, NULL, NULL, 0, 1, true, primevalHunt},
    {"PRIMEVAL_HUNT_USE_ZINK", "Render with Zink", "A special OpenGL library that fixes the graphic glitches.",
     GAME_OPTION_SWITCH, FIELD(phUseZink), false, NULL, NULL, 0, 1, true, primevalHunt},
    {"MJ4_ENABLED_ALL_THE_TIME", "Playable at any time", "The game also runs at the hours its servers refused.",
     GAME_OPTION_SWITCH, FIELD(mj4EnabledAtT), false, NULL, NULL, 0, 1, true, mahjong},
    {"RAMBO_GUNS_SWITCH", "Other gun display", "The cabinet's switch that changes how the guns are shown.",
     GAME_OPTION_SWITCH, FIELD(ramboGunsSwitch), false, NULL, NULL, 0, 1, true, rambo},
    {"HARLEY_ANALOG_REMAP", "Swap handlebar and throttle", "For JVS passthrough: the two analogue channels swapped.",
     GAME_OPTION_SWITCH, FIELD(harleyAnalogRemap), false, NULL, NULL, 0, 1, true, harley},
    {"GSEVO_CROSSHAIR_ALWAYS_ON", "Crosshairs always shown", "", GAME_OPTION_SWITCH, FIELD(gsevoCrosshairAlwaysOn), false,
     NULL, NULL, 0, 1, true, ghostSquad},
    {"GSEVO_CROSSHAIR_ALWAYS_OFF", "Crosshairs always hidden", "", GAME_OPTION_SWITCH, FIELD(gsevoCrosshairAlwaysOff),
     false, NULL, NULL, 0, 1, true, ghostSquad},
};

const int gameOptionCount = (int)(sizeof(gameOptionTable) / sizeof(gameOptionTable[0]));

static int launchValues[sizeof(gameOptionTable) / sizeof(gameOptionTable[0])];

const char *gameOptionSection(void)
{
    static char section[96];
    const char *name = getConfig()->gameShortTitle;
    if (!name || !name[0])
        return "";
    snprintf(section, sizeof(section), "Game.%s", name);
    return section;
}

int gameOptionValue(const GameOption *option)
{
    char *field = (char *)getConfig() + option->field;
    return option->isFloat ? (int)(*(float *)field + 0.5f) : *(int *)field;
}

void gameOptionSetValue(const GameOption *option, int value)
{
    char *field = (char *)getConfig() + option->field;
    if (option->isFloat)
        *(float *)field = (float)value;
    else
        *(int *)field = value;
}

void gameOptionFormat(const GameOption *option, char *buffer, size_t size)
{
    int value = gameOptionValue(option);
    if (option->kind == GAME_OPTION_SWITCH)
        snprintf(buffer, size, "%s", value ? "true" : "false");
    else if (option->kind == GAME_OPTION_CHOICE && value >= 0 && value <= option->max && option->values[value])
        snprintf(buffer, size, "%s", option->values[value]);
    else
        snprintf(buffer, size, "%d", value);
}

bool gameOptionParse(const GameOption *option, const char *text)
{
    if (option->kind == GAME_OPTION_SWITCH)
    {
        if (sameText(text, "true") || strcmp(text, "1") == 0)
            gameOptionSetValue(option, 1);
        else if (sameText(text, "false") || strcmp(text, "0") == 0)
            gameOptionSetValue(option, 0);
        else
            return false;
        return true;
    }
    if (option->kind == GAME_OPTION_CHOICE)
    {
        for (int i = 0; option->values[i]; i++)
            if (sameText(text, option->values[i]))
            {
                gameOptionSetValue(option, i);
                return true;
            }
        return false;
    }
    char *end;
    long value = strtol(text, &end, 10);
    if (end == text)
        return false;
    gameOptionSetValue(option, (int)(value < option->min ? option->min : value > option->max ? option->max : value));
    return true;
}

void gameOptionsRemember(void)
{
    for (int i = 0; i < gameOptionCount; i++)
        launchValues[i] = gameOptionValue(&gameOptionTable[i]);
}

int gameOptionLaunchValue(const GameOption *option)
{
    return launchValues[option - gameOptionTable];
}
