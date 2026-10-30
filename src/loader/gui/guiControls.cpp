// The controls page of the settings, organised as Flycast's: the devices found, each with a page that binds the
// game's actions to its own inputs, then the settings common to all devices (force feedback, dead zones and the other
// options of controls.ini's [Config]).
//
// The live axis bar is sm2-emu's centre_bar (https://github.com/dmanlfc/sm2-emu, GPL-3.0-or-later, by Daniel Martin
// (dmanlfc) and the sm2-emu contributors), modified; see guiCommon.cpp.

#include <SDL3/SDL.h>

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

#include "../../imgui/imgui.h"

#include "guiCommon.h"
#include "../config/config.h"
extern "C"
{
#include "../config/iniParser.h"
#include "../hardware/lindbergh/forceFeedback.h"
#include "../hardware/lindbergh/jvs.h"
#include "../input/controlIniGen.h"
}
#include "../input/sdlInput.h"

extern "C" SDLControllers sdlJoysticks;

namespace gui
{
namespace
{
struct Row
{
    std::string key;   // P1_Steer, GearUp...
    std::string value; // KEY_Left, JOY0_AXIS_0...
    bool dirty = false;
};

const char *kSections[] = {"Common", "Driving", "Digital", "Flying", "Shooting", "Mahjong"};
const int kNumSections = 6;

std::string g_file;
bool g_loaded = false;
std::map<std::string, std::vector<Row>> g_rows; // section -> its bindings, in the file's order
std::vector<Row> g_configRows;                  // [Config], in the file's order
int g_section = -1;                             // the game type a device's page shows, 1..5
std::map<std::string, std::map<std::string, std::string>> g_defaults; // section -> action -> as a new controls.ini has it
bool g_defaultsRead = false;

enum DeviceKind
{
    KEYBOARD,
    MOUSE,
    GAMEPAD,
    JOYSTICK
};

struct Device
{
    DeviceKind kind = KEYBOARD;
    int index = 0; // JOYn / GCn
    std::string name;
};

struct Capture
{
    bool active = false;
    bool append = false; // "Add": the input joins the device's ones; "Map": it replaces them
    std::string section;
    size_t row = 0;
    float mouseX = 0.0f, mouseY = 0.0f; // mouse motion since the capture started
    Sint16 joyAxes[MAX_JOYSTICKS][MAX_JOY_AXES] = {};
    Sint16 padAxes[MAX_JOYSTICKS][SDL_GAMEPAD_AXIS_COUNT] = {};
} g_capture;

// The device whose page is open
struct Mapping
{
    bool requestOpen = false;
    Device device;
} g_mapping;

// English label of an action, translated by tr()
const char *actionLabel(const std::string &name)
{
    static const std::map<std::string, const char *> labels = {
        {"Test", N_("Test")},
        {"Coin", N_("Coin")},
        {"Start", N_("Start")},
        {"Service", N_("Service")},
        {"ExitGame", N_("Exit the game")},
        {"Steer", N_("Steering")},
        {"Gas", N_("Accelerator")},
        {"Brake", N_("Brake")},
        {"Steer_Left", N_("Steer left")},
        {"Steer_Right", N_("Steer right")},
        {"Gas_Digital", N_("Accelerator (digital)")},
        {"Brake_Digital", N_("Brake (digital)")},
        {"GearUp", N_("Gear up")},
        {"GearDown", N_("Gear down")},
        {"ViewChange", N_("Change view")},
        {"MusicChange", N_("Change music")},
        {"Boost", N_("Boost")},
        {"BoostRight", N_("Boost right")},
        {"Up", N_("Up")},
        {"Down", N_("Down")},
        {"Left", N_("Left")},
        {"Right", N_("Right")},
        {"Button1", N_("Button 1")},
        {"Button2", N_("Button 2")},
        {"Button3", N_("Button 3")},
        {"CardInsert", N_("Insert card")},
        {"Card1Insert", N_("Insert card 1")},
        {"Card2Insert", N_("Insert card 2")},
        {"Trigger", N_("Trigger")},
        {"Reload", N_("Reload")},
        {"GunButton", N_("Gun button")},
        {"ActionButton", N_("Action button")},
        {"PedalLeft", N_("Left pedal")},
        {"PedalRight", N_("Right pedal")},
        {"GunX", N_("Gun X")},
        {"GunY", N_("Gun Y")},
        {"Flying_X", N_("Stick X")},
        {"Flying_Y", N_("Stick Y")},
        {"Flying_Left", N_("Stick left")},
        {"Flying_Right", N_("Stick right")},
        {"Flying_Up", N_("Stick up")},
        {"Flying_Down", N_("Stick down")},
        {"Throttle", N_("Throttle")},
        {"Throttle_Accelerate", N_("Throttle up")},
        {"Throttle_Slowdown", N_("Throttle down")},
        {"GunTrigger", N_("Gun trigger")},
        {"MissileTrigger", N_("Missile trigger")},
        {"ClimaxSwitch", N_("Climax switch")},
    };
    auto it = labels.find(name);
    return it != labels.end() ? tr(it->second) : name.c_str();
}

// English label of a [Config] option, translated by tr(); nullptr for one shown by its own name
const char *optionLabel(const std::string &key)
{
    static const std::map<std::string, const char *> labels = {
        {"Steer_DeadZone", N_("Steering")},
        {"Gas_DeadZone", N_("Accelerator")},
        {"Brake_DeadZone", N_("Brake")},
        {"FLYING_X_DeadZone", N_("Stick X")},
        {"FLYING_Y_DeadZone", N_("Stick Y")},
        {"Throttle_DeadZone", N_("Throttle")},
        {"GunX_Centering", N_("Gun X centring")},
        {"GunY_Centering", N_("Gun Y centring")},
        {"ShakeIncreaseRate", N_("Screen shake: increase")},
        {"ShakeDecayRate", N_("Screen shake: decay")},
        {"ShakeMinScreenFraction", N_("Screen shake: minimum")},
        {"CardInsert_Toggle", N_("Insert card: toggle")},
        {"Card1Insert_Toggle", N_("Insert card 1: toggle")},
        {"Card2Insert_Toggle", N_("Insert card 2: toggle")},
    };
    auto it = labels.find(key);
    return it != labels.end() ? tr(it->second) : nullptr;
}

// P1_Steer -> player 1, Steer
std::string genericName(const std::string &key, int *player)
{
    *player = 0;
    if (key.size() > 3 && key[0] == 'P' && key[1] >= '1' && key[1] <= '4' && key[2] == '_')
    {
        *player = key[1] - '0';
        return key.substr(3);
    }
    return key;
}

bool isAnalog(const std::string &name)
{
    return name == "Steer" || name == "Gas" || name == "Brake" || name == "GunX" || name == "GunY" || name == "Flying_X" ||
           name == "Flying_Y" || name == "Throttle";
}

// A pedal's axis mode, from where it rested when the capture started and the way it went: at the axis's bottom it is
// the whole axis, at its top _INVERTED, in its middle the side it went to. Read whole, a pedal resting at the top
// gave the full value released, one resting in the middle half of it.
const char *pedalSuffix(const std::string &name, int rest, int delta)
{
    if (name != "Gas" && name != "Brake")
        return "";
    if (rest > 16000)
        return "_INVERTED";
    if (rest >= -16000)
        return delta > 0 ? "_POSITIVE_SIDE" : "_NEGATIVE_SIDE";
    return "";
}

std::string trim(const std::string &s)
{
    size_t start = s.find_first_not_of(' ');
    size_t end = s.find_last_not_of(' ');
    return start == std::string::npos ? std::string() : s.substr(start, end - start + 1);
}

// "A, B + C" -> {"A", "B + C"}: the alternatives of a binding
std::vector<std::string> alternatives(const std::string &value)
{
    std::vector<std::string> out;
    size_t start = 0;
    while (start <= value.size())
    {
        size_t end = value.find(',', start);
        std::string token = trim(value.substr(start, end == std::string::npos ? std::string::npos : end - start));
        if (!token.empty())
            out.push_back(token);
        if (end == std::string::npos)
            break;
        start = end + 1;
    }
    return out;
}

std::string join(const std::vector<std::string> &tokens)
{
    std::string out;
    for (const std::string &token : tokens)
        out += (out.empty() ? "" : ", ") + token;
    return out;
}

// The parts of a combination: "B + C" -> {"B", "C"}
std::vector<std::string> comboParts(const std::string &token)
{
    std::vector<std::string> out;
    size_t start = 0;
    while (true)
    {
        size_t end = token.find('+', start);
        std::string part = trim(token.substr(start, end == std::string::npos ? std::string::npos : end - start));
        if (!part.empty())
            out.push_back(part);
        if (end == std::string::npos)
            break;
        start = end + 1;
    }
    return out;
}

bool hasPrefix(const std::string &s, const std::string &prefix)
{
    return s.compare(0, prefix.size(), prefix) == 0;
}

// Whether an alternative of a binding belongs to the device: its first input is one of the device's
bool owns(const Device &device, const std::string &token)
{
    switch (device.kind)
    {
        case KEYBOARD:
            return hasPrefix(token, "KEY_");
        case MOUSE:
            return hasPrefix(token, "MOUSE_");
        case GAMEPAD:
            return hasPrefix(token, "GC" + std::to_string(device.index) + "_") || hasPrefix(token, "JOY" + std::to_string(device.index) + "_");
        case JOYSTICK:
            return hasPrefix(token, "JOY" + std::to_string(device.index) + "_");
    }
    return false;
}

std::vector<std::string> deviceTokens(const Device &device, const std::string &value)
{
    std::vector<std::string> out;
    for (const std::string &token : alternatives(value))
        if (owns(device, token))
            out.push_back(token);
    return out;
}

std::vector<std::string> otherTokens(const Device &device, const std::string &value)
{
    std::vector<std::string> out;
    for (const std::string &token : alternatives(value))
        if (!owns(device, token))
            out.push_back(token);
    return out;
}

bool sameToken(const std::string &a, const std::string &b)
{
    return SDL_strcasecmp(a.c_str(), b.c_str()) == 0;
}

bool sameTokens(const std::vector<std::string> &a, const std::vector<std::string> &b)
{
    return std::equal(a.begin(), a.end(), b.begin(), b.end(), sameToken);
}

std::vector<Device> devices()
{
    std::vector<Device> out;
    out.push_back({KEYBOARD, 0, tr("Keyboard")});
    out.push_back({MOUSE, 0, tr("Mouse")});
    for (int d = 0; d < MAX_JOYSTICKS; d++)
    {
        if (sdlJoysticks.controllers[d])
            out.push_back({GAMEPAD, d, SDL_GetGamepadName(sdlJoysticks.controllers[d]) ? SDL_GetGamepadName(sdlJoysticks.controllers[d]) : "?"});
        else if (sdlJoysticks.joysticks[d])
            out.push_back({JOYSTICK, d, SDL_GetJoystickName(sdlJoysticks.joysticks[d]) ? SDL_GetJoystickName(sdlJoysticks.joysticks[d]) : "?"});
    }
    return out;
}

std::string deviceId(const Device &device)
{
    switch (device.kind)
    {
        case KEYBOARD:
            return "KEY";
        case MOUSE:
            return "MOUSE";
        case GAMEPAD:
            return "GC" + std::to_string(device.index) + " / JOY" + std::to_string(device.index);
        case JOYSTICK:
            return "JOY" + std::to_string(device.index);
    }
    return "";
}

SDL_Joystick *joystickOf(int index)
{
    if (index < 0 || index >= MAX_JOYSTICKS)
        return nullptr;
    if (sdlJoysticks.joysticks[index])
        return sdlJoysticks.joysticks[index];
    return sdlJoysticks.controllers[index] ? SDL_GetGamepadJoystick(sdlJoysticks.controllers[index]) : nullptr;
}

std::string format(const char *fmt, int value)
{
    char text[64];
    snprintf(text, sizeof(text), fmt, value);
    return text;
}

std::string axisSuffix(const char *suffix)
{
    if (strcmp(suffix, "_POSITIVE") == 0)
        return " +";
    if (strcmp(suffix, "_NEGATIVE") == 0)
        return " -";
    if (strcmp(suffix, "_POSITIVE_HALF") == 0)
        return std::string(" +") + tr(" (half)");
    if (strcmp(suffix, "_NEGATIVE_HALF") == 0)
        return std::string(" -") + tr(" (half)");
    if (strcmp(suffix, "_POSITIVE_SIDE") == 0)
        return std::string(" +") + tr(" (pedal)");
    if (strcmp(suffix, "_NEGATIVE_SIDE") == 0)
        return std::string(" -") + tr(" (pedal)");
    if (strcmp(suffix, "_INVERTED") == 0)
        return tr(" (inverted)");
    return suffix;
}

// An input as the page shows it: "Axis 0 +", "Button 3", "Left button"...
std::string describe(const std::string &token)
{
    std::vector<std::string> parts = comboParts(token);
    if (parts.size() > 1)
    {
        std::string out;
        for (const std::string &part : parts)
            out += (out.empty() ? "" : " + ") + describe(part);
        return out;
    }
    int device = 0, number = 0, length = 0;
    char name[32] = "";
    const char *s = token.c_str();
    if (hasPrefix(token, "KEY_"))
    {
        // SDL's name: "Keypad +" for KEY_KP_PLUS
        SDL_Scancode scancode = keyNameToScancode(token.c_str() + 4);
        return scancode != SDL_SCANCODE_UNKNOWN ? SDL_GetScancodeName(scancode) : token.substr(4);
    }
    if (token == "MOUSE_LEFT_BUTTON")
        return tr("Left button");
    if (token == "MOUSE_RIGHT_BUTTON")
        return tr("Right button");
    if (token == "MOUSE_MIDDLE_BUTTON")
        return tr("Middle button");
    if (token == "MOUSE_AXIS_X")
        return tr("X axis");
    if (token == "MOUSE_AXIS_Y")
        return tr("Y axis");
    if (sscanf(s, "JOY%d_BUTTON_%d%n", &device, &number, &length) == 2 && s[length] == '\0')
        return format(tr("Button %d"), number + (getConfig()->buttonsFromOne ? 1 : 0));
    if (sscanf(s, "JOY%d_AXIS_%d%n", &device, &number, &length) == 2)
        return format(tr("Axis %d"), number) + axisSuffix(s + length);
    if (sscanf(s, "JOY%d_HAT%d_%31s", &device, &number, name) == 3)
    {
        const char *direction = strcmp(name, "UP") == 0     ? tr("up")
                                : strcmp(name, "DOWN") == 0 ? tr("down")
                                : strcmp(name, "LEFT") == 0 ? tr("left")
                                                            : tr("right");
        return format(tr("Hat %d"), number) + " " + direction;
    }
    if (sscanf(s, "GC%d_BUTTON_%31s", &device, name) == 2)
        return name;
    if (sscanf(s, "GC%d_AXIS_%31[^_]%n", &device, name, &length) == 2)
        return std::string(name) + axisSuffix(s + length);
    return token;
}

// Inputs as the page shows them: "Up, Button 3"; empty for none
std::string describeAll(const std::vector<std::string> &tokens)
{
    std::string out;
    for (const std::string &token : tokens)
        out += (out.empty() ? "" : ", ") + describe(token);
    return out;
}

// Whether an input is pressed now; a combination when all its parts are
bool pressed(const std::string &token)
{
    std::vector<std::string> parts = comboParts(token);
    if (parts.size() > 1)
    {
        for (const std::string &part : parts)
            if (!pressed(part))
                return false;
        return true;
    }
    int device = 0, number = 0, length = 0;
    char name[32] = "";
    const char *s = token.c_str();
    if (hasPrefix(token, "KEY_"))
    {
        SDL_Scancode scancode = keyNameToScancode(token.c_str() + 4);
        return scancode != SDL_SCANCODE_UNKNOWN && SDL_GetKeyboardState(nullptr)[scancode];
    }
    if (hasPrefix(token, "MOUSE_") && token.find("_BUTTON") != std::string::npos)
    {
        SDL_MouseButtonFlags buttons = SDL_GetMouseState(nullptr, nullptr);
        int button = token == "MOUSE_LEFT_BUTTON" ? SDL_BUTTON_LEFT : token == "MOUSE_RIGHT_BUTTON" ? SDL_BUTTON_RIGHT : SDL_BUTTON_MIDDLE;
        return (buttons & SDL_BUTTON_MASK(button)) != 0;
    }
    if (sscanf(s, "JOY%d_BUTTON_%d%n", &device, &number, &length) == 2 && s[length] == '\0')
        return joystickOf(device) && SDL_GetJoystickButton(joystickOf(device), number);
    if (sscanf(s, "JOY%d_AXIS_%d%n", &device, &number, &length) == 2)
    {
        if (!joystickOf(device))
            return false;
        Sint16 value = SDL_GetJoystickAxis(joystickOf(device), number);
        return hasPrefix(s + length, "_POSITIVE") ? value > 16000 : hasPrefix(s + length, "_NEGATIVE") ? value < -16000 : false;
    }
    if (sscanf(s, "JOY%d_HAT%d_%31s", &device, &number, name) == 3)
    {
        if (!joystickOf(device))
            return false;
        Uint8 hat = SDL_GetJoystickHat(joystickOf(device), number);
        Uint8 mask = strcmp(name, "UP") == 0 ? SDL_HAT_UP : strcmp(name, "DOWN") == 0 ? SDL_HAT_DOWN : strcmp(name, "LEFT") == 0 ? SDL_HAT_LEFT : SDL_HAT_RIGHT;
        return (hat & mask) != 0;
    }
    if (sscanf(s, "GC%d_BUTTON_%31s", &device, name) == 2 && device >= 0 && device < MAX_JOYSTICKS && sdlJoysticks.controllers[device])
        return SDL_GetGamepadButton(sdlJoysticks.controllers[device], SDL_GetGamepadButtonFromString(name));
    if (sscanf(s, "GC%d_AXIS_%31[^_]%n", &device, name, &length) == 2 && device >= 0 && device < MAX_JOYSTICKS && sdlJoysticks.controllers[device])
    {
        Sint16 value = SDL_GetGamepadAxis(sdlJoysticks.controllers[device], SDL_GetGamepadAxisFromString(name));
        return hasPrefix(s + length, "_POSITIVE") ? value > 16000 : hasPrefix(s + length, "_NEGATIVE") ? value < -16000 : value > 16000;
    }
    return false;
}

// The raw value, -32768..32767, of the first axis among some inputs: JOYn_AXIS_m or GCn_AXIS_name; used: that input
bool axisOf(const std::vector<std::string> &tokens, Sint16 *raw, std::string *used = nullptr)
{
    for (const std::string &token : tokens)
    {
        int device = 0, axis = 0;
        char name[32] = "";
        if (sscanf(token.c_str(), "JOY%d_AXIS_%d", &device, &axis) == 2 && joystickOf(device))
        {
            *raw = SDL_GetJoystickAxis(joystickOf(device), axis);
            if (used)
                *used = token;
            return true;
        }
        if (sscanf(token.c_str(), "GC%d_AXIS_%31[^_ ,]", &device, name) == 2 && device >= 0 && device < MAX_JOYSTICKS &&
            sdlJoysticks.controllers[device])
        {
            *raw = SDL_GetGamepadAxis(sdlJoysticks.controllers[device], SDL_GetGamepadAxisFromString(name));
            if (used)
                *used = token;
            return true;
        }
    }
    return false;
}

// A pedal, 0 released to 1 pressed, as its input reads it: the whole axis, one side of it, a trigger, inverted
float pedalLevel(const std::string &token, Sint16 raw)
{
    float level;
    if (token.find("_POSITIVE_SIDE") != std::string::npos)
        level = raw > 0 ? (float)raw / 32767.0f : 0.0f;
    else if (token.find("_NEGATIVE_SIDE") != std::string::npos)
        level = raw < 0 ? -(float)raw / 32768.0f : 0.0f;
    else if (token.find("TRIGGER") != std::string::npos) // GCn_AXIS_LEFTTRIGGER: 0 to 32767
        level = (float)raw / 32767.0f;
    else
        level = ((float)raw + 32768.0f) / 65535.0f;
    return token.find("_INVERTED") != std::string::npos ? 1.0f - level : level;
}

Row *configRow(const std::string &key)
{
    for (Row &row : g_configRows)
        if (row.key == key)
            return &row;
    return nullptr;
}

void load()
{
    g_rows.clear();
    g_configRows.clear();
    // No controls.ini yet (a new folder, the picker's), or one without bindings (the loader wrote only the
    // controllers' GUIDs in the game's folder): the game uses its built-in bindings, which have no row here to map.
    // They are written to the file first, as a new controls.ini has them, keeping what the file held.
    IniConfig *ini = g_file.empty() ? nullptr : iniLoad(g_file.c_str());
    if (!g_file.empty() && !(ini && iniGetSection(ini, "Common")) && createDefaultControlsIni(g_file.c_str()) > 0)
    {
        IniConfig *defaults = iniLoad(g_file.c_str());
        if (defaults && ini)
        {
            for (int s = 0; s < ini->numSections; s++)
                for (int i = 0; i < ini->sections[s].numPairs; i++)
                    if (ini->sections[s].pairs[i].value)
                        iniSetValue(defaults, ini->sections[s].name, ini->sections[s].pairs[i].key,
                                    ini->sections[s].pairs[i].value);
            iniSave(defaults, g_file.c_str());
        }
        if (ini)
            iniFree(ini);
        ini = defaults;
    }
    if (ini)
    {
        for (int s = 0; s < ini->numSections; s++)
        {
            const IniSection &section = ini->sections[s];
            std::vector<Row> rows;
            for (int i = 0; i < section.numPairs; i++)
                rows.push_back({section.pairs[i].key, section.pairs[i].value ? section.pairs[i].value : "", false});
            if (strcmp(section.name, "Config") == 0)
                g_configRows = rows;
            else
                g_rows[section.name] = rows;
        }
        iniFree(ini);
    }
    g_loaded = true;
}

void setRow(Row &row, const std::string &value)
{
    if (row.value != value)
    {
        row.value = value;
        row.dirty = true;
    }
}

// The row with the device's inputs replaced by those given, the other devices' kept
void setDeviceTokens(const Device &device, Row &row, const std::vector<std::string> &mine)
{
    std::vector<std::string> tokens = otherTokens(device, row.value);
    tokens.insert(tokens.end(), mine.begin(), mine.end());
    setRow(row, join(tokens));
}

void startCapture(const std::string &section, size_t row, bool append)
{
    g_capture.active = true;
    g_capture.append = append;
    g_capture.section = section;
    g_capture.row = row;
    g_capture.mouseX = g_capture.mouseY = 0.0f;
    // Where the axes rest now: a pedal rests at one end, a stick in the middle
    for (int d = 0; d < MAX_JOYSTICKS; d++)
    {
        for (int a = 0; a < MAX_JOY_AXES; a++)
            g_capture.joyAxes[d][a] = joystickOf(d) ? SDL_GetJoystickAxis(joystickOf(d), a) : 0;
        for (int a = 0; a < SDL_GAMEPAD_AXIS_COUNT; a++)
            g_capture.padAxes[d][a] = sdlJoysticks.controllers[d] ? SDL_GetGamepadAxis(sdlJoysticks.controllers[d], (SDL_GamepadAxis)a) : 0;
    }
}

// "Map": the input pressed replaces the device's ones; "Add": it joins them, once. The other devices' stay
void finishCapture(const std::string &token)
{
    std::vector<Row> &rows = g_rows[g_capture.section];
    if (g_capture.row < rows.size())
    {
        Row &row = rows[g_capture.row];
        std::vector<std::string> mine;
        if (g_capture.append)
            mine = deviceTokens(g_mapping.device, row.value);
        if (std::none_of(mine.begin(), mine.end(), [&token](const std::string &t) { return sameToken(t, token); }))
            mine.push_back(token);
        setDeviceTokens(g_mapping.device, row, mine);
    }
    g_capture.active = false;
}

// The bindings of a new controls.ini, written aside and read once; false when it could not be
bool readDefaults()
{
    if (!g_defaultsRead)
    {
        g_defaultsRead = true;
        std::error_code error;
        std::filesystem::path temp = std::filesystem::temp_directory_path(error) / "linuxloader-default-controls.ini";
        // iniSave, under it, returns 1 on success and -1 on failure
        if (error || createDefaultControlsIni(temp.string().c_str()) < 0)
            return false;
        IniConfig *defaults = iniLoad(temp.string().c_str());
        if (defaults)
        {
            for (int s = 0; s < defaults->numSections; s++)
                for (int i = 0; i < defaults->sections[s].numPairs; i++)
                    if (defaults->sections[s].pairs[i].value)
                        g_defaults[defaults->sections[s].name][defaults->sections[s].pairs[i].key] = defaults->sections[s].pairs[i].value;
            iniFree(defaults);
        }
        std::filesystem::remove(temp, error);
    }
    return !g_defaults.empty();
}

// The device's inputs of an action as a new controls.ini has them
std::vector<std::string> defaultTokens(const Device &device, const std::string &section, const std::string &key)
{
    if (!readDefaults())
        return {};
    auto inSection = g_defaults.find(section);
    if (inSection == g_defaults.end())
        return {};
    auto value = inSection->second.find(key);
    return value == inSection->second.end() ? std::vector<std::string>() : deviceTokens(device, value->second);
}

// The device's bindings back to the defaults of a new controls.ini, in the sections given
void resetDevice(const Device &device, const std::vector<std::string> &sections)
{
    if (!readDefaults())
        return;
    for (const std::string &section : sections)
        for (Row &row : g_rows[section])
            setDeviceTokens(device, row, defaultTokens(device, section, row.key));
}

// sm2-emu's centre bar: the axis around its middle, the dead zone shaded
void centreBar(const char *id, Sint16 raw, float deadzone)
{
    const ImGuiStyle &style = ImGui::GetStyle();
    const float width = ImGui::CalcItemWidth();
    const float height = ImGui::GetFrameHeight() * 0.6f;
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton(id, ImVec2(width, height));
    ImDrawList *draw = ImGui::GetWindowDrawList();

    const float mid = pos.x + width * 0.5f;
    draw->AddRectFilled(pos, ImVec2(pos.x + width, pos.y + height), ImGui::GetColorU32(ImGuiCol_FrameBg), style.FrameRounding);
    if (deadzone > 0.0f)
    {
        const float half = width * 0.5f * deadzone;
        draw->AddRectFilled(ImVec2(mid - half, pos.y), ImVec2(mid + half, pos.y + height), ImGui::GetColorU32(ImGuiCol_FrameBgHovered));
    }
    draw->AddLine(ImVec2(mid, pos.y), ImVec2(mid, pos.y + height), ImGui::GetColorU32(ImGuiCol_TextDisabled));
    const float value = std::clamp((float)raw / 32767.0f, -1.0f, 1.0f);
    const float x = std::round(mid + width * 0.5f * value);
    const bool inside = std::fabs(value) <= deadzone;
    draw->AddRectFilled(ImVec2(x - 2.0f, pos.y), ImVec2(x + 2.0f, pos.y + height),
                        ImGui::GetColorU32(inside ? ImGuiCol_TextDisabled : ImGuiCol_SliderGrabActive));
}

// A dot lit while the input is pressed
void pressDot(bool on)
{
    const float size = ImGui::GetFontSize() * 0.7f;
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    ImGui::Dummy(ImVec2(size, ImGui::GetFontSize()));
    ImGui::GetWindowDrawList()->AddCircleFilled(ImVec2(pos.x + size * 0.5f, pos.y + ImGui::GetFontSize() * 0.5f), size * 0.5f,
                                                ImGui::GetColorU32(on ? ImGuiCol_SliderGrabActive : ImGuiCol_FrameBg));
}

// A title bar across the page, as Flycast's
void header(const char *label)
{
    const ImGuiStyle &style = ImGui::GetStyle();
    ImGui::Spacing();
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const float width = ImGui::GetContentRegionAvail().x;
    const float height = ImGui::GetFrameHeight();
    ImGui::GetWindowDrawList()->AddRectFilled(pos, ImVec2(pos.x + width, pos.y + height), ImGui::GetColorU32(ImGuiCol_Header), style.FrameRounding);
    ImGui::SetCursorScreenPos(ImVec2(pos.x + style.FramePadding.x, pos.y + style.FramePadding.y));
    ImGui::TextUnformatted(label);
    ImGui::SetCursorScreenPos(ImVec2(pos.x, pos.y + height + style.ItemSpacing.y));
    ImGui::Dummy(ImVec2(0.0f, 0.0f));
}

int defaultSection(bool inGame)
{
    if (inGame)
    {
        switch (getConfig()->gameType)
        {
            case DIGITAL:
                return 2;
            case FLYING:
                return 3;
            case SHOOTING:
                return 4;
            case MAHJONG:
                return 5;
            default:
                break;
        }
    }
    return 1; // driving
}

const char *sectionLabel(int section)
{
    static const char *labels[kNumSections] = {N_("All games"), N_("Driving"), N_("Buttons"), N_("Flying"), N_("Shooting"), N_("Mahjong")};
    return tr(labels[std::clamp(section, 0, kNumSections - 1)]);
}

// One section's rows of the device's page, under a title bar per player: the rows for all players first, then
// player 1's, player 2's...
void drawBindings(const std::string &section, int sectionIndex)
{
    std::vector<Row> &rows = g_rows[section];
    if (rows.empty())
        return;
    std::vector<size_t> order(rows.size());
    for (size_t i = 0; i < rows.size(); i++)
        order[i] = i;
    std::stable_sort(order.begin(), order.end(), [&rows](size_t a, size_t b) {
        int playerA = 0, playerB = 0;
        genericName(rows[a].key, &playerA);
        genericName(rows[b].key, &playerB);
        return playerA < playerB;
    });

    const Device &device = g_mapping.device;
    int shownPlayer = -1;
    bool tableOpen = false;
    for (size_t i : order)
    {
        Row &row = rows[i];
        int player = 0;
        const std::string name = genericName(row.key, &player);
        if (player != shownPlayer)
        {
            if (tableOpen)
                ImGui::EndTable();
            char title[96];
            if (player)
                snprintf(title, sizeof(title), "%s - %s", sectionLabel(sectionIndex), format(tr("Player %d"), player).c_str());
            else
                snprintf(title, sizeof(title), "%s", sectionLabel(sectionIndex));
            header(title);
            tableOpen = ImGui::BeginTable((section + std::to_string(player)).c_str(), 4, ImGuiTableFlags_SizingStretchProp);
            if (tableOpen)
            {
                ImGui::TableSetupColumn("##action", ImGuiTableColumnFlags_WidthStretch, 0.34f);
                ImGui::TableSetupColumn("##binding", ImGuiTableColumnFlags_WidthStretch, 0.30f);
                ImGui::TableSetupColumn("##value", ImGuiTableColumnFlags_WidthStretch, 0.22f);
                ImGui::TableSetupColumn("##buttons", ImGuiTableColumnFlags_WidthFixed);
            }
            shownPlayer = player;
        }
        if (!tableOpen)
            continue;

        ImGui::PushID((section + row.key).c_str());
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::AlignTextToFramePadding();
        ImGui::Text("%s%s", actionLabel(name), row.dirty ? " *" : "");
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", row.key.c_str());

        const std::vector<std::string> tokens = deviceTokens(device, row.value);
        ImGui::TableSetColumnIndex(1);
        ImGui::AlignTextToFramePadding();
        const std::string text = describeAll(tokens);
        if (text.empty())
            ImGui::TextDisabled("-");
        else
            ImGui::TextUnformatted(text.c_str());
        if (!tokens.empty() && ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", join(tokens).c_str());

        ImGui::TableSetColumnIndex(2);
        Sint16 raw = 0;
        std::string axis;
        if (isAnalog(name) && axisOf(tokens, &raw, &axis))
        {
            ImGui::SetNextItemWidth(-FLT_MIN);
            if (name == "Gas" || name == "Brake" || name == "Throttle") // a pedal: 0 % released, as its input reads it
                ImGui::ProgressBar(pedalLevel(axis, raw), ImVec2(-FLT_MIN, ImGui::GetFrameHeight() * 0.6f), "");
            else
            {
                const Row *dz = configRow(name + "_DeadZone");
                centreBar("##axis", raw, dz ? (float)atoi(dz->value.c_str()) / 32767.0f : 0.0f);
            }
        }
        else if (!tokens.empty() && !isAnalog(name))
        {
            bool on = false;
            for (const std::string &token : tokens)
                on = on || pressed(token);
            pressDot(on);
        }

        // "Map" replaces the device's inputs of the action, "Add" adds one (nothing to add to without one: "Map"),
        // "Unmap" removes them, "Default" puts back those of a new controls.ini (greyed when they are)
        ImGui::TableSetColumnIndex(3);
        if (ImGui::Button(tr("Map")))
            startCapture(section, i, false);
        ImGui::SameLine();
        ImGui::BeginDisabled(tokens.empty());
        if (ImGui::Button(tr("Add")))
            startCapture(section, i, true);
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button(tr("Unmap")))
            setDeviceTokens(device, row, {});
        ImGui::SameLine();
        const std::vector<std::string> defaults = defaultTokens(device, section, row.key);
        ImGui::BeginDisabled(!readDefaults() || sameTokens(tokens, defaults));
        if (ImGui::Button(tr("Default")))
            setDeviceTokens(device, row, defaults);
        ImGui::EndDisabled();
        if (readDefaults() && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        {
            const std::string shown = describeAll(defaults);
            ImGui::SetTooltip(tr("Default: %s"), shown.empty() ? "-" : shown.c_str());
        }
        ImGui::PopID();
    }
    if (tableOpen)
        ImGui::EndTable();
}

// A device's page, over the whole window: its inputs for every action of the game type chosen
void drawMappingPage(bool inGame)
{
    if (g_mapping.requestOpen)
    {
        ImGui::OpenPopup("###mapping");
        g_mapping.requestOpen = false;
    }
    const ImGuiViewport *viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowBgAlpha(1.0f); // a page of its own: nothing shows through
    char title[160];
    snprintf(title, sizeof(title), "%s: %s###mapping", tr("Bindings"), g_mapping.device.name.c_str());
    if (!ImGui::BeginPopupModal(title, nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings))
        return;

    if (ImGui::Button(tr("Close")))
    {
        bool written = false;
        controlsSave(inGame, &written);
        ImGui::CloseCurrentPopup();
    }

    // On the right: the defaults, and the game type whose actions are shown
    const char *resetLabel = tr("Reset...");
    const ImGuiStyle &style = ImGui::GetStyle();
    const float comboWidth = 180.0f * ImGui::GetFontSize() / 17.0f;
    const float resetWidth = ImGui::CalcTextSize(resetLabel).x + style.FramePadding.x * 2.0f;
    ImGui::SameLine(ImGui::GetContentRegionMax().x - comboWidth - resetWidth - style.ItemSpacing.x);
    if (ImGui::Button(resetLabel))
        ImGui::OpenPopup("##reset");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(comboWidth);
    if (ImGui::BeginCombo("##gametype", sectionLabel(g_section)))
    {
        for (int s = 1; s < kNumSections; s++)
            if (ImGui::Selectable(sectionLabel(s), s == g_section))
                g_section = s;
        ImGui::EndCombo();
    }

    const std::string section = kSections[std::clamp(g_section, 1, kNumSections - 1)];
    if (ImGui::BeginPopupModal("##reset", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar))
    {
        ImGui::Text("%s", tr("Set this device's bindings back to the defaults?"));
        if (ImGui::Button(tr("Yes")))
        {
            resetDevice(g_mapping.device, {"Common", section});
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button(tr("No")))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    if (ImGui::BeginChild("##bindings", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders))
    {
        drawBindings("Common", 0);
        drawBindings(section, g_section);
        if (g_rows[section].empty())
            ImGui::TextDisabled("%s", tr("This section of controls.ini is empty."));
    }
    ImGui::EndChild();

    if (g_capture.active)
        ImGui::OpenPopup("##capture");
    if (ImGui::BeginPopupModal("##capture", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar))
    {
        if (!g_capture.active)
            ImGui::CloseCurrentPopup();
        else
        {
            const std::vector<Row> &rows = g_rows[g_capture.section];
            int player = 0;
            const std::string name = g_capture.row < rows.size() ? genericName(rows[g_capture.row].key, &player) : "";
            ImGui::Text(tr("Press an input of %s for: %s"), g_mapping.device.name.c_str(), actionLabel(name));
            ImGui::TextDisabled("%s", tr("Escape: cancel"));
            // A click would be the mouse's input: the mouse cancels with Escape only
            if (g_mapping.device.kind != MOUSE && ImGui::Button(tr("Cancel")))
                g_capture.active = false;
        }
        ImGui::EndPopup();
    }
    ImGui::EndPopup();
}

// The options of [Config]: dead zones as sliders, toggles as boxes, the rest as numbers
void drawOptions()
{
    for (Row &row : g_configRows)
    {
        const char *label = optionLabel(row.key);
        const std::string shown = std::string(label ? label : row.key.c_str()) + "##" + row.key;
        const bool isDeadZone = row.key.size() > 9 && row.key.compare(row.key.size() - 9, 9, "_DeadZone") == 0;
        const bool isToggle = row.key.size() > 7 && row.key.compare(row.key.size() - 7, 7, "_Toggle") == 0;
        if (isDeadZone)
        {
            int value = atoi(row.value.c_str());
            if (ImGui::SliderInt(shown.c_str(), &value, 0, 16000))
                setRow(row, std::to_string(value));
        }
        else if (isToggle)
        {
            bool value = atoi(row.value.c_str()) != 0;
            if (ImGui::Checkbox(shown.c_str(), &value))
                setRow(row, value ? "1" : "0");
        }
        else if (row.value.find('.') != std::string::npos)
        {
            float value = (float)atof(row.value.c_str());
            if (ImGui::InputFloat(shown.c_str(), &value, 0.05f, 0.1f, "%.2f"))
            {
                char text[32];
                snprintf(text, sizeof(text), "%.2f", value);
                setRow(row, text);
            }
        }
        else
        {
            int value = atoi(row.value.c_str());
            if (ImGui::InputInt(shown.c_str(), &value))
                setRow(row, std::to_string(value));
        }
    }
}

// The device the forces go to: Automatic (the first controller's) or a force feedback device by its name, which
// FFB_DEVICE keeps. In game the effects move to it at once.
void drawFfbDevice(bool inGame)
{
    EmulatorConfig *cfg = getConfig();
    const bool automatic = sdlFfbAutomatic(cfg->ffbDevice);
    if (ImGui::BeginCombo(tr("Device"), automatic ? tr("Automatic") : cfg->ffbDevice))
    {
        static char names[16][FFB_DEVICE_NAME];
        const int count = sdlFfbDevices(names, IM_ARRAYSIZE(names));
        std::string chosen;
        if (ImGui::Selectable(tr("Automatic"), automatic))
            chosen = "AUTO";
        bool listed = automatic;
        for (int i = 0; i < count; i++)
        {
            const bool selected = !automatic && sdlFfbNameMatches(names[i], cfg->ffbDevice);
            listed = listed || selected;
            ImGui::PushID(i);
            if (ImGui::Selectable(names[i], selected))
                chosen = names[i];
            ImGui::PopID();
        }
        if (!listed)
            ImGui::Selectable((std::string(cfg->ffbDevice) + tr(" (not connected)")).c_str(), true, ImGuiSelectableFlags_Disabled);
        ImGui::EndCombo();
        if (!chosen.empty())
        {
            snprintf(cfg->ffbDevice, sizeof(cfg->ffbDevice), "%s", chosen.c_str());
            sdlFfbSelect(cfg->ffbDevice);
        }
    }
    if (inGame)
    {
        const char *current = sdlFfbCurrentDevice();
        ImGui::TextDisabled(tr("Forces sent to: %s"), current ? current : tr("no device"));
    }
}

// What the game gets: each input the game reads, as its board gives it now (a switch pressed or not, a channel's
// count, a player's coins), to check a binding from the device to the game. The players are named when there are two
void drawGameInputs()
{
    struct Input
    {
        int player;
        std::string name;
        GameInputKind kind;
        int value;
    };
    std::vector<Input> inputs;
    sdlInputGameInputs(
        [](int player, const char *name, GameInputKind kind, int value, void *context) {
            static_cast<std::vector<Input> *>(context)->push_back({player, name, kind, value});
        },
        &inputs);
    if (inputs.empty())
        ImGui::TextDisabled("-");
    if (inputs.empty() || !ImGui::BeginTable("##gameinputs", 2, ImGuiTableFlags_SizingStretchProp))
        return;
    const bool twoPlayers = std::any_of(inputs.begin(), inputs.end(), [](const Input &input) { return input.player == PLAYER_2; });
    const int analogueMax = getJVSIO()->analogueMax > 0 ? getJVSIO()->analogueMax : 1;
    ImGui::TableSetupColumn("##action", ImGuiTableColumnFlags_WidthStretch, 0.4f);
    ImGui::TableSetupColumn("##value", ImGuiTableColumnFlags_WidthStretch, 0.6f);
    for (const Input &input : inputs)
    {
        ImGui::PushID((std::to_string(input.player) + input.name).c_str());
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::AlignTextToFramePadding();
        if (twoPlayers && input.player != SYSTEM)
            ImGui::Text("%s (%s)", actionLabel(input.name), format(tr("Player %d"), input.player).c_str());
        else
            ImGui::TextUnformatted(actionLabel(input.name));
        ImGui::TableSetColumnIndex(1);
        ImGui::AlignTextToFramePadding();
        if (input.kind == GAME_INPUT_ANALOGUE)
        {
            char count[16];
            snprintf(count, sizeof(count), "%d", input.value);
            ImGui::ProgressBar(std::clamp((float)input.value / (float)analogueMax, 0.0f, 1.0f), ImVec2(-FLT_MIN, 0.0f), count);
        }
        else if (input.kind == GAME_INPUT_COIN)
            ImGui::Text("%d", input.value);
        else if (input.value)
            ImGui::TextUnformatted(tr("pressed"));
        else
            ImGui::TextDisabled("-");
        ImGui::PopID();
    }
    ImGui::EndTable();
}
} // namespace

void setControlsFile(const std::string &path)
{
    if (path != g_file)
    {
        g_file = path;
        g_loaded = false;
    }
}

void drawControlsTab(bool inGame, const std::string &configFile)
{
    static std::string buttonsSaved; // where the buttons' box wrote its choice
    if (!g_loaded)
        load();
    if (g_section < 0)
        g_section = defaultSection(inGame);

    header(tr("Devices"));
    if (ImGui::BeginTable("##devices", 3, ImGuiTableFlags_SizingStretchProp))
    {
        ImGui::TableSetupColumn(tr("Name"), ImGuiTableColumnFlags_WidthStretch, 0.6f);
        ImGui::TableSetupColumn(tr("In controls.ini"), ImGuiTableColumnFlags_WidthStretch, 0.4f);
        ImGui::TableSetupColumn("##map", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableHeadersRow();
        int i = 0;
        for (const Device &device : devices())
        {
            ImGui::PushID(i++);
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::AlignTextToFramePadding();
            const bool haptic = (device.kind == GAMEPAD || device.kind == JOYSTICK) && device.index == sdlFfbJoystick();
            ImGui::Text("%s%s", device.name.c_str(), haptic ? tr(" (force feedback)") : "");
            ImGui::TableSetColumnIndex(1);
            ImGui::AlignTextToFramePadding();
            ImGui::TextDisabled("%s", deviceId(device).c_str());
            ImGui::TableSetColumnIndex(2);
            if (ImGui::Button(tr("Map")))
            {
                g_mapping.device = device;
                g_mapping.requestOpen = true;
            }
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    if (!sdlJoysticks.controllers[0] && !sdlJoysticks.joysticks[0])
        ImGui::TextDisabled("%s", tr("No controller connected."));

    // Only the page's numbers change: written at once to the settings file read, like windowsloader's
    EmulatorConfig *cfg = getConfig();
    bool fromOne = cfg->buttonsFromOne != 0;
    if (ImGui::Checkbox(tr("Number the buttons from 1 (as Windows and vJoy do)"), &fromOne))
    {
        cfg->buttonsFromOne = fromOne;
        const char *value = fromOne ? "true" : "false";
        char message[1200];
        if (settingsUpdate(configFile, {{"Input", "BUTTONS_FROM_ONE", value}}))
            snprintf(message, sizeof(message), tr("[Input] BUTTONS_FROM_ONE = %s, saved to %s"), value, configFile.c_str());
        else
            snprintf(message, sizeof(message), tr("Cannot write %s"), configFile.c_str());
        buttonsSaved = message;
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("%s", tr("controls.ini keeps SDL's numbers, which start at 0: \"Button 1\" is JOY0_BUTTON_0 there."));
    if (!buttonsSaved.empty())
    {
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextDisabled("%s", buttonsSaved.c_str());
        ImGui::PopTextWrapPos();
    }

    if (inGame)
    {
        header(tr("What the game gets"));
        drawGameInputs();
    }

    header(tr("Force feedback"));
    drawFfbDevice(inGame);
    ImGui::SliderInt(tr("Strength"), &cfg->ffbStrength, 0, 300, "%d %%");
    bool invert = cfg->ffbInvert != 0;
    if (ImGui::Checkbox(tr("Invert the direction"), &invert))
        cfg->ffbInvert = invert;
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextDisabled("%s", tr("Strength of the drive board's forces on the wheel, in percent of the board's full scale. The "
                                 "games ask for about a quarter to a half of it."));
    ImGui::TextDisabled("%s", tr("Check it if the wheel pulls away from the centre or turns the wrong way in a collision."));
    if (!inGame)
        ImGui::TextDisabled("%s", tr("The force feedback starts with the game, on the chosen device or else the first "
                                     "controller's."));
    ImGui::PopTextWrapPos();

    header(tr("Dead zones and options"));
    drawOptions();

    drawMappingPage(inGame);
}

bool controlsCaptureEvent(const SDL_Event *event)
{
    if (!g_capture.active)
        return false;

    if (event->type == SDL_EVENT_KEY_DOWN && event->key.key == SDLK_ESCAPE)
    {
        if (!event->key.repeat)
            g_capture.active = false;
        return true;
    }

    const Device &device = g_mapping.device;
    char token[96] = "";
    int player = 0;
    const std::vector<Row> &rows = g_rows[g_capture.section];
    const bool analog = g_capture.row < rows.size() && isAnalog(genericName(rows[g_capture.row].key, &player));

    switch (event->type)
    {
        case SDL_EVENT_KEY_DOWN:
            if (device.kind != KEYBOARD || event->key.repeat)
                return true;
            snprintf(token, sizeof(token), "KEY_%s", scancodeToKeyName(event->key.scancode)); // KP_PLUS, not "Keypad +"
            break;
        case SDL_EVENT_KEY_UP:
            return true;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            if (device.kind != MOUSE)
                return false; // a click on the page
            if (event->button.button == SDL_BUTTON_LEFT)
                snprintf(token, sizeof(token), "MOUSE_LEFT_BUTTON");
            else if (event->button.button == SDL_BUTTON_RIGHT)
                snprintf(token, sizeof(token), "MOUSE_RIGHT_BUTTON");
            else if (event->button.button == SDL_BUTTON_MIDDLE)
                snprintf(token, sizeof(token), "MOUSE_MIDDLE_BUTTON");
            else
                return true;
            break;
        case SDL_EVENT_MOUSE_BUTTON_UP:
            return device.kind == MOUSE;
        case SDL_EVENT_MOUSE_MOTION:
            if (device.kind != MOUSE)
                return false;
            if (!analog) // the mouse's axes are for the gun's position
                return true;
            g_capture.mouseX += event->motion.xrel;
            g_capture.mouseY += event->motion.yrel;
            if (std::fabs(g_capture.mouseX) > 150.0f)
                snprintf(token, sizeof(token), "MOUSE_AXIS_X");
            else if (std::fabs(g_capture.mouseY) > 150.0f)
                snprintf(token, sizeof(token), "MOUSE_AXIS_Y");
            else
                return true;
            break;
        case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
        {
            int index = getControllerID(event->gbutton.which);
            if (device.kind != GAMEPAD || index != device.index)
                return true;
            snprintf(token, sizeof(token), "GC%d_BUTTON_%s", index, SDL_GetGamepadStringForButton((SDL_GamepadButton)event->gbutton.button));
            break;
        }
        case SDL_EVENT_GAMEPAD_AXIS_MOTION:
        {
            int index = getControllerID(event->gaxis.which);
            if (device.kind != GAMEPAD || index != device.index || event->gaxis.axis >= SDL_GAMEPAD_AXIS_COUNT)
                return true;
            int rest = g_capture.padAxes[index][event->gaxis.axis];
            int delta = (int)event->gaxis.value - rest;
            if (std::abs(delta) < 16000)
                return true;
            // A trigger rests at 0 and reads 0 to 1 as it is; a stick axis taken for a pedal rests in the middle
            bool trigger = event->gaxis.axis == SDL_GAMEPAD_AXIS_LEFT_TRIGGER || event->gaxis.axis == SDL_GAMEPAD_AXIS_RIGHT_TRIGGER;
            const char *pedal = !analog || trigger || rest > 16000 ? "" : pedalSuffix(genericName(rows[g_capture.row].key, &player), rest, delta);
            snprintf(token, sizeof(token), "GC%d_AXIS_%s%s", index, SDL_GetGamepadStringForAxis((SDL_GamepadAxis)event->gaxis.axis),
                     analog ? pedal : (delta > 0 ? "_POSITIVE" : "_NEGATIVE"));
            break;
        }
        case SDL_EVENT_JOYSTICK_BUTTON_DOWN:
        {
            int index = getJoystickID(event->jbutton.which);
            if (device.kind != JOYSTICK || index != device.index || getControllerID(event->jbutton.which) >= 0)
                return true;
            snprintf(token, sizeof(token), "JOY%d_BUTTON_%d", index, event->jbutton.button);
            break;
        }
        case SDL_EVENT_JOYSTICK_AXIS_MOTION:
        {
            int index = getJoystickID(event->jaxis.which);
            if (device.kind != JOYSTICK || index != device.index || getControllerID(event->jaxis.which) >= 0 || event->jaxis.axis >= MAX_JOY_AXES)
                return true;
            int rest = g_capture.joyAxes[index][event->jaxis.axis];
            int delta = (int)event->jaxis.value - rest;
            if (std::abs(delta) < 16000)
                return true;
            snprintf(token, sizeof(token), "JOY%d_AXIS_%d%s", index, event->jaxis.axis,
                     analog ? pedalSuffix(genericName(rows[g_capture.row].key, &player), rest, delta) : (delta > 0 ? "_POSITIVE" : "_NEGATIVE"));
            break;
        }
        case SDL_EVENT_JOYSTICK_HAT_MOTION:
        {
            int index = getJoystickID(event->jhat.which);
            const char *direction = event->jhat.value == SDL_HAT_UP      ? "UP"
                                    : event->jhat.value == SDL_HAT_DOWN  ? "DOWN"
                                    : event->jhat.value == SDL_HAT_LEFT  ? "LEFT"
                                    : event->jhat.value == SDL_HAT_RIGHT ? "RIGHT"
                                                                         : nullptr;
            if (device.kind != JOYSTICK || index != device.index || getControllerID(event->jhat.which) >= 0 || !direction)
                return true;
            snprintf(token, sizeof(token), "JOY%d_HAT%d_%s", index, event->jhat.hat, direction);
            break;
        }
        case SDL_EVENT_GAMEPAD_BUTTON_UP:
        case SDL_EVENT_JOYSTICK_BUTTON_UP:
            return true;
        default:
            return false;
    }

    if (token[0])
        finishCapture(token);
    return true;
}

bool controlsSave(bool inGame, bool *written)
{
    *written = false;
    if (!g_loaded || g_file.empty())
        return true;
    std::vector<IniValue> values;
    for (auto &entry : g_rows)
        for (Row &row : entry.second)
            if (row.dirty)
                values.push_back({entry.first, row.key, row.value});
    for (Row &row : g_configRows)
        if (row.dirty)
            values.push_back({"Config", row.key, row.value});
    if (values.empty())
        return true;
    if (!iniUpdate(g_file, values))
        return false;
    for (auto &entry : g_rows)
        for (Row &row : entry.second)
            row.dirty = false;
    for (Row &row : g_configRows)
        row.dirty = false;
    *written = true;
    if (inGame)
        sdlReloadBindings(g_file.c_str());
    return true;
}
} // namespace gui
