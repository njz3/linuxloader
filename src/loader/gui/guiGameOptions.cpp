// The Tab window's "Game options" page: the options of the game being run (config/gameOptions.c), drawn from their
// table, kept in the [Game.<short name>] section of the settings file that both Save buttons write. Then its boards:
// each emulated, or a real board on a serial port of the PC (hardware/lindbergh/serialPort.c), as windowsloader's.
// Also the "Outputs and network" page's list of what the game can send on the network outputs.

#include "guiCommon.h"

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

#include "../../imgui/imgui.h"
#include "../config/config.h"
#include "../config/gameOptions.h"
#include "../hardware/lindbergh/netOutputs.h"
#include "../hardware/lindbergh/serialPort.h"

extern "C"
{
#include "../config/gameData.h"
}

namespace gui
{
// The serial ports of the PC, listed again every two seconds (an adapter plugged in while the page is open)
static const std::vector<std::string> &serialPorts()
{
    static std::vector<std::string> ports;
    static double listed = -10.0;
    if (ImGui::GetTime() - listed > 2.0)
    {
        char names[64][SERIAL_PORT_NAME];
        int count = serialPortList(names, 64);
        ports.assign(names, names + count);
        listed = ImGui::GetTime();
    }
    return ports;
}

static std::string realBoard(const std::string &port)
{
    char text[160];
    snprintf(text, sizeof(text), tr("Real board on %s"), port.c_str());
    return text;
}

// One board's list: emulated, or a real board on one of the ports (and the one it is set to, not there now)
static void drawBoard(const GameBoard *board)
{
    bool emulated = gameBoardEmulated(board);
    char name[SERIAL_PORT_NAME];
    std::string current = serialPortName(gameBoardPath(board), name, sizeof(name)) ? name : gameBoardPath(board);
    const std::vector<std::string> &ports = serialPorts();
    bool present = false;
    for (const std::string &port : ports)
        present = present || port == current;
    std::string missing = realBoard(current) + " " + tr("(not found)");
    std::string preview = emulated ? tr("Emulated") : present ? realBoard(current) : missing;
    std::string label = std::string(tr(board->label)) + " (" + tr(board->gamePort) + ") " + tr("(at the next start)");
    ImGui::TextUnformatted(label.c_str());
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::BeginCombo("##board", preview.c_str()))
    {
        if (ImGui::Selectable(tr("Emulated"), emulated))
            gameBoardSet(board, NULL);
        for (const std::string &port : ports)
            if (ImGui::Selectable(realBoard(port).c_str(), !emulated && port == current))
                gameBoardSet(board, port.c_str());
        if (!emulated && !present)
            ImGui::Selectable(missing.c_str(), true);
        ImGui::EndCombo();
    }
}

// The Lindbergh games whose frames go to a Sega-like servo board: the emulated drive board, or the game's own
// writes (patch.c)
static bool servoBoard(const GameData *data)
{
    int group = data->gameGroup;
    return data->emulateDriveboard || group == GROUP_OUTRUN || group == GROUP_OUTRUN_TEST || group == GROUP_HUMMER ||
           group == GROUP_ID4_EXP || group == GROUP_ID4_JAP || group == GROUP_ID5 || data->crc32 == SEGA_RACE_TV_SBPF ||
           data->crc32 == R_TUNED_SBQW;
}

std::vector<std::string> boardDescription(const GameData *data)
{
    std::vector<std::string> lines;
    if (!data)
        return lines;
    int group = data->gameGroup;
    uint32_t crc = data->crc32;
    if (group == GROUP_DEAD_HEAT) // hardware/namco/deadHeatIo.c
    {
        lines.push_back(tr("Controls on Namco N2 I/O board (n2jvio, serial)"));
        lines.push_back(crc == NIRIN ? tr("Motor board: none") : tr("Motor board: Namco Kickback board (serial)"));
        return lines;
    }
    if (group == GROUP_GAELCO) // hardware/gaelco/gaelcoIo.c
    {
        lines.push_back(tr("Controls on Gaelco interface board"));
        lines.push_back(tr("Motor board: Gaelco interface board's motor and vibrator outputs"));
        return lines;
    }
    // The Lindbergh games: the JVS I/O board through the base board (hardware/lindbergh/jvs.c), the serial boards
    char text[200];
    snprintf(text, sizeof(text), "%s, %s, JVS 3.0 (%s)", tr("Controls on JVS board"),
             data->jvsIOType == SEGA_TYPE_1 ? "Sega 837-13551" : "Sega 837-14572", tr("base board"));
    lines.push_back(text);
    bool servo = servoBoard(data);
    if (servo)
        lines.push_back(std::string(tr("Motor board: Sega-like servoboard")) + " (/dev/ttyS0)");
    if (data->emulateMotionboard)
        lines.push_back(std::string(tr("Motion board: Sega motion board")) + " (/dev/ttyS1)");
    if (data->emulateRideboard)
        lines.push_back(std::string(tr("Ride board: Sega ride board")) + " (/dev/ttyS0)");
    if (!servo && !data->emulateMotionboard && !data->emulateRideboard)
        lines.push_back(tr("Motor board: none"));
    return lines;
}

void drawGameOptionsTab(const std::string &configFile)
{
    EmulatorConfig *cfg = getConfig();
    const char *section = gameOptionSection();
    ImGui::TextUnformatted(cfg->gameTitle ? cfg->gameTitle : tr("Unknown game"));
    for (const std::string &line : boardDescription(getGameData(cfg->crc32)))
        ImGui::TextDisabled("%s", line.c_str());
    if (section[0])
    {
        std::string file = std::filesystem::path(configFile).filename().string();
        ImGui::TextDisabled(tr("In [%s] of %s, for this game only."), section, file.c_str());
    }
    ImGui::Separator();

    bool any = false, waiting = false;
    for (int i = 0; section[0] && i < gameOptionCount; i++)
    {
        const GameOption *option = &gameOptionTable[i];
        if (!option->applies())
            continue;
        any = true;
        ImGui::PushID(i);
        std::string label = tr(option->label);
        if (option->nextStart)
            label = label + " " + tr("(at the next start)");
        int value = gameOptionValue(option);
        if (option->kind == GAME_OPTION_SWITCH)
        {
            bool on = value != 0;
            if (ImGui::Checkbox(label.c_str(), &on))
                gameOptionSetValue(option, on ? 1 : 0);
        }
        else if (option->kind == GAME_OPTION_CHOICE)
        {
            const char *preview = value >= 0 && value <= option->max && option->valueLabels[value] ? tr(option->valueLabels[value]) : "?";
            if (ImGui::BeginCombo(label.c_str(), preview))
            {
                for (int v = 0; option->values[v]; v++)
                    if (ImGui::Selectable(tr(option->valueLabels[v]), v == value))
                        gameOptionSetValue(option, v);
                ImGui::EndCombo();
            }
        }
        else if (ImGui::SliderInt(label.c_str(), &value, option->min, option->max, "%d"))
            gameOptionSetValue(option, value);
        if (option->help[0])
        {
            ImGui::PushTextWrapPos(0.0f);
            ImGui::TextDisabled("%s", tr(option->help));
            ImGui::PopTextWrapPos();
        }
        if (option->nextStart && gameOptionValue(option) != gameOptionLaunchValue(option))
            waiting = true;
        ImGui::PopID();
        ImGui::Spacing();
    }
    if (!any)
        ImGui::TextDisabled("%s", tr("No options for this game."));

    bool boards = false;
    for (int i = 0; section[0] && i < gameBoardCount; i++)
    {
        const GameBoard *board = &gameBoardTable[i];
        if (!board->applies())
            continue;
        if (!boards)
        {
            ImGui::Separator();
            ImGui::TextUnformatted(tr("Boards"));
            boards = true;
        }
        ImGui::PushID(1000 + i);
        drawBoard(board);
        ImGui::PopID();
        waiting = waiting || gameBoardChanged(board);
    }
    if (boards)
    {
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextDisabled("%s", tr("A real board: the game talks to the cabinet's board on that serial port of the PC "
                                     "(a USB adapter, RS-232 for the drive board, RS-485 for JVS) instead of the "
                                     "loader's emulation."));
        ImGui::PopTextWrapPos();
    }
    if (waiting)
    {
        ImGui::Separator();
        ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.3f, 1.0f), "%s", tr("Changed options take effect at the next start, once saved."));
    }
}

// --- What the game can send on the network outputs (hardware/lindbergh/netOutputs.c): lamp n is bit 7 - n % 8 of the
// RawLamps byte n / 8, as the JVS output n + 1

static std::string formatText(const char *pattern, ...)
{
    char text[256];
    va_list args;
    va_start(args, pattern);
    vsnprintf(text, sizeof(text), pattern, args);
    va_end(args);
    return text;
}

struct GameLamp
{
    int index;
    std::string meaning;
};

// A Gaelco byte of bits, an output of the interface board or a port of Tokyo Cop's: each bit, with its use when known
static void gaelcoByte(std::vector<GameLamp> &lamps, int byte, const std::string &source, const char *const uses[8])
{
    for (int bit = 7; bit >= 0; bit--)
    {
        std::string where = formatText(tr("%s, bit %d"), source.c_str(), bit);
        lamps.push_back({byte * 8 + 7 - bit, uses && uses[bit] ? std::string(tr(uses[bit])) + " (" + where + ")" : where});
    }
}

// The lamps of the game's board, as the boards' code sends them
static std::vector<GameLamp> gameLamps(const GameData *data)
{
    std::vector<GameLamp> lamps;
    if (data->gameGroup == GROUP_DEAD_HEAT) // the Kickback board's (hardware/namco/deadHeatIo.c), as the game names them
    {
        static const char *const deadHeat[] = {"View", "Nitro", "PinKey", "SheetKey", "RingTopKey", "RingCenterKey",
                                               "RingBottomKey"};
        static const char *const maximumTune[] = {"View", "Intrude"};
        uint32_t crc = data->crc32;
        if (crc == WANGAN_MIDNIGHT_MT3 || crc == WANGAN_MIDNIGHT_MT3DX)
            for (int lamp = 0; lamp < 2; lamp++)
                lamps.push_back({lamp, formatText(tr("Kickback board lamp %s"), maximumTune[lamp])});
        else if (crc == DEAD_HEAT_RIDERS)
            for (int lamp = 0; lamp < 8; lamp++)
                lamps.push_back({lamp, lamp < 2 ? formatText(tr("Kickback board lamp %s"), deadHeat[lamp])
                                                : formatText(tr("Kickback board lamp %d"), lamp + 1)});
        else if (crc != NIRIN)
            for (int lamp = 0; lamp < 7; lamp++)
                lamps.push_back({lamp, formatText(tr("Kickback board lamp %s"), deadHeat[lamp])});
        return lamps;
    }
    // Gaelco's bytes of bits, RawLamps and on (hardware/gaelco/gaelcoIo.c): bit 0 first in the uses
    static const char *const lamp = "Cabinet lamp", *const meter = "Coin meter", *const mute = "Sound mute";
    if (data->crc32 == TOKYO_COP) // ports 0x40 and 0x41
    {
        static const char *const port40[8] = {NULL, NULL, NULL, NULL, "Platform", "Platform", "Platform", "Platform"};
        static const char *const port41[8] = {NULL, NULL, lamp, lamp, NULL, meter, NULL, "Siren button lamp"};
        gaelcoByte(lamps, 0, formatText(tr("port 0x%X"), 0x40), port40);
        gaelcoByte(lamps, 1, formatText(tr("port 0x%X"), 0x41), port41);
        return lamps;
    }
    if (data->crc32 == RING_RIDERS) // the interface board's outputs 0 to 2
    {
        static const char *const output0[8] = {lamp, lamp, lamp, lamp, NULL, "Platform security", NULL, NULL};
        static const char *const output1[8] = {mute, NULL, lamp, lamp, NULL, meter, NULL, lamp};
        for (int output = 0; output < 3; output++)
            gaelcoByte(lamps, output, formatText(tr("interface board output %d"), output),
                       output == 0 ? output0 : output == 1 ? output1 : NULL);
        return lamps;
    }
    if (data->gameGroup == GROUP_GAELCO) // Championship Tuning Race: output 0
    {
        static const char *const output0[8] = {meter, lamp, lamp, mute, NULL, NULL, NULL, NULL};
        gaelcoByte(lamps, 0, formatText(tr("interface board output %d"), 0), output0);
        return lamps;
    }
    int outputs = getConfig()->jvsIOType == SEGA_TYPE_1 ? 6 : 20; // jvs.c's boards
    for (int output = 0; output < outputs; output++)
        lamps.push_back({output, formatText(tr("JVS output %d"), output + 1)});
    return lamps;
}

struct OutputLine
{
    std::string name, type, meaning, value;
};

void drawOutputsList()
{
    EmulatorConfig *cfg = getConfig();
    const GameData *data = getGameData(cfg->crc32);
    if (!data)
        return;
    bool running = netOutputsRunning();
    auto value = [running](int v) { return running && v >= 0 ? std::to_string(v) : std::string("-"); };

    std::vector<OutputLine> lines;
    const char *name = cfg->gameShortTitle && cfg->gameShortTitle[0] ? cfg->gameShortTitle : "lindbergh";
    lines.push_back({"mame_start", tr("text"), tr("The game's name, the first line a client gets"), name});
    std::vector<GameLamp> lamps = gameLamps(data);
    int bytes = 0;
    for (const GameLamp &lamp : lamps)
        bytes = std::max(bytes, lamp.index / 8 + 1);
    for (int byte = 0; byte < bytes; byte++)
    {
        int raw = netOutputsGpo(byte);
        lines.push_back({byte ? formatText("RawLamps%d", byte) : std::string("RawLamps"), tr("byte, 0 to 255"),
                         formatText(tr("Lamps %d to %d, lamp %d on bit 7"), byte * 8, byte * 8 + 7, byte * 8), value(raw)});
        for (const GameLamp &lamp : lamps)
            if (lamp.index / 8 == byte)
                lines.push_back({formatText("lamp%d", lamp.index), tr("0 or 1"), lamp.meaning,
                                 value(raw < 0 ? -1 : (raw >> (7 - lamp.index % 8)) & 1)});
    }
    bool drive = data->gameGroup == GROUP_GAELCO || (data->gameGroup == GROUP_DEAD_HEAT && data->crc32 != NIRIN) ||
                 (data->gameGroup != GROUP_DEAD_HEAT && servoBoard(data));
    if (drive)
        lines.push_back({"RawDrive", tr("byte, 0 to 255, one line each"),
                         tr("The motor board's bytes, as the Sega servo board's frames: command, two values, checksum"),
                         value(netOutputsLastDrive())});
    lines.push_back({"mame_stop", "1", tr("When the game exits"), "-"});

    ImGui::TextUnformatted(tr("What this game sends"));
    float height = ImGui::GetTextLineHeightWithSpacing() * 10.5f;
    ImGuiTableFlags flags = ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter |
                            ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_Resizable;
    if (ImGui::BeginTable("##outputs", 4, flags, ImVec2(0.0f, height)))
    {
        ImGui::TableSetupScrollFreeze(0, 1); // the header stays
        ImGui::TableSetupColumn(tr("Name"), ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn(tr("Type"), ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn(tr("Meaning"), ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn(tr("Value"), ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableHeadersRow();
        for (const OutputLine &line : lines)
        {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(line.name.c_str());
            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(line.type.c_str());
            ImGui::TableSetColumnIndex(2);
            ImGui::TextUnformatted(line.meaning.c_str());
            ImGui::SetItemTooltip("%s", line.meaning.c_str()); // whole, when the column cuts it
            ImGui::TableSetColumnIndex(3);
            ImGui::TextUnformatted(line.value.c_str());
        }
        ImGui::EndTable();
    }
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextDisabled("%s", tr("Each line goes as \"name = value\" on the TCP port, at each change; a client that "
                                 "connects gets every known value first. Value: the last sent."));
    if (!running)
        ImGui::TextDisabled("%s", tr("The values show while the outputs are on (from the next start once checked and saved)."));
    ImGui::PopTextWrapPos();
}

void gameOptionsIniValues(std::vector<IniValue> &values)
{
    const char *section = gameOptionSection();
    for (int i = 0; section[0] && i < gameOptionCount; i++)
    {
        const GameOption *option = &gameOptionTable[i];
        if (!option->applies())
            continue;
        char text[64];
        gameOptionFormat(option, text, sizeof(text));
        values.push_back({section, option->key, text});
    }
    for (int i = 0; section[0] && i < gameBoardCount; i++)
    {
        const GameBoard *board = &gameBoardTable[i];
        if (!board->applies())
            continue;
        values.push_back({section, board->emulateKey, gameBoardEmulated(board) ? "true" : "false"});
        values.push_back({section, board->pathKey, gameBoardPath(board)});
    }
}
} // namespace gui
