#include "configIni.h"
#include "config.h"
#include "../log/log.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// The new file's text, built in memory before it is written
typedef struct
{
    char *text;
    size_t length;
    size_t size;
} IniText;

static void iniOut(IniText *out, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    int needed = vsnprintf(NULL, 0, format, args);
    va_end(args);
    if (needed < 0)
        return;
    if (out->length + needed + 1 > out->size)
    {
        size_t size = out->size ? out->size : 16384;
        while (size < out->length + needed + 1)
            size *= 2;
        char *text = realloc(out->text, size);
        if (!text)
            return;
        out->text = text;
        out->size = size;
    }
    va_start(args, format);
    vsnprintf(out->text + out->length, out->size - out->length, format, args);
    va_end(args);
    out->length += needed;
}

// Writes the text, each setting's "KEY = value" line preceded by "# Default: value", the value a new file has: the
// comment stays when the value is changed
static void writeWithDefaults(FILE *file, const char *text)
{
    const char *line = text;
    while (*line)
    {
        const char *end = strchr(line, '\n');
        const size_t length = end ? (size_t)(end - line) : strlen(line);
        const char *equals = memchr(line, '=', length);
        if (equals && line[0] != '#' && line[0] != ';' && line[0] != '[')
        {
            const char *value = equals + 1;
            while (value < line + length && *value == ' ')
                value++;
            const int valueLength = (int)(line + length - value);
            if (valueLength == 0 || (valueLength == 2 && value[0] == '"' && value[1] == '"'))
                fprintf(file, "# Default: (empty)\n");
            else
                fprintf(file, "# Default: %.*s\n", valueLength, value);
        }
        fwrite(line, 1, length, file);
        fputc('\n', file);
        if (!end)
            break;
        line = end + 1;
    }
}

int createDefaultIni(const char *filePath)
{
    printf("Generating default %s\n", filePath);

    FILE *file = fopen(filePath, "w");
    if (!file)
    {
        log_error("Could not create default INI file at %s", filePath);
        return -1;
    }

    IniText out = {0};
    EmulatorConfig defaults = {0};
    setDefaultValues(&defaults);

    iniOut(&out, "# Linux Loader Configuration File\n");
    iniOut(&out, "# By the Linux Loader Development Team 2026\n\n");

    // [Display]
    iniOut(&out, "[Display]\n");
    iniOut(&out, "# Set the width resolution here\nWIDTH = AUTO\n\n");
    iniOut(&out, "# Set the height resolution here\nHEIGHT = AUTO\n\n");
    iniOut(&out, "# Boost render resolution in HOD4/2Spicy/Harley/Rambo/HOD-EX/ID4/ID5 and LGJ\n");
    iniOut(&out, "BOOST_RENDER_RES = %s\n\n", defaults.boostRenderRes ? "true" : "false");
    iniOut(&out, "# Set to true for full screen\nFULLSCREEN = %s\n\n", defaults.fullscreen ? "true" : "false");
    iniOut(&out, "# Set the display to use (0 = first display, 1 = second display...)\nDISPLAY_INDEX = %d\n\n", defaults.displayIndex);
    iniOut(&out, "# Shape of the picture: ORIGINAL (the game's render resolution); 4:3 or 16:9 (that frame, the picture whole\n");
    iniOut(&out, "# inside at its own shape, black bars where the shapes differ; 4:3_LETTERBOX is 4:3); 4:3_STRETCH or 16:9_STRETCH\n");
    iniOut(&out, "# (the picture fills the frame, deformed); 4:3_CROP or 16:9_CROP (the picture fills the frame, cut on the\n");
    iniOut(&out, "# sides or above and below)\nASPECT_RATIO = ORIGINAL\n\n");
    iniOut(&out, "# The part a cropped aspect ratio keeps: -100 (left or top) to 100 (right or bottom), 0 the middle\n");
    iniOut(&out, "CROP_POSITION = 0\n\n");
    iniOut(&out, "# Cropped aspect ratios: true bends the parts they would cut onto the frame's edges, as on a cylinder seen from\n");
    iniOut(&out, "# the front, so the whole picture stays visible\nCROP_BARREL = false\n\n");
    iniOut(&out, "# With CROP_BARREL: the middle shown undistorted, in %% of the frame\nBARREL_CENTER = 60\n\n");
    iniOut(&out, "# With CROP_BARREL: the cylinder's radius in %% of the frame (small bends hard at the edge, large compresses\n");
    iniOut(&out, "# evenly)\nBARREL_RADIUS = 50\n\n");
    iniOut(&out, "# WINDOW (window at the aspect ratio, square pixels), WINDOW_STRETCH (the picture fills the window),\n");
    iniOut(&out, "# FULLSCREEN (full screen at the aspect ratio, the screen set to the game's resolution), FULLSCREEN_STRETCH\n");
    iniOut(&out, "# (the picture fills the screen), BORDERLESS (a borderless window over the screen at the desktop's resolution,\n");
    iniOut(&out, "# the aspect ratio) or BORDERLESS_STRETCH (the picture fills it).\n");
    iniOut(&out, "# Takes over FULLSCREEN when set.\nDISPLAY_MODE = %s\n\n", defaults.fullscreen ? "FULLSCREEN" : "WINDOW");
    iniOut(&out, "# Set to true if you\'d like to add a border for optical light gun tracking\n");
    iniOut(&out, "BORDER_ENABLED = %s\n\n", defaults.borderEnabled ? "true" : "false");
    iniOut(&out, "# Set the thickness of the white border as a percentage of the width of the screen\n");
    iniOut(&out, "WHITE_BORDER_PERCENTAGE = %.1f\n\n", defaults.whiteBorderPercentage * 100.0f);
    iniOut(&out, "# Set the thickness of the black border which sits around the\n");
    iniOut(&out, "# white border as a percentage of the width of the screen\n");
    iniOut(&out, "BLACK_BORDER_PERCENTAGE = %.1f\n\n", defaults.blackBorderPercentage * 100.0f);
    iniOut(&out, "# Set to keep the original aspect ratio in the games that support it\n#  (Sega Race TV, Primeval Hunt, LGJ-SP, GSEvo, After Burner Climax)\n");
    iniOut(&out, "KEEP_ASPECT_RATIO = %s\n\n", defaults.keepAspectRatio ? "true" : "false");
    iniOut(&out, "# Filter used to stretch the game to the window or screen (LINEAR/NEAREST)\n");
    iniOut(&out, "#  NEAREST keeps the pixels sharp, but looks uneven unless the scale is a whole number\n");
    iniOut(&out, "SCALING_FILTER = %s\n\n", defaults.scalingFilterNearest ? "NEAREST" : "LINEAR");
    iniOut(&out, "# Set to true to enable the mouse pointer/Cursor\nHIDE_CURSOR = %s\n\n", defaults.hideCursor ? "true" : "false");
    iniOut(&out, "# Set to false if you don't want to limit the FPS\n");
    iniOut(&out, "FPS_LIMITER_ENABLED = %s\n\n", defaults.fpsLimiter ? "true" : "false");
    iniOut(&out, "# Set the target FPS (will only work if FPS_LIMITER_ENABLED = 1)\nFPS_TARGET = %.1f\n\n", defaults.fpsTarget);
    iniOut(&out, "# Set to true to enable the FPS overlay\n");
    iniOut(&out, "FPS_OVERLAY_ENABLED = %s\n\n", defaults.fpsOverlayEnabled ? "true" : "false");
    iniOut(&out, "# Set the position of the FPS overlay (0: TOP_LEFT, 1: TOP_RIGHT, 2: BOTTOM_LEFT, 3: BOTTOM_RIGHT)\n");
    iniOut(&out, "FPS_OVERLAY_POSITION = %d\n\n", defaults.fpsOverlayPosition);

    // [Input]
    iniOut(&out, "[Input]\n");
    iniOut(&out, "# Sets the Input Mode (1: SDL, 2: EVDEV\n");
    iniOut(&out, "INPUT_MODE = %d\n\n", defaults.inputMode);
    iniOut(&out, "# The settings menu's controls page numbers a joystick's buttons from 1, as Windows and vJoy do (true),\n");
    iniOut(&out, "# or from 0, as SDL and controls.ini do (false). controls.ini keeps SDL's numbers either way\n");
    iniOut(&out, "BUTTONS_FROM_ONE = %s\n\n", defaults.buttonsFromOne ? "true" : "false");

    // [Emulation]
    iniOut(&out, "[Emulation]\n");
    iniOut(&out, "# Set the Region (JP/US/EX)\nREGION = EX\n\n");
    iniOut(&out, "# Set to true for Free Play, none to leave as default\nFREEPLAY = none\n\n");
    iniOut(&out, "# Set to true to emulate JVS and use the keyboard/mouse for controls.\n");
    iniOut(&out, "# If this is set to false, the loader will route the traffic to the serial device\n");
    iniOut(&out, "# defined in JVS_PATH if it has been defined.\nEMULATE_JVS = %s\n\n", defaults.emulateJVS ? "true" : "false");
    iniOut(&out, "# Set to true to emulate the rideboard used in the SPECIAL games\n");
    iniOut(&out, "# If this is set to false, then the emulator will route the traffic to one of the serial ports\n");
    iniOut(&out, "EMULATE_RIDEBOARD = AUTO\n\n");
    iniOut(&out, "# Set to true to emulate the driveboard used in driving games\n");
    iniOut(&out, "# If this is set to false, then the emulator will route the traffic to one of the serial ports\n");
    iniOut(&out, "EMULATE_DRIVEBOARD = AUTO\n\n");
    iniOut(&out, "# Set to true to emulate the motion board from Outrun 2 SP SDX\n");
    iniOut(&out, "# If this is set to false, then the emulator will route the traffic to one of the serial ports\n");
    iniOut(&out, "EMULATE_MOTIONBOARD = AUTO\n\n");
    iniOut(&out, "# Set to true to enable card reader emulation in Virtua Tennis 3 or R-Tuned\nEMULATE_HW210_CARDREADER = AUTO\n\n");
    iniOut(&out, "# Set to true to enable card reader emulation in ID4 and ID5 file \nEMULATE_ID_CARD_READER = AUTO\n\n");
    iniOut(&out, "# Set to true to enable touchscreen emulation  with the mouse\nEMULATE_TOUCHSCREEN = AUTO\n\n");

    // [Cards]
    iniOut(&out, "[Cards]\n");
    iniOut(&out, "# Set to false to use a button to insert a card manually in ID4 or ID5.\n");
    iniOut(&out, "# Or true to make the loader auto load\nID_CARDFILE_AUTOLOAD = %s\n\n", defaults.idCardFileAutoload ? "true" : "false");
    iniOut(&out, "# Card File for reader 1 in VT3 or R-Tuned; Maximum Tune 3's card (the card emulators' .RAW text, the\n");
    iniOut(&out, "# CardInsert key offers it to the card reader-printer)\nCARDFILE_01 = \"%s\"\n\n", defaults.cardFile1);
    iniOut(&out, "# Card File for reader 2 in VT3 or R-Tuned\nCARDFILE_02 = \"%s\"\n\n", defaults.cardFile2);
    iniOut(&out, "# Folder for ID Card files\nID_CARDFOLDER = \"%s\"\n\n", defaults.idCardFolder);

    // [Paths]
    iniOut(&out, "[Paths]\n");
    iniOut(&out, "# Define the path to pass the JVS packets to if JVS is not being emulated\nJVS_PATH = \"%s\"\n\n", defaults.jvsPath);
    iniOut(&out, "# Define the path to pass the first serial port to\nSERIAL_1_PATH = \"%s\"\n\n", defaults.serial1Path);
    iniOut(&out, "# Define the path to pass the second serial port to\nSERIAL_2_PATH = \"%s\"\n\n", defaults.serial2Path);
    iniOut(&out, "# Define the path to the sram.bin file\nSRAM_PATH = \"%s\"\n\n", defaults.sramPath);
    iniOut(&out, "# Define the path to the eeprom.bin file\nEEPROM_PATH = \"%s\"\n\n", defaults.eepromPath);
    iniOut(&out, "# Windows: the folder everything is written to (or --write-dir); the loader's and the game's folders are then\n");
    iniOut(&out, "# only read. Its root holds linuxloader.ini and controls.ini (copied there the first time), WRITE_DIR\\<game>\n");
    iniOut(&out, "# the NVRAM, saves, logs, temporary files and compiled shaders; for a write filter (UWF) or games on a share.\n");
    iniOut(&out, "# Once it is set, the linuxloader.ini and controls.ini read are the copies in that folder: change those, not\n");
    iniOut(&out, "# the ones beside the loader.\n");
    iniOut(&out, "# For Example: C:\\Racecab\\Saves\\linuxloader_single\nWRITE_DIR = \"\"\n\n");
    iniOut(&out, "# Windows, with WRITE_DIR (or --seed-dir): SEED_DIR\\<game> holds the files WRITE_DIR\\<game> starts with, in\n");
    iniOut(&out, "# place of the game folder's (a link cabinet's NVRAM, its LINK ID set), copied there when the game starts; a\n");
    iniOut(&out, "# file already in WRITE_DIR\\<game> is never replaced. Relative to the loader's folder, or a full path.\n");
    iniOut(&out, "# For Example: saves\nSEED_DIR = \"\"\n\n");
    iniOut(&out, "# If set, libCG.so needed for 2Spicy, Harley, Rambo and HOD-Ex shader recompilation\n");
    iniOut(&out, "# will be loaded from the specified location. (include the name of file in the location ");
    iniOut(&out, "For Example: /my/file/location/myLibCg.so)\nLIBCG_PATH = \"%s\"\n\n", defaults.libCgPath);

    // [Graphics]
    iniOut(&out, "[Graphics]\n");
    iniOut(&out, "# Set to true if you experience flickering in hummer\n");
    iniOut(&out, "HUMMER_FLICKER_FIX = %s\n\n", defaults.hummerFlickerFix ? "true" : "false");
    iniOut(&out, "# Set to true if you want to render LGJ using the mesa patches\n");
    iniOut(&out, "LGJ_RENDER_WITH_MESA = %s\n\n", defaults.lgjRenderWithMesa ? "true" : "false");
    iniOut(&out, "# Disable to use the original fonts instead of the built in font\n");
    iniOut(&out, "DISABLE_BUILTIN_FONT = %s\n\n", defaults.disableBuiltinFont ? "true" : "false");
    iniOut(&out, "# Disable to use the original logos instead of the built in logos\n");
    iniOut(&out, "DISABLE_BUILTIN_LOGOS = %s\n\n", defaults.disableBuiltinLogos ? "true" : "false");

    // [Cursor]
    iniOut(&out, "[Cursor]\n");
    iniOut(&out, "# If true, a custom mouse cursor will be used loaded from a png file set in CUSTOM_CURSOR\n");
    iniOut(&out, "# Ovewrites HIDE_CURSOR\n");
    iniOut(&out, "CUSTOM_CURSOR_ENABLED = %s\n\n", defaults.customCursorEnabled ? "true" : "false");
    iniOut(&out, "# Set the custom mouse pointer from a PNG file (Usefull for shooting games)\n");
    iniOut(&out, "CUSTOM_CURSOR = \"%s\"\n\n", defaults.customCursor);
    iniOut(&out, "# Set the width of the custom cursor\nCUSTOM_CURSOR_WIDTH = %d\n\n", defaults.customCursorWidth);
    iniOut(&out, "# Set the height of the custom cursor\nCUSTOM_CURSOR_HEIGHT = %d\n\n", defaults.customCursorHeight);
    iniOut(&out, "# Set a custom cursor for the touch screen in Primeval Hunt, MJ4 Games and AxA Games\n");
    iniOut(&out, "TOUCH_CURSOR = \"%s\"\n\n", defaults.touchCursor);
    iniOut(&out, "# Set the width of the custom cursor\nTOUCH_CURSOR_WIDTH = %d\n\n", defaults.touchCursorWidth);
    iniOut(&out, "# Set the height of the custom cursor\nTOUCH_CURSOR_HEIGHT = %d\n\n", defaults.touchCursorHeight);

    // [GameSpecific]
    iniOut(&out, "[GameSpecific]\n");
    iniOut(&out, "# Set Primeval Hunt\n# Mode 0: Default (no scaling)\n# Mode 1: No Touch screen\n# Mode 2: Side By Side\n");
    iniOut(&out, "# Mode 3: 3ds mode 1 (Touch screen to the right)\n# Mode 4: 3ds mode 2 (Touch screen to the bottom)\n");
    iniOut(&out, "PRIMEVAL_HUNT_SCREEN_MODE = %d\n\n", defaults.phScreenMode);
    iniOut(&out, "# Set Primeval Hunt Test mode screen to single screen\n");
    iniOut(&out, "PRIMEVAL_HUNT_TEST_SCREEN_SINGLE = %s\n\n", defaults.phTestScreenSingle ? "true" : "false");
    iniOut(&out, "# Set to true to bypass cabinet checks including drive board and tower in Outrun 2 SP SDX, for a cabinet\n"
                 "# without a drive board (no force feedback then); ignored with EMULATE_DRIVEBOARD, which passes them\n");
    iniOut(&out, "SKIP_OUTRUN_CABINET_CHECK = %s\n\n", defaults.skipOutrunCabinetCheck ? "true" : "false");
    iniOut(&out, "# Hacky way to make MJ4 and AxA work at prohibited times\nMJ4_ENABLED_ALL_THE_TIME = %s\n\n",
            defaults.mj4EnabledAtT ? "true" : "false");
    iniOut(&out, "# House of the dead 4 speed fix, set the frequency of your CPU in Ghz\nCPU_FREQ_GHZ = %.1f\n\n", defaults.cpuFreqGhz);
    iniOut(&out, "# Set to true if you want to chnge the way the guns are show in Rambo\n");
    iniOut(&out, "RAMBO_GUNS_SWITCH = false\n\n");
    iniOut(&out, "# Set to true if you want to enable analog swapping of the steering wheel and Gas for Harley for JVS Passthrough\n");
    iniOut(&out, "HARLEY_ANALOG_REMAP = %s\n\n", defaults.harleyAnalogRemap ? "true" : "false");
    iniOut(&out, "# Set to true to set the language in Chinese for ID5 DVP-0084 and DVP-0084A\n");
    iniOut(&out, "ID5_CHINESE_LANGUAGE = %s\n\n", defaults.id5ChineseLanguage ? "true" : "false");
    iniOut(&out, "# Set the percentage of the steering wheel travel reduction\n");
    iniOut(&out, "ID_STEERING_REDUCTION_PERCENTAGE = %.1f\n\n", defaults.idSteeringPercentageReduction);

    iniOut(&out, "[CrossHairs]\n");
    iniOut(&out, "# Set to true to enable Crosshairs even when using GunLights\n");
    iniOut(&out, "ENABLE_CROSSHAIRS = %s\n\n", defaults.enableCrosshairs ? "true" : "false");
    iniOut(&out, "# Set the Crosshair image from a PNG file for Player 1\n");
    iniOut(&out, "P1_CROSSHAIR_PATH = \"%s\"\n\n", defaults.p1CrossHairPath);
    iniOut(&out, "# Set the Crosshair image from a PNG file for Player 2\n");
    iniOut(&out, "P2_CROSSHAIR_PATH = \"%s\"\n\n", defaults.p2CrossHairPath);
    iniOut(&out, "# Set the width of the Crosshair image\n");
    iniOut(&out, "CUSTOM_CROSSHAIRS_WIDTH = %d\n\n", defaults.customCrossHairWidth);
    iniOut(&out, "# Set the height of the Crosshair image\n");
    iniOut(&out, "CUSTOM_CROSSHAIRS_HEIGHT = %d\n\n", defaults.customCrossHairHeight);
    iniOut(&out, "# Set to true to always enable Crosshairs in Ghost Squad Evolution\n");
    iniOut(&out, "GSEVO_CROSSHAIR_ALWAYS_ON = %s\n\n", defaults.gsevoCrosshairAlwaysOn ? "true" : "false");
    iniOut(&out, "# Set to true to always disable Crosshairs in Ghost Squad Evolution\n");
    iniOut(&out, "GSEVO_CROSSHAIR_ALWAYS_OFF = %s\n\n", defaults.gsevoCrosshairAlwaysOff ? "true" : "false");

    // [System]
    iniOut(&out, "[System]\n");
    iniOut(&out, "# Set to true to see debug messages in the console\n");
    iniOut(&out, "DEBUG_MSGS = %s\n\n", defaults.showDebugMessages ? "true" : "false");
    iniOut(&out, "# Set the colour of the lindbergh (YELLOW, RED, BLUE, SILVER, REDEX)\nLINDBERGH_COLOUR = YELLOW\n\n");

    // [Outputs]
    iniOut(&out, "[Outputs]\n");
    iniOut(&out, "# Set to true to send lamps and drive board data over the network, MAME style (BackForceFeeder, MAMEHooker...)\n");
    iniOut(&out, "OUTPUTS_NETWORK = %s\n\n", defaults.outputsNetwork ? "true" : "false");
    iniOut(&out, "# TCP port the outputs server listens on\n");
    iniOut(&out, "OUTPUTS_NETWORK_PORT = %d\n\n", defaults.outputsNetworkPort);
    iniOut(&out, "# UDP port on 127.0.0.1 that gets a start announcement (0 to disable)\n");
    iniOut(&out, "OUTPUTS_NETWORK_UDP_PORT = %d\n\n", defaults.outputsNetworkUdpPort);

    // [ForceFeedback]
    iniOut(&out, "[ForceFeedback]\n");
    iniOut(&out, "# Strength of the drive board forces on a force feedback wheel, in percent of the board's full scale\n");
    iniOut(&out, "FFB_STRENGTH = %d\n", defaults.ffbStrength);
    iniOut(&out, "# Set to true if the wheel turns the wrong way: the torque then pushes the other way\n");
    iniOut(&out, "FFB_INVERT = %s\n", defaults.ffbInvert ? "true" : "false");
    iniOut(&out, "# The device the forces go to, wherever the steering is read: AUTO for the first controller's, or a\n");
    iniOut(&out, "# force feedback device's name as Tab > Controls lists it (case and surrounding spaces ignored)\n");
    iniOut(&out, "FFB_DEVICE = %s\n\n", defaults.ffbDevice);

    // [Launcher]
    iniOut(&out, "[Launcher]\n");
    iniOut(&out, "# Folder the game picker lists when linuxloader starts without a game (empty: games next to the program)\n");
    iniOut(&out, "GAMES_DIR = %s\n\n", defaults.gamesDir);
    iniOut(&out, "# Language of the picker and of the Tab settings: auto (the system's), en, or a .po file's name from the lang\n");
    iniOut(&out, "# folder next to the program (fr for lang/fr.po)\n");
    iniOut(&out, "LANGUAGE = %s\n\n", defaults.language);
    iniOut(&out, "# Set to false on a public cabinet: Tab no longer opens the settings over a running game (which pauses it)\n");
    iniOut(&out, "SETTINGS_MENU = %s\n\n", defaults.settingsMenu ? "true" : "false");

    // [Network]
    iniOut(&out, "[Network]\n");
    iniOut(&out, "# If true, the loader will apply the following network patches depending on the game\n");
    iniOut(&out, "ENABLE_NETWORK_PATCHES = %s\n\n", defaults.enableNetworkPatches ? "true" : "false");
    iniOut(&out, "# Sets the name of the Network Interface Card\nNIC_NAME = \"enp0s1\"\n\n");
    iniOut(&out, "# Initial D 4 Export and Initial D 5 link: the address of the cabinet set to seat 1 (test menu, GAME\n");
    iniOut(&out, "# ASSIGNMENTS, SEAT NUMBER), the same on both cabinets\nID_IP_SEAT_1 = \"192.168.1.2\"\n\n");
    iniOut(&out, "# Initial D 4 Export and Initial D 5 link: the address of the cabinet set to seat 2, the same on both cabinets\n");
    iniOut(&out, "ID_IP_SEAT_2 = \"192.168.1.3\"\n\n");
    iniOut(&out, "# OutRun 2 link: this cabinet's address (one of this PC's); the cabinets' addresses follow each other in the\n");
    iniOut(&out, "# order of their LINK ID (test menu, GAME ASSIGNMENTS: LINK ID, TOTAL MACHINE)\n");
    iniOut(&out, "OR2_IPADDRESS = \"192.168.1.2\"\n\n");
    iniOut(&out, "# OutRun 2 link: the netmask of the cabinets' network\nOR2_NETMASK = \"255.255.255.0\"\n\n");
    iniOut(&out, "# Harley, Hummer and R-Tuned link: the address of the cabinet with LINK ID 1; IP_CAB1 to IP_CAB4 are the same on\n");
    iniOut(&out, "# every cabinet (Hummer takes the one of this PC as its own)\nIP_CAB1 = \"192.168.1.2\"\n\n");
    iniOut(&out, "# Harley, Hummer and R-Tuned link: the address of the cabinet with LINK ID 2\nIP_CAB2 = \"192.168.1.3\"\n\n");
    iniOut(&out, "# Harley, Hummer and R-Tuned link: the address of the cabinet with LINK ID 3\nIP_CAB3 = \"192.168.1.4\"\n\n");
    iniOut(&out, "# Harley, Hummer and R-Tuned link: the address of the cabinet with LINK ID 4\nIP_CAB4 = \"192.168.1.5\"\n\n");
    iniOut(&out, "# 2Spicy link: the address of cabinet 1\n2SPICY_IP_CAB1 = \"192.168.1.2\"\n\n");
    iniOut(&out, "# 2Spicy link: the address of cabinet 2\n2SPICY_IP_CAB2 = \"192.168.1.3\"\n\n");
    iniOut(&out, "# SEGA Race TV's IP address: one of this PC's for a link, empty for a cabinet alone (127.0.0.1; an address\n");
    iniOut(&out, "# that is not the PC's is replaced by it)\nSRTV_IPADDRESS = \"\"\n\n");
    iniOut(&out, "# The link on one PC, one copy of linuxloader per cabinet (each its own linuxloader.ini and write folder):\n");
    iniOut(&out, "# this copy's address, 127.0.0.2 for the first cabinet, 127.0.0.3 for the second... It is also OR2_IPADDRESS and\n");
    iniOut(&out, "# SRTV_IPADDRESS; IP_CAB1 to 4 and ID_IP_SEAT_1 and 2 list the copies' addresses, the same in every copy. Empty:\n");
    iniOut(&out, "# a cabinet on the network, or alone\nLOCAL_LINK_ADDRESS = \"\"\n\n");

    // [EVDEV]
    iniOut(&out, "[EVDEV]\n");
    iniOut(&out, "# EVDEV MODE (Input Mode 2)\n# To find the value pairs for these run ./linuxloader --list-controllers\n\n");
    iniOut(&out, "#TEST_BUTTON = \"AT_TRANSLATED_SET_2_KEYBOARD_KEY_T\"\n");
    iniOut(&out, "#EXIT_GAME = \"KEY_ESC, BTN_START+BTN_SELECT\"\n\n");
    iniOut(&out, "#PLAYER_1_COIN = \"AT_TRANSLATED_SET_2_KEYBOARD_KEY_5\"\n");
    iniOut(&out, "#PLAYER_1_BUTTON_START = \"AT_TRANSLATED_SET_2_KEYBOARD_KEY_1\n");
    iniOut(&out, "#PLAYER_1_BUTTON_SERVICE = \"AT_TRANSLATED_SET_2_KEYBOARD_KEY_S\"\n");
    iniOut(&out, "#PLAYER_1_BUTTON_UP = \"AT_TRANSLATED_SET_2_KEYBOARD_KEY_UP\"\n");
    iniOut(&out, "#PLAYER_1_BUTTON_DOWN = \"AT_TRANSLATED_SET_2_KEYBOARD_KEY_DOWN\"\n");
    iniOut(&out, "#PLAYER_1_BUTTON_LEFT = \"AT_TRANSLATED_SET_2_KEYBOARD_KEY_LEFT\"\n");
    iniOut(&out, "#PLAYER_1_BUTTON_RIGHT = \"AT_TRANSLATED_SET_2_KEYBOARD_KEY_RIGHT\"\n");
    iniOut(&out, "#PLAYER_1_BUTTON_1 = \"AT_TRANSLATED_SET_2_KEYBOARD_KEY_Q\"\n");
    iniOut(&out, "#PLAYER_1_BUTTON_2 = \"AT_TRANSLATED_SET_2_KEYBOARD_KEY_W\"\n");
    iniOut(&out, "#PLAYER_1_BUTTON_3 = \"AT_TRANSLATED_SET_2_KEYBOARD_KEY_E\"\n");
    iniOut(&out, "#PLAYER_1_BUTTON_4 = \"AT_TRANSLATED_SET_2_KEYBOARD_KEY_R\"\n");
    iniOut(&out, "#PLAYER_1_BUTTON_5 = \"AT_TRANSLATED_SET_2_KEYBOARD_KEY_Y\"\n");
    iniOut(&out, "#PLAYER_1_BUTTON_6 = \"AT_TRANSLATED_SET_2_KEYBOARD_KEY_U\"\n");
    iniOut(&out, "#PLAYER_1_BUTTON_7 = \"AT_TRANSLATED_SET_2_KEYBOARD_KEY_I\"\n");
    iniOut(&out, "#PLAYER_1_BUTTON_8 = \"AT_TRANSLATED_SET_2_KEYBOARD_KEY_O\"\n\n");
    iniOut(&out, "#PLAYER_1_COIN = \"AT_TRANSLATED_SET_2_KEYBOARD_KEY_6\"\n");
    iniOut(&out, "#PLAYER_2_BUTTON_START = \"AT_TRANSLATED_SET_2_KEYBOARD_KEY_1\"\n");
    iniOut(&out, "#PLAYER_2_BUTTON_SERVICE = \"AT_TRANSLATED_SET_2_KEYBOARD_KEY_S\"\n");
    iniOut(&out, "#PLAYER_2_BUTTON_UP = \"AT_TRANSLATED_SET_2_KEYBOARD_KEY_UP\"\n");
    iniOut(&out, "#PLAYER_2_BUTTON_DOWN = \"AT_TRANSLATED_SET_2_KEYBOARD_KEY_DOWN\"\n");
    iniOut(&out, "#PLAYER_2_BUTTON_LEFT = \"AT_TRANSLATED_SET_2_KEYBOARD_KEY_LEFT\"\n");
    iniOut(&out, "#PLAYER_2_BUTTON_RIGHT = \"AT_TRANSLATED_SET_2_KEYBOARD_KEY_RIGHT\"\n");
    iniOut(&out, "#PLAYER_2_BUTTON_1 = \"AT_TRANSLATED_SET_2_KEYBOARD_KEY_Q\"\n");
    iniOut(&out, "#PLAYER_2_BUTTON_2 = \"AT_TRANSLATED_SET_2_KEYBOARD_KEY_W\"\n");
    iniOut(&out, "#PLAYER_2_BUTTON_3 = \"AT_TRANSLATED_SET_2_KEYBOARD_KEY_E\"\n");
    iniOut(&out, "#PLAYER_2_BUTTON_4 = \"AT_TRANSLATED_SET_2_KEYBOARD_KEY_R\"\n");
    iniOut(&out, "#PLAYER_2_BUTTON_5 = \"AT_TRANSLATED_SET_2_KEYBOARD_KEY_Y\"\n");
    iniOut(&out, "#PLAYER_2_BUTTON_6 = \"AT_TRANSLATED_SET_2_KEYBOARD_KEY_U\"\n");
    iniOut(&out, "#PLAYER_2_BUTTON_7 = \"AT_TRANSLATED_SET_2_KEYBOARD_KEY_I\"\n");
    iniOut(&out, "#PLAYER_2_BUTTON_8 = \"AT_TRANSLATED_SET_2_KEYBOARD_KEY_O\"\n\n");
    iniOut(&out, "#ANALOGUE_1 = \"SYNPS_2_SYNAPTICS_TOUCHPAD_ABS_X\"\n");
    iniOut(&out, "#ANALOGUE_2 = \"SYNPS_2_SYNAPTICS_TOUCHPAD_ABS_Y\"\n");
    iniOut(&out, "#ANALOGUE_3 = \"SYNPS_2_SYNAPTICS_TOUCHPAD_ABS_Z\"\n");
    iniOut(&out, "#ANALOGUE_4 = \"SYNPS_2_SYNAPTICS_TOUCHPAD_ABS_RZ\"\n");
    iniOut(&out, "#ANALOGUE_5 = \"SYNPS_2_SYNAPTICS_TOUCHPAD_ABS_X2\"\n");
    iniOut(&out, "#ANALOGUE_6 = \"SYNPS_2_SYNAPTICS_TOUCHPAD_ABS_Y2\"\n");
    iniOut(&out, "#ANALOGUE_7 = \"SYNPS_2_SYNAPTICS_TOUCHPAD_ABS_Z2\"\n");
    iniOut(&out, "#ANALOGUE_8 = \"SYNPS_2_SYNAPTICS_TOUCHPAD_ABS_RZ2\"\n\n");

    for (int x = 1; x < 9; x++)
        iniOut(&out, "#ANALOGUE_DEADZONE_%d = 0 0 0\n", x);

    iniOut(&out, "\n");
    if (out.text)
        writeWithDefaults(file, out.text);
    free(out.text);
    fclose(file);
    return 1;
}
