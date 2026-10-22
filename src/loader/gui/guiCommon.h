#pragma once

// Shared pieces of the picker and of the F10 overlay: the ImGui context, the style, the translations and the
// settings window.

#include <string>
#include <vector>

struct SDL_Window;
union SDL_Event;

namespace gui
{
// ImGui context with the SDL3 platform backend and the OpenGL2 renderer, for the current GL context.
bool initContext(SDL_Window *window, const char *language);
void shutdownContext();
bool processEvent(const SDL_Event *event);
void newFrame();
void renderFrame(int width, int height);

// The text in the interface's language: English by default, French when LANGUAGE is fr (or auto on a French
// Windows).
const char *tr(const char *text);

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

// The controls page: the bindings of controls.ini, captured from the devices.
void setControlsFile(const std::string &path);
void drawControlsTab(bool inGame);
// While a binding is being captured, the input that sets it: true when the event is taken.
bool controlsCaptureEvent(const SDL_Event *event);
// Writes the changed bindings; in game they apply at once. False when nothing could be written.
bool controlsSave(bool inGame, bool *written);
} // namespace gui
