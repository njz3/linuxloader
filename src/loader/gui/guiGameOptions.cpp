// The Tab window's "Game options" page: the options of the game being run (config/gameOptions.c), drawn from their
// table, kept in the [Game.<short name>] section of the settings file that both Save buttons write. Then its boards:
// each emulated, or a real board on a serial port of the PC (hardware/lindbergh/serialPort.c), as windowsloader's.

#include "guiCommon.h"

#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

#include "../../imgui/imgui.h"
#include "../config/config.h"
#include "../config/gameOptions.h"
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
    bool servo = data->emulateDriveboard || group == GROUP_OUTRUN || group == GROUP_OUTRUN_TEST || group == GROUP_HUMMER ||
                 group == GROUP_ID4_EXP || group == GROUP_ID4_JAP || group == GROUP_ID5 || crc == SEGA_RACE_TV_SBPF ||
                 crc == R_TUNED_SBQW;
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
