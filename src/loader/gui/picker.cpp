// The game picker, shown when linuxloader starts without a game: the games of the games folder as tiles, a
// description pane, Enter to launch. Laid out as sm2-emu's picker (https://github.com/dmanlfc/sm2-emu); see
// guiCommon.cpp. A game runs in a process of its own (the ELF is loaded at fixed addresses), and the picker comes
// back when it ends.

#include <glad/gl.h>
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

#include "../../imgui/imgui.h"

#include "gui.h"
#include "guiCommon.h"
#include "../config/config.h"
#include "../mainShared.h"

extern "C"
{
#include "../config/gameData.h"
#include "../config/iniParser.h"
#include "../hardware/lindbergh/jvs.h"
    extern char *games[]; // known ELF names, "END" last (mainShared.c)
}
#include "../input/sdlInput.h"

extern "C" SDLControllers sdlJoysticks;

#ifdef _WIN32
#include <windows.h>
#endif

namespace fs = std::filesystem;

namespace
{
struct GameEntry
{
    std::string folder; // the game's folder
    std::string elf;    // its program
    std::string config; // the ini passed with -c, empty for the default one
    std::string title;
    std::string shortTitle;
    std::string year;
    std::string dvp;
    std::string gameId;
    std::string resolution;
    int status = -1;
    GLuint art = 0;
    float artW = 0.0f, artH = 0.0f;
    bool artTried = false;
};

// Compared as paths: filename().string() converts to the ANSI code page, and MSVC's throws on a name it cannot hold
// (Initial D 4's data folder has some)
bool isKnownElfName(const fs::path &name)
{
    for (int i = 0; strcmp(games[i], "END") != 0; i++)
        if (name == fs::path(games[i]))
            return true;
    return false;
}

// The partial CRC the loader identifies a game by: 0x4000 bytes from offset 10 of the loaded image, which starts
// with the file's first bytes.
uint32_t elfCrc(const fs::path &path)
{
    std::vector<uint8_t> buffer(10 + 0x4000);
    FILE *f = fopen(path.string().c_str(), "rb");
    if (!f)
        return 0;
    size_t n = fread(buffer.data(), 1, buffer.size(), f);
    fclose(f);
    if (n < buffer.size() || memcmp(buffer.data(), "\x7f" "ELF", 4) != 0)
        return 0;
    return getCrc32Mem(buffer.data() + 10, 0x4000);
}

std::string firstExisting(const std::vector<fs::path> &candidates)
{
    std::error_code ec;
    for (const fs::path &p : candidates)
        if (!p.empty() && fs::is_regular_file(p, ec))
            return p.string();
    return "";
}

// One folder of the games folder: false when it holds no known program
bool scanGame(const fs::directory_entry &dir, const fs::path &programDir, GameEntry &entry)
{
    std::error_code ec;
    if (!dir.is_directory(ec))
        return false;

    // The game's programs, shallowest first; a known version wins over an unknown one
    std::vector<fs::path> programs;
    for (auto it = fs::recursive_directory_iterator(dir.path(), fs::directory_options::skip_permission_denied, ec);
         it != fs::recursive_directory_iterator(); it.increment(ec))
    {
        if (ec)
            break;
        if (it.depth() > 3)
        {
            it.disable_recursion_pending();
            continue;
        }
        if (it->is_regular_file(ec) && isKnownElfName(it->path().filename()))
            programs.push_back(it->path());
    }
    std::sort(programs.begin(), programs.end(), [](const fs::path &a, const fs::path &b) {
        auto depth = [](const fs::path &p) { return std::distance(p.begin(), p.end()); };
        return depth(a) != depth(b) ? depth(a) < depth(b) : a.string() < b.string();
    });
    if (programs.empty())
        return false;

    entry.folder = dir.path().string();
    entry.elf = programs.front().string();
    const GameData *data = nullptr;
    for (const fs::path &program : programs)
    {
        data = getGameData(elfCrc(program));
        if (data)
        {
            entry.elf = program.string();
            break;
        }
    }
    const std::string folderName = dir.path().filename().string();
    if (data)
    {
        entry.title = data->gameTitle;
        entry.shortTitle = data->gameShortTitle;
        entry.year = data->gameReleaseYear;
        entry.dvp = data->gameDVP;
        entry.gameId = data->gameID;
        entry.resolution = data->gameNativeResolutions;
        entry.status = data->gameStatus;
    }
    else
    {
        entry.title = folderName;
    }
    entry.config = firstExisting({dir.path() / "linuxloader.ini",
                                  entry.shortTitle.empty() ? fs::path() : programDir / (entry.shortTitle + ".ini"),
                                  programDir / (folderName + ".ini")});
    return true;
}

std::vector<GameEntry> scanGames(const fs::path &gamesDir, const fs::path &programDir)
{
    std::vector<GameEntry> entries;
    std::error_code ec;
    for (const fs::directory_entry &dir : fs::directory_iterator(gamesDir, ec))
    {
        GameEntry entry;
        try
        {
            if (scanGame(dir, programDir, entry))
                entries.push_back(entry);
        }
        catch (const std::exception &) // a name the ANSI code page cannot hold, with MSVC: that folder is left out
        {
        }
    }
    std::sort(entries.begin(), entries.end(), [](const GameEntry &a, const GameEntry &b) { return a.title < b.title; });
    return entries;
}

// A picture of the game in its folder, if there is one: snap, title or the folder's name, png or jpg
void loadArt(GameEntry &entry)
{
    entry.artTried = true;
    const fs::path folder(entry.folder);
    std::vector<fs::path> candidates;
    for (const char *name : {"snap", "title", "art", "cover"})
        for (const char *ext : {".png", ".jpg"})
            candidates.push_back(folder / (std::string(name) + ext));
    for (const char *ext : {".png", ".jpg"})
        candidates.push_back(folder / (folder.filename().string() + ext));
    std::string path = firstExisting(candidates);
    if (path.empty())
        return;

    SDL_Surface *loaded = IMG_Load(path.c_str());
    if (!loaded)
        return;
    SDL_Surface *rgba = SDL_ConvertSurface(loaded, SDL_PIXELFORMAT_ABGR8888);
    SDL_DestroySurface(loaded);
    if (!rgba)
        return;
    glad_glGenTextures(1, &entry.art);
    glad_glBindTexture(GL_TEXTURE_2D, entry.art);
    glad_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glad_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glad_glPixelStorei(GL_UNPACK_ROW_LENGTH, rgba->pitch / 4);
    glad_glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, rgba->w, rgba->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba->pixels);
    glad_glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    entry.artW = (float)rgba->w;
    entry.artH = (float)rgba->h;
    SDL_DestroySurface(rgba);
}

const char *statusText(int status)
{
    switch (status)
    {
        case WORKING:
            return gui::tr("Working");
        case NOT_WORKING:
            return gui::tr("Not working");
        default:
            return gui::tr("Partially working");
    }
}

// Runs the game in a process of its own and waits for it, keeping the picker's window alive. The game's process starts
// the next one when Test is pressed (the test menu, then the game again): all of them are waited for, in a job.
void launchGame(const GameEntry &entry, const std::string &programPath, const fs::path &programDir, SDL_Window *window)
{
#ifdef _WIN32
    std::string command = "\"" + programPath + "\"";
    if (!entry.config.empty())
        command += " -c \"" + entry.config + "\"";
    command += " \"" + entry.elf + "\"";

    STARTUPINFOA startup = {};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process = {};
    std::vector<char> commandLine(command.begin(), command.end());
    commandLine.push_back('\0');
    if (!CreateProcessA(programPath.c_str(), commandLine.data(), NULL, NULL, FALSE, CREATE_SUSPENDED, NULL, programDir.string().c_str(),
                        &startup, &process))
        return;
    HANDLE job = CreateJobObjectA(NULL, NULL);
    if (job && !AssignProcessToJobObject(job, process.hProcess))
    {
        CloseHandle(job);
        job = NULL;
    }
    ResumeThread(process.hThread);

    SDL_HideWindow(window);
    for (;;)
    {
        if (job)
        {
            JOBOBJECT_BASIC_ACCOUNTING_INFORMATION info = {};
            if (!QueryInformationJobObject(job, JobObjectBasicAccountingInformation, &info, sizeof(info), NULL) || info.ActiveProcesses == 0)
                break;
            SDL_Delay(50);
        }
        else if (WaitForSingleObject(process.hProcess, 50) != WAIT_TIMEOUT)
            break;
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
        }
    }
    if (job)
        CloseHandle(job);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    SDL_ShowWindow(window);
    SDL_RaiseWindow(window);

    // When the game's window closed, the picker's was hidden: a moment later Windows gives the focus to the next
    // window, the picker's console, and refuses it back to a process in the background. Without the keyboard, F9 and
    // Enter would do nothing. The picker takes it, attached to the foreground window's input, until it keeps it.
    HWND hwnd = (HWND)SDL_GetPointerProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);
    int kept = 0;
    for (int i = 0; hwnd && i < 40 && kept < 6; i++)
    {
        HWND foreground = GetForegroundWindow();
        if (foreground == hwnd)
            kept++;
        else
        {
            kept = 0;
            DWORD foregroundThread = GetWindowThreadProcessId(foreground, NULL);
            DWORD thisThread = GetCurrentThreadId();
            AttachThreadInput(foregroundThread, thisThread, TRUE);
            BringWindowToTop(hwnd);
            SetForegroundWindow(hwnd);
            SetFocus(hwnd);
            AttachThreadInput(foregroundThread, thisThread, FALSE);
        }
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
        }
        SDL_Delay(50);
    }

    // The key or button that launched the game was released in the game's window: for ImGui it would still be down,
    // and its repeat would launch the game again.
    ImGui::GetIO().ClearInputKeys();
    ImGui::GetIO().ClearInputMouse();
#else
    (void)entry;
    (void)programPath;
    (void)programDir;
    (void)window;
#endif
}

struct Picker
{
    std::vector<GameEntry> entries;
    std::string gamesDirShown;
    int selected = 0;
    bool scrollToSelected = true;
    bool settingsOpen = false;
    int launch = -1;
    bool quit = false;

    void draw()
    {
        const ImGuiViewport *viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                                       ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.06f, 0.07f, 0.09f, 1.0f));
        if (!ImGui::Begin("##picker", nullptr, flags))
        {
            ImGui::End();
            ImGui::PopStyleColor();
            ImGui::PopStyleVar();
            return;
        }

        ImGui::TextUnformatted(gui::tr("Select a game"));
        const char *hint = gui::tr("Enter: launch   F10: settings   F9: quit");
        ImGui::SameLine(ImGui::GetWindowWidth() - ImGui::CalcTextSize(hint).x - ImGui::GetStyle().WindowPadding.x);
        ImGui::TextDisabled("%s", hint);
        ImGui::Separator();

        if (entries.empty())
        {
            ImGui::Spacing();
            ImGui::PushTextWrapPos(0.0f);
            ImGui::Text(gui::tr("No game was found in %s. Set the games folder in Settings (F10) > Picker."), gamesDirShown.c_str());
            ImGui::PopTextWrapPos();
            ImGui::End();
            ImGui::PopStyleColor();
            ImGui::PopStyleVar();
            return;
        }

        selected = std::clamp(selected, 0, (int)entries.size() - 1);
        const float paneW = std::min(360.0f * ImGui::GetStyle().FontScaleMain, ImGui::GetContentRegionAvail().x * 0.34f);
        const float gridW = ImGui::GetContentRegionAvail().x - paneW - 8.0f;
        const float scale = ImGui::GetStyle().FontScaleMain;
        const ImVec2 tile(180.0f * scale, 135.0f * scale);
        const int columns = std::max(1, (int)(gridW / (tile.x + ImGui::GetStyle().ItemSpacing.x)));
        const int count = (int)entries.size();

        // The picker drives the selection itself, so the pane and the launch follow the highlighted tile
        if (!settingsOpen)
        {
            auto pressed = [](ImGuiKey key) { return ImGui::IsKeyPressed(key, true); };
            int sel = selected;
            if (pressed(ImGuiKey_RightArrow) || pressed(ImGuiKey_GamepadDpadRight))
                ++sel;
            if (pressed(ImGuiKey_LeftArrow) || pressed(ImGuiKey_GamepadDpadLeft))
                --sel;
            if (pressed(ImGuiKey_DownArrow) || pressed(ImGuiKey_GamepadDpadDown))
                sel += columns;
            if (pressed(ImGuiKey_UpArrow) || pressed(ImGuiKey_GamepadDpadUp))
                sel -= columns;
            sel = std::clamp(sel, 0, count - 1);
            if (sel != selected)
            {
                selected = sel;
                scrollToSelected = true;
            }
            if (pressed(ImGuiKey_Enter) || pressed(ImGuiKey_KeypadEnter) || pressed(ImGuiKey_GamepadFaceDown))
                launch = selected;
        }

        ImGui::BeginChild("##grid", ImVec2(gridW, 0.0f), false);
        for (int i = 0; i < count; ++i)
        {
            GameEntry &entry = entries[i];
            if (!entry.artTried)
                loadArt(entry);
            if (i % columns != 0)
                ImGui::SameLine();

            ImGui::PushID(i);
            const bool isSelected = i == selected;
            const ImVec2 p0 = ImGui::GetCursorScreenPos();
            if (ImGui::Button("##tile", tile))
            {
                if (selected == i)
                    launch = i;
                else
                    selected = i;
            }
            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                launch = i;
            if (isSelected && scrollToSelected)
            {
                ImGui::SetScrollHereY(0.5f);
                scrollToSelected = false;
            }

            ImDrawList *draw = ImGui::GetWindowDrawList();
            const ImVec2 p1(p0.x + tile.x, p0.y + tile.y);
            if (entry.art)
            {
                // Letterboxed to the tile
                draw->AddRectFilled(p0, p1, IM_COL32(20, 22, 27, 255));
                const float s = std::min(tile.x / entry.artW, tile.y / entry.artH);
                const ImVec2 d0(p0.x + (tile.x - entry.artW * s) * 0.5f, p0.y + (tile.y - entry.artH * s) * 0.5f);
                draw->AddImage((ImTextureID)(intptr_t)entry.art, d0, ImVec2(d0.x + entry.artW * s, d0.y + entry.artH * s));
            }
            else
            {
                draw->AddRectFilled(p0, p1, IM_COL32(28, 30, 36, 255));
                draw->PushClipRect(p0, p1, true);
                draw->AddText(nullptr, 0.0f, ImVec2(p0.x + 8.0f * scale, p0.y + 8.0f * scale), IM_COL32(200, 205, 215, 255),
                              entry.title.c_str(), nullptr, tile.x - 16.0f * scale);
                if (!entry.year.empty())
                    draw->AddText(ImVec2(p0.x + 8.0f * scale, p1.y - ImGui::GetTextLineHeight() - 8.0f * scale), IM_COL32(130, 136, 150, 255),
                                  entry.year.c_str());
                draw->PopClipRect();
            }
            draw->AddRect(p0, p1, isSelected ? IM_COL32(120, 190, 255, 255) : IM_COL32(60, 64, 74, 255), 0.0f, 0, isSelected ? 3.0f : 1.0f);
            ImGui::PopID();
        }
        ImGui::EndChild();

        // Description pane
        ImGui::SameLine();
        ImGui::BeginChild("##pane", ImVec2(paneW, 0.0f), true);
        const GameEntry &cur = entries[selected];
        ImGui::TextWrapped("%s", cur.title.c_str());
        std::string sub = "SEGA";
        if (!cur.year.empty())
            sub += "  " + cur.year;
        ImGui::TextDisabled("%s", sub.c_str());
        if (!cur.dvp.empty())
            ImGui::TextDisabled("%s  %s", cur.dvp.c_str(), cur.gameId.c_str());
        ImGui::Separator();
        ImGui::PushTextWrapPos(0.0f);
        if (cur.status < 0)
            ImGui::TextDisabled("%s", gui::tr("Unknown version of the game"));
        else
        {
            ImGui::Text("%s : %s", gui::tr("Status"), statusText(cur.status));
            if (!cur.resolution.empty())
                ImGui::Text("%s : %s", gui::tr("Native resolution"), cur.resolution.c_str());
        }
        ImGui::Spacing();
        ImGui::TextDisabled("%s", gui::tr("Folder"));
        ImGui::TextUnformatted(cur.folder.c_str());
        ImGui::TextDisabled("%s", gui::tr("Program"));
        ImGui::TextUnformatted(fs::path(cur.elf).lexically_relative(cur.folder).string().c_str());
        if (!cur.config.empty())
        {
            ImGui::TextDisabled("%s", gui::tr("Configuration"));
            ImGui::TextUnformatted(fs::path(cur.config).filename().string().c_str());
        }
        ImGui::PopTextWrapPos();
        ImGui::EndChild();

        ImGui::End();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();
    }
};

fs::path resolveGamesDir(const fs::path &programDir)
{
    const char *configured = getConfig()->gamesDir;
    std::error_code ec;
    if (configured[0])
    {
        fs::path dir(configured);
        return dir.is_absolute() ? dir : programDir / dir;
    }
    for (const fs::path &candidate : {programDir / "games", programDir.parent_path() / "games", programDir.parent_path().parent_path() / "games"})
        if (fs::is_directory(candidate, ec))
            return candidate;
    return programDir / "games";
}
} // namespace

extern "C" int guiRunPicker(const char *programPath)
{
    const fs::path program = fs::absolute(programPath);
    const fs::path programDir = program.parent_path();
    std::string configFile = (programDir / "linuxloader.ini").string();
    loadConfigFile(configFile.c_str());

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD))
        return 1;
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_Window *window = SDL_CreateWindow("Lindbergh Loader", 1280, 720, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (!window)
        return 1;
    SDL_GLContext context = SDL_GL_CreateContext(window);
    if (!context || !gladLoadGL((GLADloadfunc)SDL_GL_GetProcAddress))
        return 1;
    SDL_GL_SetSwapInterval(1);
    if (!gui::initContext(window, getConfig()->language))
        return 1;
    gui::setControlsFile((programDir / "controls.ini").string());

    // The controllers, for the controls page: numbered in SDL's order (a game numbers them by player)
    int numJoysticks = 0;
    SDL_JoystickID *joysticks = SDL_GetJoysticks(&numJoysticks);
    for (int i = 0; joysticks && i < numJoysticks && i < MAX_JOYSTICKS; i++)
    {
        if (SDL_IsGamepad(joysticks[i]))
            sdlJoysticks.controllers[i] = SDL_OpenGamepad(joysticks[i]);
        else
            sdlJoysticks.joysticks[i] = SDL_OpenJoystick(joysticks[i]);
    }
    SDL_free(joysticks);

    Picker picker;
    fs::path gamesDir = resolveGamesDir(programDir);
    picker.gamesDirShown = gamesDir.string();
    picker.entries = scanGames(gamesDir, programDir);
    std::string lastGamesDir = getConfig()->gamesDir;

    while (!picker.quit)
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            if (picker.settingsOpen && gui::controlsCaptureEvent(&event))
                continue;
            gui::processEvent(&event);
            if (event.type == SDL_EVENT_QUIT || event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED)
                picker.quit = true;
            if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat)
            {
                if (event.key.key == SDLK_F9)
                    picker.quit = true;
                else if (event.key.key == SDLK_F10 || (event.key.key == SDLK_ESCAPE && picker.settingsOpen))
                    picker.settingsOpen = !picker.settingsOpen;
            }
        }

        gui::newFrame();
        picker.draw();
        if (picker.settingsOpen)
            gui::drawSettings(&picker.settingsOpen, configFile, false);
        int width, height;
        SDL_GetWindowSizeInPixels(window, &width, &height);
        glad_glClearColor(0.06f, 0.07f, 0.09f, 1.0f);
        glad_glClear(GL_COLOR_BUFFER_BIT);
        gui::renderFrame(width, height);
        SDL_GL_SwapWindow(window);

        // The games folder changed in the settings: list it again once they are closed
        if (!picker.settingsOpen && lastGamesDir != getConfig()->gamesDir)
        {
            lastGamesDir = getConfig()->gamesDir;
            gamesDir = resolveGamesDir(programDir);
            picker.gamesDirShown = gamesDir.string();
            for (GameEntry &entry : picker.entries)
                if (entry.art)
                    glad_glDeleteTextures(1, &entry.art);
            picker.entries = scanGames(gamesDir, programDir);
        }

        if (picker.launch >= 0)
        {
            launchGame(picker.entries[picker.launch], program.string(), programDir, window);
            picker.launch = -1;
        }
    }

    gui::shutdownContext();
    SDL_GL_DestroyContext(context);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
