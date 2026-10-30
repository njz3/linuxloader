#pragma once

// Shared pieces of the picker and of the Tab overlay: the ImGui context, the style, the translations and the
// settings window.

#include <string>
#include <vector>

struct SDL_Window;
union SDL_Event;
struct GameData;

namespace gui
{
// ImGui context with the SDL3 platform backend and the OpenGL2 renderer, for the current GL context.
bool initContext(SDL_Window *window, const char *language);
void shutdownContext();
bool processEvent(const SDL_Event *event);
void newFrame();
void renderFrame(int width, int height);

// The text in the interface's language: the English text itself, or its translation in lang/<language>.po (LANGUAGE
// = auto: the system's language). The pointer stays valid until the program ends.
const char *tr(const char *text);
// A text tr() translates later, marked for xgettext (scripts/i18n/update-translations.sh)
#ifndef N_
#define N_(text) text
#endif

// The settings window. inGame: settings that need a restart are marked so, display settings apply live. configFile
// becomes the game's own file once "Save for this game" wrote it.
void drawSettings(bool *open, std::string &configFile, bool inGame);
// Writes the settings to configFile, keeping its other lines and comments.
bool saveSettings(const std::string &configFile);

// Sets section/key = value in an ini file, keeping its other lines and comments.
struct IniValue
{
    std::string section;
    std::string key;
    std::string value;
};
bool iniUpdate(const std::string &path, const std::vector<IniValue> &values);

// The "Game options" page: the options of the game being run (config/gameOptions.c), in its [Game.<short name>]
// section of configFile; and their values for saveSettings
void drawGameOptionsTab(const std::string &configFile);
void gameOptionsIniValues(std::vector<IniValue> &values);
// A game's boards, translated, for its description: the controls' I/O board, the motor board (the same wording as
// windowsloader's)
std::vector<std::string> boardDescription(const GameData *data);
// The "Outputs and network" page's list of what the game being run can send on the network outputs, with the values
// the game gave last
void drawOutputsList();
// [Outputs] OUTPUTS_NETWORK while the game runs: the server started or stopped at once, the choice written at once to
// configFile, *saved telling where. The settings' box and the agent's "menu outputs" (guiOutputsSwitch). False when
// the server did not start
bool switchOutputs(bool on, const std::string &configFile, std::string *saved);
// The network outputs' state, in a sentence
std::string outputsState();

// The controls page: the bindings of controls.ini, captured from the devices.
void setControlsFile(const std::string &path);
void drawControlsTab(bool inGame, const std::string &configFile);
// While a binding is being captured, the input that sets it: true when the event is taken.
bool controlsCaptureEvent(const SDL_Event *event);
// Writes the changed bindings; in game they apply at once. False when nothing could be written.
bool controlsSave(bool inGame, bool *written);
} // namespace gui
