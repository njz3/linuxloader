#if defined(_WIN32) || defined(__MINGW32__)
#include <cstdlib>
#include <ctime>
#include <sys/stat.h>

#include <filesystem>
#include <string>
#include <vector>
#include <winsock2.h>
#include <windows.h>
#include <libgen.h>

#include "glad/gl.h"
#include "elfLoader/alsa2sdlBridge.hpp"
#include "elfLoader/segaapiBridge.hpp"
#include "elfLoader/gccBridge.hpp"
#include "elfLoader/ipcBridge.hpp"
#include "elfLoader/elfLoader.hpp"
#include "elfLoader/filesystemBridge.hpp"
#include "elfLoader/libcBridge.hpp"
#include "elfLoader/gccBridge.hpp"
#include "elfLoader/graphicsBridge.hpp"
#include "elfLoader/pthreadBridge.hpp"
#include "elfLoader/regexBridge.hpp"
#include "elfLoader/termiosBridge.hpp"
#include "elfLoader/networkBridge.hpp"
#include "elfLoader/mathBridge.hpp"
#include "elfLoader/symbolResolver.hpp"
#include "elfLoader/pthread/pthreadEmu.hpp"
#include "loader/config/config.h"
#include "log/log.h"
#include "graphics/gpuVendor.h"
#include "init.h"
#include "patching/patch.h"
#include "input/sdlInput.h"
#include "mainShared.h"
#include "gui/gui.h"
#include "agent/agent.h"

std::string g_absoluteElfPath;
extern "C" { uint32_t partialElfCrc = 0; } // read from C (config.c)

extern SDLControllers sdlJoysticks;

extern int gGrp;

extern "C" { HMODULE hGlDLL = NULL; } // read from C (gpuVendor.c)

extern "C" int jvsTestPressedRecently();

// The command line and the folder the loader was started from, to start the next program of segabootOnExit
static int s_argc = 0;
static char **s_argv = NULL;
static char s_startDir[MAX_PATH_LENGTH] = "";
static time_t s_startTime = 0;

static bool isTestModeArg(const char *arg)
{
    return strcmp(arg, "-t") == 0 || strcmp(arg, "--test") == 0;
}

// The flag a game leaves for the boot program when Test is pressed, in its folder (/tmp/segaboot is redirected there):
// "test", "testmode" for OutRun. Only one written during this run counts, and it is removed, as the boot program does.
static bool takeTestFlag()
{
    bool found = false;
    for (const char *flag : {"tmp/segaboot/test", "tmp/segaboot/testmode"})
    {
        struct _stat64 info;
        if (_stat64(flag, &info) == 0 && info.st_mtime >= s_startTime)
        {
            found = true;
            remove(flag);
        }
    }
    return found;
}

// What Sega's boot program does on a cabinet: a game leaves when Test is pressed, and the test menu starts; leaving
// the test menu starts the game again. Called when the game's program ends by itself (exit, or back from main), not
// for ExitGame or a closed window. The next program runs in a process of its own; the picker waits for all of them.
void segabootOnExit()
{
    static bool done = false;
    if (done || !s_argv || agentActive()) // an agent drives one program: it starts the next one itself
        return;
    done = true;

    bool testMode = false;
    for (int i = 1; i < s_argc; i++)
        testMode = testMode || isTestModeArg(s_argv[i]);
    const bool testFlag = !testMode && takeTestFlag(); // the flag, or Test pressed just before
    if (!testMode && !testFlag && !jvsTestPressedRecently())
        return;

    char exe[MAX_PATH];
    GetModuleFileNameA(NULL, exe, MAX_PATH);
    std::string command = "\"" + std::string(exe) + "\"";
    if (!testMode)
        command += " -t";
    for (int i = 1; i < s_argc; i++)
        if (!isTestModeArg(s_argv[i]))
            command += " \"" + std::string(s_argv[i]) + "\"";

    STARTUPINFOA startup = {};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process = {};
    std::vector<char> commandLine(command.begin(), command.end());
    commandLine.push_back('\0');
    if (CreateProcessA(exe, commandLine.data(), NULL, NULL, FALSE, 0, NULL, s_startDir, &startup, &process))
    {
        log_info("%s: %s", testMode ? "Leaving the test menu, the game again" : "Test pressed, the test menu", command.c_str());
        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);
    }
}

LONG CALLBACK myVectoredHandler(PEXCEPTION_POINTERS ExceptionInfo)
{
    DWORD exceptionCode = ExceptionInfo->ExceptionRecord->ExceptionCode;
    PCONTEXT ctx = ExceptionInfo->ContextRecord;

    if (exceptionCode == EXCEPTION_ACCESS_VIOLATION || exceptionCode == EXCEPTION_PRIV_INSTRUCTION) {
        uint8_t* pint = (uint8_t*)ctx->Eip;
        if (pint && !IsBadReadPtr(pint, 2) && pint[0] == 0xCD && pint[1] == 0x80) {        
            if (ctx->Eax == 0xE0) { 
                ctx->Eax = (DWORD)PthreadEmu::pthreadSelf();
                ctx->Eip += 2;
                return EXCEPTION_CONTINUE_EXECUTION;
            }
        }
    }

    return EXCEPTION_CONTINUE_SEARCH;
}   

void initBridges()
{
    FileSystemBridge::initBridges();
    GraphicsBridge::initBridges();
    LibcBridge::initBridges();
    PthreadBridge::initBridges();
    TermiosBridge::initBridges();
    GccBridge::initBridges();
    NetworkBridge::initBridges();
    IpcBridge::initBridges();
    SegaApiBridge::initBridges();
    Alsa2SdlBridge::initBridges();
    RegexBridge::initBridges();
    MathBridge::initBridges();
}

#ifdef _MSC_VER
// The UCRT kills the process when a CRT function gets an invalid argument (a bad fd from the game, a NULL
// stream...). Return the error to the caller instead, as msvcrt.dll does in the MinGW build.
static void ignoreInvalidParameter(const wchar_t *, const wchar_t *, const wchar_t *, unsigned int, uintptr_t)
{
}
#endif

int main(int argc, char *argv[], char *envp[])
{
#ifdef _MSC_VER
    _set_invalid_parameter_handler(ignoreInvalidParameter);
#endif
    logSetMinLevel(LOG_WARN);

    // --agent / --agent-show: the game driven over stdin/stdout (agent/agent.c), not arguments of the game
    static std::vector<char *> args;
    int agentMode = 0;
    for (int i = 0; i < argc; i++)
    {
        if (strcmp(argv[i], "--agent") == 0)
            agentMode = 1;
        else if (strcmp(argv[i], "--agent-show") == 0)
            agentMode = 2;
        else
            args.push_back(argv[i]);
    }
    if (agentMode)
    {
        agentInit(agentMode == 2);
        argc = (int)args.size();
        args.push_back(NULL);
        argv = args.data();
    }

    // Started without a game: the game picker
    if (argc == 1)
        return guiRunPicker(argv[0]);

    char command[MAX_PATH_LENGTH] = {0};
    char originalDir[MAX_PATH_LENGTH] = {0};
    char gameELF[MAX_PATH_LENGTH] = {0};
    char libraryPath[MAX_PATH_LENGTH] = {0};
    char gamePath[MAX_PATH_LENGTH] = {0};

    log_info("Parsing arguments...\n");
    if (parseArgs(argc, argv, command, originalDir, gameELF, libraryPath) != PARSE_ARGS_SUCCESS)
        return EXIT_SUCCESS;
    s_argc = argc;
    s_argv = argv;
    s_startTime = time(NULL);
    snprintf(s_startDir, sizeof(s_startDir), "%s", originalDir);

    char elfPath[MAX_PATH_LENGTH];
    char elfArgs[MAX_PATH_LENGTH] = "";

    char *spacePos = strchr(gameELF, ' ');
    if (spacePos != NULL)
    {
        size_t elfPathLen = spacePos - gameELF;
        strncpy(elfPath, gameELF, elfPathLen);
        elfPath[elfPathLen] = '\0';
        strcpy(elfArgs, spacePos + 1);
    }
    else
    {
        strcpy(elfPath, gameELF);
    }

    ElfLoader::PreReserveAddressSpace(elfPath);

    log_info("Initializing library search paths...\n");
    GetCurrentDirectoryA(MAX_PATH_LENGTH, gamePath);
    SymbolResolver::GetInstance().InitSearchPaths(libraryPath, gamePath);

    // Install Exception Handler (BEFORE any C++ code runs)
    if (!AddVectoredExceptionHandler(1, myVectoredHandler))
        log_error("Failed to register MyVectoredHandler");

    log_info("Initializing bridges...\n");
    initBridges();

    if (!std::filesystem::exists("tmp"))
        std::filesystem::create_directory("tmp");
    if (!std::filesystem::exists("tmp\\segaboot"))
        std::filesystem::create_directory("tmp\\segaboot");

    log_info("Loading ELF file: %s\n", elfPath);
    ElfLoader loader;

    if (!loader.Load(elfPath))
    {
        log_fatal("Failed to load ELF file: %s", elfPath);
        return 1;
    }

    uint8_t *baseAddr = (uint8_t *)loader.GetBaseAddress();
    if (baseAddr)
        partialElfCrc = getCrc32Mem(baseAddr + 10, 0x4000);

    initConfig(configPath);
    if (agentHidden()) // --agent: a window never shown, frames as fast as they go
    {
        getConfig()->fullscreen = 0;
        getConfig()->fpsLimiter = 0;
    }

    if (partialElfCrc == PRIMEVAL_HUNT_SBPP && getConfig()->phUseZink)
    {
        SymbolResolver::GetInstance().AddDllSearchSubPath("zink");
        SetEnvironmentVariableA("GALLIUM_DRIVER", "zink");
        std::string zinkGlPath = SymbolResolver::GetInstance().GetDllSearchBase() + "\\zink\\opengl32.dll";
        hGlDLL = LoadLibraryA(zinkGlPath.c_str());
    }
    else
    {
        hGlDLL = LoadLibraryA("opengl32.dll");
    }

    if (!hGlDLL)
    {
        printf("Failed to load OpenGL!\n");
        return -1;
    }

    log_debug("Initializing main...");
    initMain(configPath, controlsPath);

    int final_argc = 0;
    char *final_argv_arr[10];

    final_argv_arr[final_argc++] = elfPath;

    if (strlen(elfArgs) > 0)
    {
        char *token = strtok(elfArgs, " ");
        while (token != NULL && final_argc < 9)
        {
            final_argv_arr[final_argc++] = token;
            token = strtok(NULL, " ");
        }
    }
    final_argv_arr[final_argc] = NULL;

    if (!loader.Execute(final_argc, final_argv_arr, envp))
    {
        log_error("Failed to execute ELF file.");
        return 1;
    }

    log_info("ELF Execution Finished");
    return 0;
}

#endif