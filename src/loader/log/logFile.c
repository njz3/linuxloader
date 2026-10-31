#include "logFile.h"

#ifdef _WIN32

#include <windows.h>
#include <ctype.h>
#include <fcntl.h>
#include <io.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../config/config.h"
#include "../config/iniParser.h"
#include "log.h"

#define LOG_FILE_NAME "linuxloader.log"
#define LOG_FILE_OLD "linuxloader.old.log"
#define LOG_FILE_MAX (100ull * 1024 * 1024)

static HANDLE fileHandle = INVALID_HANDLE_VALUE;
static HANDLE pipeRead = NULL, pipeWrite = NULL;
static HANDLE copyThread = NULL;
static HANDLE output = NULL; // where this process's output went before (its console, or a front end's file or pipe)
static int savedOutFd = -1, savedErrFd = -1; // the descriptors 1 and 2 from before, restored at the end

void logFilePath(const char *writeRoot, char *path, size_t size)
{
    if (writeRoot && writeRoot[0])
    {
        snprintf(path, size, "%s\\" LOG_FILE_NAME, writeRoot);
        return;
    }
    char exe[MAX_PATH];
    GetModuleFileNameA(NULL, exe, MAX_PATH);
    char *slash = strrchr(exe, '\\');
    if (slash)
        *slash = '\0';
    snprintf(path, size, "%s\\" LOG_FILE_NAME, exe);
}

void logFileSettings(const char *settingsPath, const char *writeRoot, char *path, size_t size)
{
    if (writeRoot && writeRoot[0])
    {
        snprintf(path, size, "%s\\linuxloader.ini", writeRoot);
        if (GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES)
            return;
    }
    // None given: initConfig's linuxloader.ini, in the current folder
    snprintf(path, size, "%s", settingsPath && settingsPath[0] ? settingsPath : "linuxloader.ini");
}

bool logFileWanted(const char *settingsPath, const char *writeRoot)
{
    char path[MAX_PATH];
    logFileSettings(settingsPath, writeRoot, path, sizeof(path));
    IniConfig *ini = iniLoad(path);
    if (!ini)
        ini = (IniConfig *)calloc(1, sizeof(IniConfig)); // the default, with the command line's settings over it
    if (!ini)
        return true;
    configApplyOverrides(ini);
    bool wanted = true;
    const char *value = iniGetValue(ini, "System", "LOG_FILE");
    if (value)
    {
        char word[16] = "";
        size_t length = 0;
        for (const char *c = value; *c && length < sizeof(word) - 1; c++)
            if (!isspace((unsigned char)*c) && *c != '"')
                word[length++] = (char)tolower((unsigned char)*c);
        word[length] = '\0';
        wanted = !(strcmp(word, "false") == 0 || strcmp(word, "0") == 0 || strcmp(word, "no") == 0 ||
                   strcmp(word, "off") == 0);
    }
    iniFree(ini);
    return wanted;
}

// A folder and its parents
static void makeFolders(const char *folder)
{
    char buffer[MAX_PATH];
    snprintf(buffer, sizeof(buffer), "%s", folder);
    for (char *c = buffer + 1; *c; c++)
    {
        if (*c != '\\' || c[-1] == ':' || c[-1] == '\\')
            continue;
        *c = '\0';
        CreateDirectoryA(buffer, NULL);
        *c = '\\';
    }
    CreateDirectoryA(buffer, NULL);
}

// The file made anew, the previous run's kept as linuxloader.old.log beside it. Readable while it is written
static HANDLE openLog(const char *path)
{
    char old[MAX_PATH];
    snprintf(old, sizeof(old), "%s", path);
    char *slash = strrchr(old, '\\');
    snprintf(slash ? slash + 1 : old, sizeof(old) - (slash ? (size_t)(slash + 1 - old) : 0), LOG_FILE_OLD);
    MoveFileExA(path, old, MOVEFILE_REPLACE_EXISTING);
    return CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
}

// What comes through the pipe: as it is to the output, and to the file as plain text, the log's colours (ANSI
// sequences) taken out and each line ended by CR LF, a CR alone dropped (the log's "repeated" line rewrites)
static DWORD WINAPI copyMain(LPVOID unused)
{
    (void)unused;
    char in[4096], text[2 * sizeof(in)];
    int escape = 0; // 1 after ESC, 2 inside its sequence
    DWORD got, written;
    unsigned long long total = 0; // written to the file, which stops at LOG_FILE_MAX: a game's output never fills a disk
    while (ReadFile(pipeRead, in, sizeof(in), &got, NULL) && got > 0)
    {
        if (output)
            WriteFile(output, in, got, &written, NULL);
        if (total >= LOG_FILE_MAX)
            continue;
        DWORD length = 0;
        for (DWORD i = 0; i < got; i++)
        {
            unsigned char c = (unsigned char)in[i];
            if (escape == 1)
                escape = c == '[' ? 2 : 0;
            else if (escape == 2)
                escape = c >= 0x40 && c <= 0x7E ? 0 : 2;
            else if (c == 0x1B)
                escape = 1;
            else if (c != '\r')
            {
                if (c == '\n')
                    text[length++] = '\r';
                text[length++] = (char)c;
            }
        }
        WriteFile(fileHandle, text, length, &written, NULL);
        total += length;
        if (total >= LOG_FILE_MAX)
        {
            static const char full[] = "\r\n(linuxloader.log stops here, at 100 MB: the rest is in the console only)\r\n";
            WriteFile(fileHandle, full, sizeof(full) - 1, &written, NULL);
        }
    }
    return 0;
}

bool logFileStart(const char *writeRoot)
{
    char path[MAX_PATH], wanted[MAX_PATH] = "";
    logFilePath(writeRoot, path, sizeof(path));
    if (writeRoot && writeRoot[0])
        makeFolders(writeRoot);
    HANDLE file = openLog(path);
    if (file == INVALID_HANDLE_VALUE)
    {
        // Never the loader's folder instead of the write folder: Windows' temporary one
        snprintf(wanted, sizeof(wanted), "%s", path);
        char temp[MAX_PATH];
        DWORD length = GetTempPathA(sizeof(temp), temp);
        if (length > 0 && length < sizeof(temp))
        {
            snprintf(path, sizeof(path), "%s" LOG_FILE_NAME, temp);
            file = openLog(path);
        }
        if (file == INVALID_HANDLE_VALUE)
        {
            log_warn("LOG_FILE: neither %s nor %s can be written, the log stays in the console", wanted, path);
            return false;
        }
        log_warn("LOG_FILE: %s cannot be written, the log goes to %s", wanted, path);
    }

    SECURITY_ATTRIBUTES inherited = {sizeof(inherited), NULL, TRUE};
    if (!CreatePipe(&pipeRead, &pipeWrite, &inherited, 0))
    {
        CloseHandle(file);
        return false;
    }
    SetHandleInformation(pipeRead, HANDLE_FLAG_INHERIT, 0); // only this process reads it
    fileHandle = file;

    // The run's first lines: the build, when, the command line, and where the file was meant to go
    SYSTEMTIME now;
    GetLocalTime(&now);
    char header[4096];
    int length = snprintf(header, sizeof(header), "%s, %04d-%02d-%02d %02d:%02d:%02d\r\n%s\r\n", buildEdition(), now.wYear,
                          now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond, GetCommandLineA());
    if (wanted[0] && length > 0 && length < (int)sizeof(header))
        length += snprintf(header + length, sizeof(header) - length, "(%s could not be written)\r\n", wanted);
    DWORD written;
    if (length > 0)
        WriteFile(fileHandle, header, (DWORD)(length < (int)sizeof(header) ? length : (int)sizeof(header) - 1), &written,
                  NULL);
    WriteFile(fileHandle, "\r\n", 2, &written, NULL);

    // A handle of the copy's own: _dup2 below closes the one the standard output had, and Windows gives its number to
    // the next handle made, the pipe's (the copy then wrote into what it reads, without end)
    output = NULL;
    HANDLE standard = GetStdHandle(STD_OUTPUT_HANDLE);
    if (standard && standard != INVALID_HANDLE_VALUE &&
        !DuplicateHandle(GetCurrentProcess(), standard, GetCurrentProcess(), &output, 0, FALSE, DUPLICATE_SAME_ACCESS))
        output = NULL;
    copyThread = CreateThread(NULL, 0, copyMain, NULL, 0, NULL);
    if (!copyThread)
    {
        CloseHandle(pipeRead);
        CloseHandle(pipeWrite);
        CloseHandle(fileHandle);
        if (output)
            CloseHandle(output);
        pipeRead = pipeWrite = output = NULL;
        fileHandle = INVALID_HANDLE_VALUE;
        return false;
    }

    // This process's output into the pipe: the C runtime's descriptors, and the handles the game's processes get
    // (segabootSupervise passes its own)
    fflush(stdout);
    fflush(stderr);
    savedOutFd = _dup(1);
    savedErrFd = _dup(2);
    HANDLE copy = NULL;
    if (DuplicateHandle(GetCurrentProcess(), pipeWrite, GetCurrentProcess(), &copy, 0, FALSE, DUPLICATE_SAME_ACCESS))
    {
        int fd = _open_osfhandle((intptr_t)copy, _O_TEXT | _O_WRONLY);
        if (fd >= 0)
        {
            _dup2(fd, 1);
            _dup2(fd, 2);
            _close(fd);
        }
        else
            CloseHandle(copy);
    }
    setvbuf(stdout, NULL, _IONBF, 0);
    SetStdHandle(STD_OUTPUT_HANDLE, pipeWrite);
    SetStdHandle(STD_ERROR_HANDLE, pipeWrite);
    return true;
}

void logFileStop(void)
{
    if (!copyThread)
        return;
    fflush(stdout);
    fflush(stderr);
    // The descriptors back where they were, their ends of the pipe closed with them
    if (savedOutFd >= 0)
    {
        _dup2(savedOutFd, 1);
        _close(savedOutFd);
    }
    else
        _close(1);
    if (savedErrFd >= 0)
    {
        _dup2(savedErrFd, 2);
        _close(savedErrFd);
    }
    else
        _close(2);
    // The handles of the descriptors as they are now (the ones from before were closed by the first _dup2)
    HANDLE restoredOut = (HANDLE)_get_osfhandle(1), restoredErr = (HANDLE)_get_osfhandle(2);
    SetStdHandle(STD_OUTPUT_HANDLE, restoredOut != INVALID_HANDLE_VALUE ? restoredOut : NULL);
    SetStdHandle(STD_ERROR_HANDLE, restoredErr != INVALID_HANDLE_VALUE ? restoredErr : NULL);
    CloseHandle(pipeWrite);
    pipeWrite = NULL;
    // The game's processes have ended: the pipe empties, then reads its end
    WaitForSingleObject(copyThread, 3000);
    CloseHandle(copyThread);
    copyThread = NULL;
    CloseHandle(pipeRead);
    pipeRead = NULL;
    CloseHandle(fileHandle);
    fileHandle = INVALID_HANDLE_VALUE;
    if (output)
        CloseHandle(output);
    output = NULL;
}

#else

#include <stdio.h>

void logFilePath(const char *writeRoot, char *path, size_t size)
{
    (void)writeRoot;
    if (size)
        path[0] = '\0';
}

void logFileSettings(const char *settingsPath, const char *writeRoot, char *path, size_t size)
{
    (void)writeRoot;
    snprintf(path, size, "%s", settingsPath ? settingsPath : "");
}

bool logFileWanted(const char *settingsPath, const char *writeRoot)
{
    (void)settingsPath;
    (void)writeRoot;
    return false;
}

bool logFileStart(const char *writeRoot)
{
    (void)writeRoot;
    return false;
}

void logFileStop(void)
{
}

#endif
