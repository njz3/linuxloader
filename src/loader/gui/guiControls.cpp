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
        {"Test", "Test"},
        {"Coin", "Coin"},
        {"Start", "Start"},
        {"Service", "Service"},
        {"ExitGame", "Exit the game"},
        {"Steer", "Steering"},
        {"Gas", "Accelerator"},
        {"Brake", "Brake"},
        {"Steer_Left", "Steer left"},
        {"Steer_Right", "Steer right"},
        {"Gas_Digital", "Accelerator (digital)"},
        {"Brake_Digital", "Brake (digital)"},
        {"GearUp", "Gear up"},
        {"GearDown", "Gear down"},
        {"ViewChange", "Change view"},
        {"MusicChange", "Change music"},
        {"Boost", "Boost"},
        {"BoostRight", "Boost right"},
        {"Up", "Up"},
        {"Down", "Down"},
        {"Left", "Left"},
        {"Right", "Right"},
        {"Button1", "Button 1"},
        {"Button2", "Button 2"},
        {"Button3", "Button 3"},
        {"CardInsert", "Insert card"},
        {"Card1Insert", "Insert card 1"},
        {"Card2Insert", "Insert card 2"},
        {"Trigger", "Trigger"},
        {"Reload", "Reload"},
        {"GunButton", "Gun button"},
        {"ActionButton", "Action button"},
        {"PedalLeft", "Left pedal"},
        {"PedalRight", "Right pedal"},
        {"GunX", "Gun X"},
        {"GunY", "Gun Y"},
        {"Flying_X", "Stick X"},
        {"Flying_Y", "Stick Y"},
        {"Flying_Left", "Stick left"},
        {"Flying_Right", "Stick right"},
        {"Flying_Up", "Stick up"},
        {"Flying_Down", "Stick down"},
        {"Throttle", "Throttle"},
        {"Throttle_Accelerate", "Throttle up"},
        {"Throttle_Slowdown", "Throttle down"},
        {"GunTrigger", "Gun trigger"},
        {"MissileTrigger", "Missile trigger"},
        {"ClimaxSwitch", "Climax switch"},
    };
    auto it = labels.find(name);
    return it != labels.end() ? tr(it->second) : name.c_str();
}

// English label of a [Config] option, translated by tr(); nullptr for one shown by its own name
const char *optionLabel(const std::string &key)
{
    static const std::map<std::string, const char *> labels = {
        {"Steer_DeadZone", "Steering"},
        {"Gas_DeadZone", "Accelerator"},
        {"Brake_DeadZone", "Brake"},
        {"FLYING_X_DeadZone", "Stick X"},
        {"FLYING_Y_DeadZone", "Stick Y"},
        {"Throttle_DeadZone", "Throttle"},
        {"GunX_Centering", "Gun X centring"},
        {"GunY_Centering", "Gun Y centring"},
        {"ShakeIncreaseRate", "Screen shake: increase"},
        {"ShakeDecayRate", "Screen shake: decay"},
        {"ShakeMinScreenFraction", "Screen shake: minimum"},
        {"CardInsert_Toggle", "Insert card: toggle"},
        {"Card1Insert_Toggle", "Insert card 1: toggle"},
        {"Card2Insert_Toggle", "Insert card 2: toggle"},
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
        return token.substr(4);
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
        return format(tr("Button %d"), number);
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
        SDL_Scancode scancode = SDL_GetScancodeFromName(token.c_str() + 4);
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

// The raw value, -32768..32767, of the first axis among some inputs: JOYn_AXIS_m or GCn_AXIS_name
bool axisOf(const std::vector<std::string> &tokens, Sint16 *raw)
{
    for (const std::string &token : tokens)
    {
        int device = 0, axis = 0;
        char name[32] = "";
        if (sscanf(token.c_str(), "JOY%d_AXIS_%d", &device, &axis) == 2 && joystickOf(device))
        {
            *raw = SDL_GetJoystickAxis(joystickOf(device), axis);
            return true;
        }
        if (sscanf(token.c_str(), "GC%d_AXIS_%31[^_ ,]", &device, name) == 2 && device >= 0 && device < MAX_JOYSTICKS &&
            sdlJoysticks.controllers[device])
        {
            *raw = SDL_GetGamepadAxis(sdlJoysticks.controllers[device], SDL_GetGamepadAxisFromString(name));
            return true;
        }
    }
    return false;
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

void startCapture(const std::string &section, size_t row)
{
    g_capture.active = true;
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

// The input pressed replaces the device's ones; the other devices' stay
void finishCapture(const std::string &token)
{
    std::vector<Row> &rows = g_rows[g_capture.section];
    if (g_capture.row < rows.size())
    {
        Row &row = rows[g_capture.row];
        std::vector<std::string> tokens = otherTokens(g_mapping.device, row.value);
        tokens.push_back(token);
        setRow(row, join(tokens));
    }
    g_capture.active = false;
}

// The device's bindings back to the defaults of a new controls.ini, in the sections given
void resetDevice(const Device &device, const std::vector<std::string> &sections)
{
    std::error_code error;
    std::filesystem::path temp = std::filesystem::temp_directory_path(error) / "linuxloader-default-controls.ini";
    // iniSave, under it, returns 1 on success and -1 on failure
    if (error || createDefaultControlsIni(temp.string().c_str()) < 0)
        return;
    IniConfig *defaults = iniLoad(temp.string().c_str());
    if (defaults)
    {
        for (const std::string &section : sections)
        {
            for (Row &row : g_rows[section])
            {
                const char *value = iniGetValue(defaults, section.c_str(), row.key.c_str());
                std::vector<std::string> tokens = otherTokens(device, row.value);
                for (const std::string &token : deviceTokens(device, value ? value : ""))
                    tokens.push_back(token);
                setRow(row, join(tokens));
            }
        }
        iniFree(defaults);
    }
    std::filesystem::remove(temp, error);
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
    static const char *labels[kNumSections] = {"All games", "Driving", "Buttons", "Flying", "Shooting", "Mahjong"};
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
        std::string text;
        for (const std::string &token : tokens)
            text += (text.empty() ? "" : ", ") + describe(token);
        if (text.empty())
            ImGui::TextDisabled("-");
        else
            ImGui::TextUnformatted(text.c_str());
        if (!tokens.empty() && ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", join(tokens).c_str());

        ImGui::TableSetColumnIndex(2);
        Sint16 raw = 0;
        if (isAnalog(name) && axisOf(tokens, &raw))
        {
            ImGui::SetNextItemWidth(-FLT_MIN);
            if (name == "Gas" || name == "Brake" || name == "Throttle") // a pedal: from its rest at one end
                ImGui::ProgressBar(((float)raw + 32768.0f) / 65535.0f, ImVec2(-FLT_MIN, ImGui::GetFrameHeight() * 0.6f), "");
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

        ImGui::TableSetColumnIndex(3);
        if (ImGui::Button(tr("Map")))
            startCapture(section, i);
        ImGui::SameLine();
        if (ImGui::Button(tr("Unmap")))
            setRow(row, join(otherTokens(device, row.value)));
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
} // namespace

void setControlsFile(const std::string &path)
{
    if (path != g_file)
    {
        g_file = path;
        g_loaded = false;
    }
}

void drawControlsTab(bool inGame)
{
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
            const bool haptic = (device.kind == GAMEPAD || device.kind == JOYSTICK) && device.index == 0 && sdlJoysticks.haptics[0];
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

    EmulatorConfig *cfg = getConfig();
    header(tr("Force feedback"));
    ImGui::SliderInt(tr("Strength"), &cfg->ffbStrength, 0, 300, "%d %%");
    bool invert = cfg->ffbInvert != 0;
    if (ImGui::Checkbox(tr("Invert the direction"), &invert))
        cfg->ffbInvert = invert;
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextDisabled("%s", tr("Strength of the drive board's forces on the wheel, in percent of the board's full scale. The "
                                 "games ask for about a quarter to a half of it."));
    ImGui::TextDisabled("%s", tr("Check it if the wheel pulls away from the centre or turns the wrong way in a collision."));
    if (!inGame)
        ImGui::TextDisabled("%s", tr("The wheel's force feedback starts with the game, on the first controller."));
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
            snprintf(token, sizeof(token), "KEY_%s", SDL_GetScancodeName(event->key.scancode));
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
            int delta = (int)event->gaxis.value - g_capture.padAxes[index][event->gaxis.axis];
            if (std::abs(delta) < 16000)
                return true;
            snprintf(token, sizeof(token), "GC%d_AXIS_%s%s", index, SDL_GetGamepadStringForAxis((SDL_GamepadAxis)event->gaxis.axis),
                     analog ? "" : (delta > 0 ? "_POSITIVE" : "_NEGATIVE"));
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
            int delta = (int)event->jaxis.value - g_capture.joyAxes[index][event->jaxis.axis];
            if (std::abs(delta) < 16000)
                return true;
            snprintf(token, sizeof(token), "JOY%d_AXIS_%d%s", index, event->jaxis.axis, analog ? "" : (delta > 0 ? "_POSITIVE" : "_NEGATIVE"));
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
