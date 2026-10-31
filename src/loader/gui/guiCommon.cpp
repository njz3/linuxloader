// Shared pieces of the game picker and of the Tab settings overlay.
//
// The interface is modelled on sm2-emu's (https://github.com/dmanlfc/sm2-emu), a Sega Model 2 emulator by Daniel
// Martin (dmanlfc) and the sm2-emu contributors, GPL-3.0-or-later: its dark style, its scaling with the window, its
// picker with a description pane and its tabbed settings. This is a modified work based on sm2-emu's interface;
// the About tab says so, and credits Flycast (https://github.com/flyinghead/flycast), whose controls page the
// Controls tab follows (guiControls.cpp).

#include "guiCommon.h"

#include <glad/gl.h>
#include <SDL3/SDL.h>

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <cstring>
#include <fstream>
#include <memory>
#include <sstream>
#include <unordered_map>
#include <vector>

#include "../../imgui/imgui.h"
#include "../../imgui/backends/imgui_impl_opengl2.h"
#include "../../imgui/backends/imgui_impl_sdl3.h"

#include "gui.h"
#include "../config/config.h"
#include "../redirections/writeDir.h"
#include "../hardware/lindbergh/netOutputs.h"
extern "C"
{
#include "../config/configIni.h"
#include "../graphics/fpsLimiter.h"
}

#ifdef _WIN32
#include <windows.h>
#endif

namespace gui
{
namespace
{
bool g_initialised = false;
float g_uiScale = 0.0f;
ImGuiStyle g_baseStyle;
std::string g_status; // result of the last save, shown under the buttons
std::string g_outputsSaved; // where the network outputs' box wrote its choice

// The list's tooltip: the games patchResolution.c resizes, and their limits
const char *internalResolutionGames = N_(
    "Games whose render size is patched:\n"
    "  OutRun 2 SP SDX: any size above 800x480 (640x480 is the Chihiro mode)\n"
    "  Sega Race TV: any size, with black bars of its own beyond 4:3\n"
    "  R-Tuned: any size\n"
    "  Initial D 4: any size (the service menu stays at 1360x768)\n"
    "  Hummer, Hummer Extreme: any size, TeknoParrot's 2560x1440 in kernel.nut too\n"
    "Patched, not tried here:\n"
    "  After Burner Climax, Harley-Davidson, Rambo, Too Spicy, Virtua Fighter 5,\n"
    "  House of the Dead 4 and EX, Let's Go Jungle: any size\n"
    "  Initial D 5: 1360x768 and above\n"
    "  House of the Dead 4 Special: above 1024x768\n"
    "  Primeval Hunt: above 1280x480\n"
    "  Virtua Tennis 3: above 1360x768\n"
    "  Let's Go Jungle Special: the width chosen, the height follows at 16:9\n"
    "Fixed size: Ghost Squad Evolution, Mahjong Fight Club 4.\n"
    "Other games, Dead Heat included: keep Native.");

// A translation of the interface: lang/<code>.po, gettext's format (msgid: the English text, msgstr: its translation),
// read once and kept until the program ends, as tr() hands out pointers into it
struct Catalog
{
    std::string code; // the file's name: fr, pt_BR
    std::string name; // the list's label: the header's X-Language-Name, else the code
    std::unordered_map<std::string, std::string> texts;
};
std::vector<std::unique_ptr<Catalog>> g_catalogs; // sorted by name
const Catalog *g_catalog = nullptr;               // the language shown, nullptr for English

// The text of a quoted .po string from its first quote, its escapes undone, appended to out
void poUnquote(const std::string &line, std::string &out)
{
    size_t start = line.find('"');
    size_t end = line.rfind('"');
    if (start == std::string::npos || end <= start)
        return;
    for (size_t i = start + 1; i < end; i++)
    {
        char c = line[i];
        if (c == '\\' && i + 1 < end)
        {
            c = line[++i];
            c = c == 'n' ? '\n' : c == 't' ? '\t' : c == 'r' ? '\r' : c;
        }
        out += c;
    }
}

// Reads a .po file: its translated entries, without the fuzzy ones as gettext, and its header's X-Language-Name.
// Contexts and plurals, which the interface does not use, are skipped.
bool loadPo(const std::filesystem::path &file, Catalog &catalog)
{
    std::ifstream in(file, std::ios::binary);
    if (!in)
        return false;
    std::string id, str, skipped, header;
    std::string *current = nullptr; // what a "..." line continues
    bool fuzzy = false, other = false, complete = false; // complete: past its msgstr, the next line may start another
    auto finish = [&]() {
        if (complete && !other)
        {
            if (id.empty())
                header = str;
            else if (!fuzzy && !str.empty())
                catalog.texts[id] = str;
        }
        id.clear();
        str.clear();
        current = nullptr;
        fuzzy = other = complete = false;
    };
    std::string line;
    bool first = true;
    while (std::getline(in, line))
    {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        if (first && line.compare(0, 3, "\xEF\xBB\xBF") == 0)
            line.erase(0, 3);
        first = false;
        if (line.empty())
            continue;
        if (line[0] == '#') // a comment, an obsolete entry (#~) included, starts the next entry
        {
            if (complete)
                finish();
            if (line.compare(0, 2, "#,") == 0 && line.find("fuzzy") != std::string::npos)
                fuzzy = true;
        }
        else if (line.compare(0, 7, "msgctxt") == 0 || line.compare(0, 12, "msgid_plural") == 0 ||
                 line.compare(0, 7, "msgstr[") == 0)
        {
            if (line.compare(0, 7, "msgctxt") == 0 && complete)
                finish();
            other = true;
            complete = complete || line.compare(0, 7, "msgstr[") == 0;
            current = &skipped;
        }
        else if (line.compare(0, 6, "msgid ") == 0)
        {
            if (complete)
                finish();
            current = &id;
            poUnquote(line, id);
        }
        else if (line.compare(0, 7, "msgstr ") == 0)
        {
            complete = true;
            current = &str;
            poUnquote(line, str);
        }
        else if (line[0] == '"' && current)
            poUnquote(line, *current);
    }
    finish();

    std::istringstream fields(header);
    while (std::getline(fields, line))
        if (line.compare(0, 16, "X-Language-Name:") == 0)
        {
            size_t start = line.find_first_not_of(' ', 16);
            catalog.name = start == std::string::npos ? "" : line.substr(start);
        }
    if (catalog.name.empty())
        catalog.name = catalog.code;
    return true;
}

// The lang folder next to the program
std::filesystem::path languageDir()
{
#ifdef _WIN32
    wchar_t path[MAX_PATH];
    DWORD length = GetModuleFileNameW(NULL, path, MAX_PATH);
    return length > 0 && length < MAX_PATH ? std::filesystem::path(path).parent_path() / "lang" : std::filesystem::path();
#else
    const char *base = SDL_GetBasePath();
    return base ? std::filesystem::path(base) / "lang" : std::filesystem::path();
#endif
}

// Every lang/*.po, once
void loadCatalogs()
{
    static bool loaded = false;
    if (loaded)
        return;
    loaded = true;
    std::error_code ec;
    const std::filesystem::path dir = languageDir();
    for (const auto &entry : std::filesystem::directory_iterator(dir, ec))
    {
        if (entry.path().extension() != ".po")
            continue;
        auto catalog = std::make_unique<Catalog>();
        catalog->code = entry.path().stem().string();
        if (loadPo(entry.path(), *catalog))
            g_catalogs.push_back(std::move(catalog));
    }
    std::sort(g_catalogs.begin(), g_catalogs.end(),
              [](const std::unique_ptr<Catalog> &a, const std::unique_ptr<Catalog> &b) { return a->name < b->name; });
}

// The catalog of a code: its own file (pt_BR), else its language's (pt); nullptr when there is none
const Catalog *findCatalog(const std::string &code)
{
    for (const std::string &wanted : {code, code.substr(0, code.find('_'))})
        for (const auto &catalog : g_catalogs)
            if (catalog->code == wanted)
                return catalog.get();
    return nullptr;
}

// The system's language as a .po's name, fr_FR (findCatalog falls back on fr); "" when it is not known
std::string systemLanguage()
{
#ifdef _WIN32
    const LCID locale = MAKELCID(GetUserDefaultUILanguage(), SORT_DEFAULT);
    char language[16] = "", country[16] = "";
    if (!GetLocaleInfoA(locale, LOCALE_SISO639LANGNAME, language, sizeof(language)))
        return "";
    GetLocaleInfoA(locale, LOCALE_SISO3166CTRYNAME, country, sizeof(country));
    return country[0] ? std::string(language) + "_" + country : std::string(language);
#else
    for (const char *variable : {"LC_ALL", "LC_MESSAGES", "LANG"})
    {
        const char *value = getenv(variable);
        if (value && value[0])
        {
            std::string name(value);
            return name.substr(0, name.find_first_of(".@")); // fr_FR.UTF-8 -> fr_FR
        }
    }
    return "";
#endif
}

// LANGUAGE: auto (the system's), en, or a lang/*.po's name
void selectLanguage(const char *language)
{
    loadCatalogs();
    const std::string code = language ? language : "";
    if (code == "en")
        g_catalog = nullptr;
    else if (code.empty() || code == "auto")
        g_catalog = findCatalog(systemLanguage());
    else
        g_catalog = findCatalog(code);
}

// Sets key = value in section of the lines of an ini file, keeping every other line.
void iniSet(std::vector<std::string> &lines, const char *section, const char *key, const std::string &value)
{
    auto trim = [](const std::string &s) {
        size_t b = s.find_first_not_of(" \t\r");
        size_t e = s.find_last_not_of(" \t\r");
        return b == std::string::npos ? std::string() : s.substr(b, e - b + 1);
    };
    auto lower = [](std::string s) {
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char)std::tolower(c); });
        return s;
    };

    const std::string wantedSection = lower(section);
    const std::string wantedKey = lower(key);
    const std::string newLine = std::string(key) + " = " + value;
    bool inSection = false;
    long sectionStart = -1, sectionEnd = -1;
    for (size_t i = 0; i < lines.size(); i++)
    {
        std::string line = trim(lines[i]);
        if (!line.empty() && line[0] == '[')
        {
            if (inSection)
            {
                sectionEnd = (long)i;
                break;
            }
            size_t close = line.find(']');
            inSection = lower(trim(line.substr(1, close == std::string::npos ? std::string::npos : close - 1))) == wantedSection;
            if (inSection)
                sectionStart = (long)i;
            continue;
        }
        if (!inSection || line.empty() || line[0] == '#' || line[0] == ';')
            continue;
        size_t eq = line.find('=');
        if (eq != std::string::npos && lower(trim(line.substr(0, eq))) == wantedKey)
        {
            lines[i] = newLine;
            return;
        }
    }
    if (sectionStart < 0)
    {
        if (!lines.empty() && !trim(lines.back()).empty())
            lines.push_back("");
        lines.push_back(std::string("[") + section + "]");
        lines.push_back(newLine);
        return;
    }
    if (sectionEnd < 0)
        sectionEnd = (long)lines.size();
    // after the last non-empty line of the section
    long at = sectionEnd;
    while (at - 1 > sectionStart && trim(lines[at - 1]).empty())
        at--;
    lines.insert(lines.begin() + at, newLine);
}

// ASPECT_RATIO's names, in ASPECT_* order, and the list Tab offers (4:3_LETTERBOX shown as 4:3)
const char *aspectNames[] = {"ORIGINAL", "4:3", "16:9", "4:3_LETTERBOX", "4:3_STRETCH", "16:9_STRETCH", "4:3_CROP", "16:9_CROP"};
const int aspectChoices[] = {ASPECT_ORIGINAL,     ASPECT_4_3,      ASPECT_16_9,     ASPECT_4_3_STRETCH,
                             ASPECT_16_9_STRETCH, ASPECT_4_3_CROP, ASPECT_16_9_CROP};

// The internal resolutions offered: the game computes and draws its frame at this size when it can be patched to
struct Resolution
{
    int width, height;
};
const Resolution internalResolutions[] = {{640, 480},  {800, 600},   {1024, 768},  {1280, 720},  {1280, 768},
                                          {1360, 768}, {1600, 900},  {1920, 1080}, {2560, 1440}, {3840, 2160}};
const char *displayModeNames[] = {"WINDOW",     "WINDOW_STRETCH", "FULLSCREEN", "FULLSCREEN_STRETCH",
                                  "BORDERLESS", "BORDERLESS_STRETCH"};

int currentDisplayMode()
{
    EmulatorConfig *cfg = getConfig();
    if (cfg->displayMode >= 0)
        return cfg->displayMode;
    return cfg->fullscreen ? DISPLAY_FULLSCREEN : DISPLAY_WINDOW;
}

void restartNote(bool inGame)
{
    if (inGame)
        ImGui::TextDisabled("%s", tr("Takes effect at the next start."));
}

void applyScale()
{
    // Scale with the window as sm2-emu does: against a 992x768 base, the smaller ratio winning. A game window smaller
    // than that (640x480) shrinks the windows down to 80 % of the font's own size on the screen, still readable; the
    // settings then scroll when they do not fit.
    const ImGuiIO &io = ImGui::GetIO();
    const float sx = io.DisplaySize.x > 0.0f ? io.DisplaySize.x / 992.0f : 1.0f;
    const float sy = io.DisplaySize.y > 0.0f ? io.DisplaySize.y / 768.0f : 1.0f;
    const float fb = io.DisplayFramebufferScale.y > 0.0f ? io.DisplayFramebufferScale.y : 1.0f;
    float scale = std::min(sx, sy);
    scale = std::clamp(scale, std::clamp(0.8f / fb, 0.5f, 0.8f), 4.0f);
    if (scale != g_uiScale)
    {
        g_uiScale = scale;
        ImGuiStyle &style = ImGui::GetStyle();
        style = g_baseStyle;
        style.ScaleAllSizes(scale);
        style.FontScaleMain = scale;
    }
}
} // namespace

bool settingsUpdate(const std::string &configFile, const std::vector<IniValue> &values)
{
    if (writeDirRefused()) // the write folder refused: linuxloader.ini only read
        return false;
    // A file not there yet starts as a new linuxloader.ini, with the comments of each setting
    std::error_code error;
    if (!std::filesystem::exists(configFile, error))
        createDefaultIni(configFile.c_str());
    return iniUpdate(configFile, values);
}

bool iniUpdate(const std::string &path, const std::vector<IniValue> &values)
{
    if (writeDirRefused()) // the write folder refused: the settings files only read
        return false;
    std::vector<std::string> lines;
    {
        std::ifstream in(path);
        std::string line;
        while (std::getline(in, line))
        {
            if (!line.empty() && line.back() == '\r')
                line.pop_back();
            lines.push_back(line);
        }
    }
    for (const IniValue &v : values)
        iniSet(lines, v.section.c_str(), v.key.c_str(), v.value);
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out)
        return false;
    for (const std::string &line : lines)
        out << line << "\r\n";
    return (bool)out;
}

void useLanguage(const char *language)
{
    selectLanguage(language);
}

const char *tr(const char *text)
{
    if (!g_catalog)
        return text;
    auto it = g_catalog->texts.find(text);
    return it != g_catalog->texts.end() ? it->second.c_str() : text;
}

bool initContext(SDL_Window *window, const char *language)
{
    if (g_initialised)
        return true;

    selectLanguage(language);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad;
    io.IniFilename = nullptr; // the settings live in the loader's ini

    // sm2-emu's dark style
    ImGui::StyleColorsDark();
    ImGuiStyle &style = ImGui::GetStyle();
    style.WindowRounding = 4.0f;
    style.FrameRounding = 2.0f;
    style.GrabRounding = 2.0f;
    style.WindowBorderSize = 0.0f;
    g_baseStyle = style;
    g_uiScale = 0.0f;

    // A font with the accented letters of the translations (ImGui loads the glyphs a text needs as it is shown)
#ifdef _WIN32
    char fontPath[MAX_PATH];
    const char *windir = getenv("WINDIR");
    snprintf(fontPath, sizeof(fontPath), "%s\\Fonts\\segoeui.ttf", windir ? windir : "C:\\Windows");
    if (SDL_GetPathInfo(fontPath, NULL))
        io.Fonts->AddFontFromFileTTF(fontPath, 17.0f);
#endif

    if (!ImGui_ImplSDL3_InitForOpenGL(window, SDL_GL_GetCurrentContext()) || !ImGui_ImplOpenGL2_Init())
    {
        ImGui::DestroyContext();
        return false;
    }
    g_initialised = true;
    return true;
}

void shutdownContext()
{
    if (!g_initialised)
        return;
    ImGui_ImplOpenGL2_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    g_initialised = false;
}

bool processEvent(const SDL_Event *event)
{
    if (!g_initialised)
        return false;
    return ImGui_ImplSDL3_ProcessEvent(event);
}

void newFrame()
{
    ImGui_ImplOpenGL2_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    applyScale();
    ImGui::NewFrame();
}

void renderFrame(int width, int height)
{
    ImGui::Render();
    glad_glViewport(0, 0, width, height);
    ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());
}

void drawSettings(bool *open, std::string &configFile, bool inGame)
{
    EmulatorConfig *cfg = getConfig();
    const ImGuiViewport *viewport = ImGui::GetMainViewport();
    ImVec2 size(std::min(viewport->WorkSize.x * 0.9f, 660.0f * g_uiScale), std::min(viewport->WorkSize.y * 0.9f, 500.0f * g_uiScale));
    ImGui::SetNextWindowSize(size, ImGuiCond_Appearing);
    // Never larger than the game's window, even one made smaller while the settings are open
    ImGui::SetNextWindowSizeConstraints(ImVec2(0.0f, 0.0f), viewport->WorkSize);
    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + viewport->WorkSize.x * 0.5f, viewport->WorkPos.y + viewport->WorkSize.y * 0.5f),
                            ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    char title[64];
    snprintf(title, sizeof(title), "%s###settings", tr("Settings"));
    if (!ImGui::Begin(title, open, ImGuiWindowFlags_NoCollapse))
    {
        ImGui::End();
        return;
    }
    // Kept inside the game's window: one made smaller after the settings opened left them out of sight
    {
        const ImVec2 pos = ImGui::GetWindowPos(), extent = ImGui::GetWindowSize();
        const ImVec2 inside(std::clamp(pos.x, viewport->WorkPos.x, std::max(viewport->WorkPos.x, viewport->WorkPos.x + viewport->WorkSize.x - extent.x)),
                            std::clamp(pos.y, viewport->WorkPos.y, std::max(viewport->WorkPos.y, viewport->WorkPos.y + viewport->WorkSize.y - extent.y)));
        if (inside.x != pos.x || inside.y != pos.y)
            ImGui::SetWindowPos(inside);
    }

    const float footer = ImGui::GetFrameHeightWithSpacing() * 2.2f;
    // The tabs scroll both ways in a small window; the buttons below stay in sight. Their width is the visible one less
    // a vertical scroll bar, 480 at least: left to ImGui, the texts wrapped at the edge kept the width they had before a
    // vertical scroll bar came, cut at the edge, and a horizontal one came with it
    ImGui::SetNextWindowContentSize(
        ImVec2(std::max(ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ScrollbarSize, 480.0f * g_uiScale), 0.0f));
    ImGui::BeginChild("##tabs", ImVec2(0, -footer), ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar);
    if (ImGui::BeginTabBar("##settingsTabs"))
    {
        if (ImGui::BeginTabItem(tr("Display")))
        {
            // The internal resolution: the frame the game computes and draws, before the aspect ratio and the mode
            {
                const int count = (int)(sizeof(internalResolutions) / sizeof(internalResolutions[0]));
                int current = 0; // AUTO
                for (int i = 0; i < count; i++)
                    if (internalResolutions[i].width == cfg->internalWidth && internalResolutions[i].height == cfg->internalHeight)
                        current = i + 1;
                auto size = [](const char *pattern, int width, int height) {
                    char text[64];
                    snprintf(text, sizeof(text), pattern, width, height);
                    return std::string(text);
                };
                std::string preview = current ? size("%d x %d", cfg->internalWidth, cfg->internalHeight)
                                      : cfg->internalWidth > 0 ? size(tr("%d x %d (custom)"), cfg->internalWidth, cfg->internalHeight)
                                                               : std::string(tr("Native (AUTO)"));
                // The supported games show on the closed list and on each of its entries
                if (ImGui::BeginCombo(tr("Internal resolution"), preview.c_str()))
                {
                    if (ImGui::Selectable(tr("Native (AUTO)"), cfg->internalWidth <= 0))
                        cfg->internalWidth = cfg->internalHeight = 0;
                    for (int i = 0; i < count; i++)
                    {
                        std::string label = size("%d x %d", internalResolutions[i].width, internalResolutions[i].height);
                        if (ImGui::Selectable(label.c_str(), current == i + 1))
                        {
                            cfg->internalWidth = internalResolutions[i].width;
                            cfg->internalHeight = internalResolutions[i].height;
                        }
                        ImGui::SetItemTooltip("%s", tr(internalResolutionGames));
                    }
                    ImGui::EndCombo();
                }
                else
                    ImGui::SetItemTooltip("%s", tr(internalResolutionGames));
                ImGui::PushTextWrapPos(0.0f);
                ImGui::TextDisabled("%s", tr("The size the game computes and draws its frame at, when it can be patched to; then fitted to the aspect ratio and shown by the display mode. Takes effect at the next start."));
                ImGui::TextDisabled("%s", tr("Hover the list for the games it works with."));
                ImGui::PopTextWrapPos();
            }

            const char *aspectLabels[] = {tr("Original"),        tr("4:3, black bars"), tr("16:9, black bars"), tr("4:3, stretched"),
                                          tr("16:9, stretched"), tr("4:3, cropped"),    tr("16:9, cropped")};
            int aspect = 0;
            for (int i = 0; i < IM_ARRAYSIZE(aspectChoices); i++)
                if (aspectChoices[i] == (cfg->aspectRatio == ASPECT_4_3_LETTERBOX ? ASPECT_4_3 : cfg->aspectRatio))
                    aspect = i;
            if (ImGui::Combo(tr("Aspect ratio"), &aspect, aspectLabels, IM_ARRAYSIZE(aspectLabels)))
            {
                cfg->aspectRatio = aspectChoices[aspect];
                if (inGame)
                    sdlApplyDisplaySettings();
            }
            if (cfg->aspectRatio == ASPECT_4_3_CROP || cfg->aspectRatio == ASPECT_16_9_CROP)
            {
                // Read at each frame (blitPresent): the picture follows the sliders
                ImGui::SliderInt(tr("Crop position"), &cfg->cropPosition, -100, 100);
                ImGui::TextDisabled("%s", tr("-100: left or top, 0: the middle, 100: right or bottom."));
                bool barrel = cfg->cropBarrel != 0;
                if (ImGui::Checkbox(tr("Bend the cut parts onto the edges"), &barrel))
                    cfg->cropBarrel = barrel;
                if (barrel)
                {
                    ImGui::SliderInt(tr("Undistorted middle"), &cfg->barrelCenter, 0, 100, "%d %%");
                    ImGui::SliderInt(tr("Curvature radius"), &cfg->barrelRadius, 10, 300, "%d %%");
                    ImGui::PushTextWrapPos(0.0f);
                    // xgettext:no-c-format (shown as it is)
                    ImGui::TextDisabled("%s", tr("The whole picture stays visible: the middle (in % of the frame) as cropped, the parts the crop would cut squeezed onto the edges as on a cylinder seen from the front. A small radius bends hard at the edge, a large one squeezes evenly."));
                    ImGui::PopTextWrapPos();
                }
            }
            ImGui::TextDisabled("%s", tr("Original: the shape of the game's render resolution."));
            ImGui::PushTextWrapPos(0.0f);
            ImGui::TextDisabled("%s", tr("Black bars: the frame's shape, with the whole picture inside at its own shape and black bars where the shapes differ. Stretched: the picture fills the frame, deformed. Cropped: the picture fills the frame at its own shape, cut on the sides (or above and below); the crop position moves the part kept. The display mode then fits the frame to the window or the screen, or stretches it."));
            ImGui::PopTextWrapPos();

            const char *modeLabels[] = {tr("Window, square pixels"), tr("Window, stretched"), tr("Full screen"),
                                        tr("Full screen, stretched"), tr("Borderless fullscreen window"),
                                        tr("Borderless fullscreen window, stretched")};
            int mode = currentDisplayMode();
            if (ImGui::Combo(tr("Display mode"), &mode, modeLabels, IM_ARRAYSIZE(modeLabels)))
            {
                cfg->displayMode = mode;
                cfg->fullscreen = mode >= DISPLAY_FULLSCREEN;
                if (inGame)
                    sdlApplyDisplaySettings();
            }

            // DISPLAY_INDEX: the screens in SDL's order, the game's window opened on one (sdlCalls.c, at its start)
            {
                int count = 0;
                SDL_DisplayID *displays = SDL_GetDisplays(&count);
                auto screenName = [&](int i) {
                    char text[192];
                    if (!displays || i >= count)
                        snprintf(text, sizeof(text), tr("Screen %d (not connected)"), i + 1);
                    else
                    {
                        const char *name = SDL_GetDisplayName(displays[i]);
                        const SDL_DisplayMode *desktop = SDL_GetDesktopDisplayMode(displays[i]);
                        if (desktop)
                            snprintf(text, sizeof(text), "%d: %s (%d x %d)", i + 1, name ? name : "", desktop->w, desktop->h);
                        else
                            snprintf(text, sizeof(text), "%d: %s", i + 1, name ? name : "");
                    }
                    return std::string(text);
                };
                if (ImGui::BeginCombo(tr("Screen"), screenName(std::max(cfg->displayIndex, 0)).c_str()))
                {
                    for (int i = 0; i < count; i++)
                        if (ImGui::Selectable(screenName(i).c_str(), i == cfg->displayIndex))
                            cfg->displayIndex = i;
                    ImGui::EndCombo();
                }
                SDL_free(displays);
                restartNote(inGame);
            }

            // SCALING_FILTER: read at each frame (blitPresent), the picture follows it
            const char *filterLabels[] = {tr("Smooth"), tr("Sharp pixels")};
            int filter = cfg->scalingFilterNearest ? 1 : 0;
            if (ImGui::Combo(tr("Scaling filter"), &filter, filterLabels, IM_ARRAYSIZE(filterLabels)))
                cfg->scalingFilterNearest = filter;
            ImGui::PushTextWrapPos(0.0f);
            ImGui::TextDisabled("%s", tr("Sharp pixels: square and crisp, but uneven unless the picture is enlarged a whole number of times."));
            ImGui::PopTextWrapPos();

            ImGui::Separator();
            bool limiter = cfg->fpsLimiter != 0;
            if (ImGui::Checkbox(tr("Limit the frame rate"), &limiter))
                cfg->fpsLimiter = limiter;
            ImGui::SliderFloat(tr("Target frame rate"), &cfg->fpsTarget, 30.0f, 120.0f, "%.1f");
            restartNote(inGame);
            bool overlay = cfg->fpsOverlayEnabled != 0;
            if (ImGui::Checkbox(tr("Show the frame rate (F12)"), &overlay))
            {
                cfg->fpsOverlayEnabled = overlay;
                if (inGame)
                    fpsOverlayToggleVisibility();
            }
            bool hideCursor = cfg->hideCursor != 0;
            if (ImGui::Checkbox(tr("Hide the mouse pointer"), &hideCursor))
                cfg->hideCursor = hideCursor;
            ImGui::EndTabItem();
        }

        // Force feedback is set in Controls, with the devices
        if (ImGui::BeginTabItem(tr("Controls")))
        {
            drawControlsTab(inGame, configFile);
            ImGui::EndTabItem();
        }

        // The options of the game being run, from its module's table (config/gameOptions.c)
        if (inGame && ImGui::BeginTabItem(tr("Game options")))
        {
            drawGameOptionsTab(configFile);
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem(tr("Outputs and network")))
        {
            if (inGame)
            {
                // The server as it is: unchecked when its port could not be opened
                bool outputs = netOutputsRunning();
                if (ImGui::Checkbox(tr("Send the network outputs (OUTPUTS_NETWORK)"), &outputs))
                    switchOutputs(outputs, configFile, &g_outputsSaved);
                ImGui::PushTextWrapPos(0.0f);
                ImGui::TextDisabled("%s", outputsState().c_str());
                if (!g_outputsSaved.empty())
                    ImGui::TextDisabled("%s", g_outputsSaved.c_str());
                ImGui::PopTextWrapPos();
            }
            else
            {
                bool outputs = cfg->outputsNetwork != 0;
                if (ImGui::Checkbox(tr("Send the network outputs (OUTPUTS_NETWORK)"), &outputs))
                    cfg->outputsNetwork = outputs;
            }
            ImGui::InputInt(tr("TCP port"), &cfg->outputsNetworkPort);
            ImGui::InputInt(tr("UDP announcement port"), &cfg->outputsNetworkUdpPort);
            if (inGame)
            {
                ImGui::Spacing();
                drawOutputsList();
            }
            ImGui::Separator();
            bool patches = cfg->enableNetworkPatches != 0;
            if (ImGui::Checkbox(tr("Network patches"), &patches))
                cfg->enableNetworkPatches = patches;
            ImGui::InputText(tr("Sega Race TV IP address"), cfg->srtvIP, sizeof(cfg->srtvIP));
            restartNote(inGame);
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem(tr("Picker")))
        {
            ImGui::InputText(tr("Games folder"), cfg->gamesDir, sizeof(cfg->gamesDir));
            ImGui::TextDisabled("%s", tr("Empty: the games folder next to the program."));
            // Automatic, English, then the languages of lang/*.po
            std::vector<std::pair<std::string, std::string>> languages = {{"auto", tr("Automatic")}, {"en", "English"}};
            for (const auto &catalog : g_catalogs)
                languages.push_back({catalog->code, catalog->name});
            int language = 0;
            for (int i = 0; i < (int)languages.size(); i++)
                if (languages[i].first == cfg->language)
                    language = i;
            if (ImGui::BeginCombo(tr("Language"), languages[language].second.c_str()))
            {
                for (int i = 0; i < (int)languages.size(); i++)
                    if (ImGui::Selectable(languages[i].second.c_str(), i == language))
                    {
                        snprintf(cfg->language, sizeof(cfg->language), "%s", languages[i].first.c_str());
                        selectLanguage(cfg->language);
                    }
                ImGui::EndCombo();
            }
            ImGui::TextDisabled("%s", tr("The translations: the lang folder's .po files, next to the program."));
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem(tr("About")))
        {
            ImGui::TextUnformatted("Lindbergh Loader");
            ImGui::TextDisabled("Dear ImGui %s", IMGUI_VERSION);
            ImGui::Separator();
            ImGui::PushTextWrapPos(0.0f);
            ImGui::TextUnformatted(tr("Interface based on sm2-emu's, by Daniel Martin (dmanlfc) and the sm2-emu contributors, modified "
                                      "for the Lindbergh loader:"));
            ImGui::PopTextWrapPos();
            ImGui::TextUnformatted("https://github.com/dmanlfc/sm2-emu");
            ImGui::Spacing();
            ImGui::PushTextWrapPos(0.0f);
            ImGui::TextUnformatted(tr("Controls page organised as Flycast's, the Dreamcast, Naomi and Atomiswave emulator:"));
            ImGui::PopTextWrapPos();
            ImGui::TextUnformatted("https://github.com/flyinghead/flycast");
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::EndChild();

    ImGui::Separator();
    if (ImGui::Button(tr("Save")))
    {
        char message[1200];
        bool controlsWritten = false;
        if (!saveSettings(configFile))
            snprintf(message, sizeof(message), tr("Cannot write %s"), configFile.c_str());
        else if (!controlsSave(inGame, &controlsWritten))
            snprintf(message, sizeof(message), tr("Cannot write %s"), "controls.ini");
        else
            snprintf(message, sizeof(message), controlsWritten ? tr("Saved to %s and controls.ini") : tr("Saved to %s"), configFile.c_str());
        g_status = message;
    }
    // The game's own file, <short name>.ini beside the settings file: a copy of it the first time (its other settings
    // kept), then read in its place at the next start (config.c) and by the picker
    const char *shortTitle = getConfig()->gameShortTitle;
    if (inGame && shortTitle && shortTitle[0])
    {
        ImGui::SameLine();
        if (ImGui::Button(tr("Save for this game")))
        {
            namespace fs = std::filesystem;
            fs::path own = fs::path(configFile).parent_path() / (std::string(shortTitle) + ".ini");
            std::error_code error;
            if (!fs::exists(own, error) && fs::exists(configFile, error))
                fs::copy_file(configFile, own, error);
            char message[1200];
            bool controlsWritten = false;
            if (error || !saveSettings(own.string()))
                snprintf(message, sizeof(message), tr("Cannot write %s"), own.string().c_str());
            else
            {
                configFile = own.string(); // the next saves go there too
                controlsSave(inGame, &controlsWritten);
                snprintf(message, sizeof(message), tr("Saved to %s (this game only)"), configFile.c_str());
            }
            g_status = message;
        }
    }
    ImGui::SameLine();
    if (ImGui::Button(tr("Close")))
        *open = false;
    if (!g_status.empty())
    {
        // Beside the buttons when it fits, else on the line under them
        const float right = ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;
        if (ImGui::GetItemRectMax().x + ImGui::GetStyle().ItemSpacing.x + ImGui::CalcTextSize(g_status.c_str()).x <= right)
            ImGui::SameLine();
        ImGui::TextDisabled("%s", g_status.c_str());
    }
    ImGui::End();
}

bool saveSettings(const std::string &configFile)
{
    EmulatorConfig *cfg = getConfig();
    char number[32];
    int mode = currentDisplayMode();
    snprintf(number, sizeof(number), "%.1f", cfg->fpsTarget);
    std::vector<IniValue> values = {
                                     {"Display", "ASPECT_RATIO", aspectNames[std::clamp(cfg->aspectRatio, 0, IM_ARRAYSIZE(aspectNames) - 1)]},
                                     {"Display", "CROP_POSITION", std::to_string(cfg->cropPosition)},
                                     {"Display", "CROP_BARREL", cfg->cropBarrel ? "true" : "false"},
                                     {"Display", "BARREL_CENTER", std::to_string(cfg->barrelCenter)},
                                     {"Display", "BARREL_RADIUS", std::to_string(cfg->barrelRadius)},
                                     {"Display", "WIDTH", cfg->internalWidth > 0 ? std::to_string(cfg->internalWidth) : "AUTO"},
                                     {"Display", "HEIGHT", cfg->internalHeight > 0 ? std::to_string(cfg->internalHeight) : "AUTO"},
                                     {"Display", "DISPLAY_MODE", displayModeNames[std::clamp(mode, 0, IM_ARRAYSIZE(displayModeNames) - 1)]},
                                     {"Display", "FULLSCREEN", mode >= DISPLAY_FULLSCREEN ? "true" : "false"},
                                     {"Display", "DISPLAY_INDEX", std::to_string(std::max(cfg->displayIndex, 0))},
                                     {"Display", "SCALING_FILTER", cfg->scalingFilterNearest ? "NEAREST" : "LINEAR"},
                                     {"Display", "FPS_LIMITER_ENABLED", cfg->fpsLimiter ? "true" : "false"},
                                     {"Display", "FPS_TARGET", number},
                                     {"Display", "FPS_OVERLAY_ENABLED", cfg->fpsOverlayEnabled ? "true" : "false"},
                                     {"Display", "HIDE_CURSOR", cfg->hideCursor ? "true" : "false"},
                                     {"Input", "BUTTONS_FROM_ONE", cfg->buttonsFromOne ? "true" : "false"},
                                     {"ForceFeedback", "FFB_STRENGTH", std::to_string(cfg->ffbStrength)},
                                     {"ForceFeedback", "FFB_INVERT", cfg->ffbInvert ? "true" : "false"},
                                     {"ForceFeedback", "FFB_DEVICE", cfg->ffbDevice},
                                     {"Outputs", "OUTPUTS_NETWORK", cfg->outputsNetwork ? "true" : "false"},
                                     {"Outputs", "OUTPUTS_NETWORK_PORT", std::to_string(cfg->outputsNetworkPort)},
                                     {"Outputs", "OUTPUTS_NETWORK_UDP_PORT", std::to_string(cfg->outputsNetworkUdpPort)},
                                     {"Network", "ENABLE_NETWORK_PATCHES", cfg->enableNetworkPatches ? "true" : "false"},
                                     {"Network", "SRTV_IPADDRESS", cfg->srtvIP},
                                     {"Launcher", "GAMES_DIR", cfg->gamesDir},
                                     {"Launcher", "LANGUAGE", cfg->language},
                                 };
    gameOptionsIniValues(values); // [Game.<short name>] of the game being run, none in the picker
    return settingsUpdate(configFile, values);
}

bool switchOutputs(bool on, const std::string &configFile, std::string *saved)
{
    bool started = netOutputsEnable(on);
    getConfig()->outputsNetwork = on; // the choice, kept even when the port could not be opened
    const char *value = on ? "true" : "false";
    char message[1200];
    if (settingsUpdate(configFile, {{"Outputs", "OUTPUTS_NETWORK", value}}))
        snprintf(message, sizeof(message), tr("[Outputs] OUTPUTS_NETWORK = %s, saved to %s"), value, configFile.c_str());
    else
        snprintf(message, sizeof(message), tr("Cannot write %s"), configFile.c_str());
    *saved = message;
    return started;
}

std::string outputsState()
{
    NetOutputsStatus status;
    netOutputsGetStatus(&status);
    char text[512];
    if (status.running)
        snprintf(text, sizeof(text), tr("Network outputs on: TCP port %d, %d client(s) connected, start announced to 127.0.0.1:%d (UDP)."),
                 status.tcpPort, status.clients, status.udpPort);
    else if (status.portInUse)
        snprintf(text, sizeof(text), tr("Network outputs off: TCP port %d is already taken by another program."), status.tcpPort);
    else if (status.error)
        snprintf(text, sizeof(text), tr("Network outputs off: TCP port %d could not be opened (error %d)."), status.tcpPort, status.error);
    else
        snprintf(text, sizeof(text), "%s", tr("Network outputs off. The values below are the ones the game gives."));
    return text;
}
} // namespace gui
