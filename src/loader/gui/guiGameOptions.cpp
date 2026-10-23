// The F10 window's "Game options" page: the options of the game being run (config/gameOptions.c), drawn from their
// table, kept in the [Game.<short name>] section of the settings file that both Save buttons write.

#include "guiCommon.h"

#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

#include "../../imgui/imgui.h"
#include "../config/config.h"
#include "../config/gameOptions.h"

namespace gui
{
void drawGameOptionsTab(const std::string &configFile)
{
    EmulatorConfig *cfg = getConfig();
    const char *section = gameOptionSection();
    ImGui::TextUnformatted(cfg->gameTitle ? cfg->gameTitle : tr("Unknown game"));
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
}
} // namespace gui
