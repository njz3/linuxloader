#include "writeDir.h"

#ifdef _WIN32

#include <windows.h>
#include <ctype.h>
#include <direct.h>
#include <errno.h>
#include <fcntl.h>
#include <io.h>
#include <stdlib.h>
#include <string.h>

#include "../config/iniParser.h"
#include "../log/log.h"

static char rootDir[MAX_PATH] = "";      // WRITE_DIR
static char gameWriteDir[MAX_PATH] = ""; // WRITE_DIR\<game>, the current folder while the game runs
static char gameDir[MAX_PATH] = "";      // the game's folder, only read
static int active = 0;

static int exists(const char *path)
{
    return GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES;
}

static int isFolder(const char *path)
{
    DWORD attributes = GetFileAttributesA(path);
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY);
}

static int isAbsolute(const char *path)
{
    return (isalpha((unsigned char)path[0]) && path[1] == ':') || (path[0] == '\\' && path[1] == '\\');
}

static void join(char *out, size_t size, const char *folder, const char *name)
{
    if (name[0])
        snprintf(out, size, "%s\\%s", folder, name);
    else
        snprintf(out, size, "%s", folder);
}

// The folders of path made, path itself too when last
static void makeFolders(const char *path, int last)
{
    char buffer[MAX_PATH];
    snprintf(buffer, sizeof(buffer), "%s", path);
    size_t start = 0;
    if (buffer[0] == '\\' && buffer[1] == '\\') // \\server\share\: from the share on
    {
        const char *share = strchr(buffer + 2, '\\');
        share = share ? strchr(share + 1, '\\') : NULL;
        start = share ? (size_t)(share - buffer) + 1 : strlen(buffer);
    }
    else if (buffer[1] == ':')
        start = 3;
    for (size_t i = start; buffer[i]; i++)
    {
        if (buffer[i] != '\\')
            continue;
        buffer[i] = '\0';
        CreateDirectoryA(buffer, NULL);
        buffer[i] = '\\';
    }
    if (last)
        CreateDirectoryA(buffer, NULL);
}

// path inside folder (case-insensitive): what follows it, else NULL
static const char *inside(const char *path, const char *folder)
{
    size_t length = strlen(folder);
    if (!length || _strnicmp(path, folder, length) != 0)
        return NULL;
    if (path[length] == '\\')
        return path + length + 1;
    return path[length] == '\0' ? path + length : NULL;
}

// The part of path in the game's folder: a relative path, or an absolute one in WRITE_DIR\<game> or in the game's
// folder. NULL for the others: devices, root-relative Linux paths, other folders.
static const char *relativePart(const char *path, char *buffer, size_t size)
{
    if (!active || !path || !path[0])
        return NULL;
    char normal[MAX_PATH];
    snprintf(normal, sizeof(normal), "%s", path);
    for (char *c = normal; *c; c++)
        if (*c == '/')
            *c = '\\';
    if (_stricmp(normal, "NUL") == 0 || _stricmp(normal, "CON") == 0 || strncmp(normal, "\\\\.\\", 4) == 0)
        return NULL;

    const char *rest;
    if (isAbsolute(normal))
    {
        rest = inside(normal, gameWriteDir);
        if (!rest)
            rest = inside(normal, gameDir);
        if (!rest)
            return NULL;
    }
    else if (normal[0] == '\\')
        return NULL;
    else
    {
        rest = normal;
        while (rest[0] == '.' && rest[1] == '\\')
            rest += 2;
        if (strcmp(rest, ".") == 0)
            rest = "";
    }
    snprintf(buffer, size, "%s", rest);
    size_t length = strlen(buffer);
    if (length && buffer[length - 1] == '\\')
        buffer[length - 1] = '\0';
    return buffer;
}

int writeDirActive(void)
{
    return active;
}

int writeDirGameFolder(char *buffer, size_t size)
{
    if (!active)
        return 0;
    snprintf(buffer, size, "%s", gameDir);
    return 1;
}

const char *writeDirForRead(const char *path, char *buffer, size_t size)
{
    char relative[MAX_PATH], gameSide[MAX_PATH];
    if (!relativePart(path, relative, sizeof(relative)))
        return path;
    join(buffer, size, gameWriteDir, relative);
    if (exists(buffer))
        return buffer;
    join(gameSide, sizeof(gameSide), gameDir, relative);
    if (exists(gameSide))
        snprintf(buffer, size, "%s", gameSide);
    return buffer;
}

const char *writeDirForWrite(const char *path, int keep, char *buffer, size_t size)
{
    char relative[MAX_PATH], gameSide[MAX_PATH];
    if (!relativePart(path, relative, sizeof(relative)))
        return path;
    join(buffer, size, gameWriteDir, relative);
    makeFolders(buffer, 0);
    if (keep && !exists(buffer))
    {
        join(gameSide, sizeof(gameSide), gameDir, relative);
        if (exists(gameSide) && !isFolder(gameSide))
        {
            if (CopyFileA(gameSide, buffer, TRUE))
                log_debug("write folder: %s copied to be modified", relative);
        }
    }
    return buffer;
}

int writeDirSides(const char *path, char *writeSide, char *gameSide, size_t size)
{
    char relative[MAX_PATH];
    if (!relativePart(path, relative, sizeof(relative)))
        return 0;
    join(writeSide, size, gameWriteDir, relative);
    join(gameSide, size, gameDir, relative);
    return 1;
}

FILE *writeDirFopen(const char *path, const char *mode)
{
    char buffer[MAX_PATH];
    if (!active)
        return fopen(path, mode);
    int write = strchr(mode, 'w') || strchr(mode, 'a') || strchr(mode, '+');
    const char *target = write ? writeDirForWrite(path, strchr(mode, 'w') == NULL, buffer, sizeof(buffer))
                               : writeDirForRead(path, buffer, sizeof(buffer));
    return fopen(target, mode);
}

int writeDirOpen(const char *path, int flags, int mode)
{
    char buffer[MAX_PATH];
    if (!active)
        return _open(path, flags, mode);
    int write = (flags & (_O_WRONLY | _O_RDWR | _O_CREAT | _O_APPEND)) != 0;
    const char *target = write ? writeDirForWrite(path, (flags & _O_TRUNC) == 0, buffer, sizeof(buffer))
                               : writeDirForRead(path, buffer, sizeof(buffer));
    return _open(target, flags, mode);
}

int writeDirMkdir(const char *path)
{
    char buffer[MAX_PATH];
    return _mkdir(writeDirForWrite(path, 0, buffer, sizeof(buffer)));
}

// A file only in the game's folder stays there: removing it succeeds, and it is read again from there
int writeDirRemove(const char *path)
{
    char writeSide[MAX_PATH], gameSide[MAX_PATH];
    if (!writeDirSides(path, writeSide, gameSide, sizeof(writeSide)))
        return remove(path);
    if (exists(writeSide))
        return remove(writeSide);
    if (exists(gameSide))
        return 0;
    errno = ENOENT;
    return -1;
}

// As on Linux, the target replaced; a file only in the game's folder copied to its new name
int writeDirRename(const char *from, const char *to)
{
    char source[MAX_PATH], target[MAX_PATH];
    if (!active)
        return rename(from, to);
    const char *readFrom = writeDirForRead(from, source, sizeof(source));
    const char *writeTo = writeDirForWrite(to, 0, target, sizeof(target));
    int fromGame = gameDir[0] && inside(readFrom, gameDir) && !inside(readFrom, gameWriteDir);
    BOOL done = fromGame ? CopyFileA(readFrom, writeTo, FALSE) : MoveFileExA(readFrom, writeTo, MOVEFILE_REPLACE_EXISTING);
    if (!done)
    {
        errno = ENOENT;
        return -1;
    }
    return 0;
}

// WRITE_DIR relative to the loader's folder
static void absoluteRoot(const char *writeDir)
{
    if (isAbsolute(writeDir))
    {
        snprintf(rootDir, sizeof(rootDir), "%s", writeDir);
    }
    else
    {
        char exe[MAX_PATH];
        GetModuleFileNameA(NULL, exe, MAX_PATH);
        char *slash = strrchr(exe, '\\');
        if (slash)
            *slash = '\0';
        snprintf(rootDir, sizeof(rootDir), "%s\\%s", exe, writeDir);
    }
    for (char *c = rootDir; *c; c++)
        if (*c == '/')
            *c = '\\';
    size_t length = strlen(rootDir);
    while (length > 3 && rootDir[length - 1] == '\\')
        rootDir[--length] = '\0';
}

// The settings file at WRITE_DIR's root, copied there from its first place the first time
static void overlaySettings(char **path, const char *name)
{
    char overlay[MAX_PATH];
    join(overlay, sizeof(overlay), rootDir, name);
    if (!exists(overlay) && *path && (*path)[0] && exists(*path))
    {
        if (CopyFileA(*path, overlay, TRUE))
            log_info("write folder: %s copied from %s", name, *path);
    }
    *path = _strdup(overlay);
}

void writeDirSettings(const char *writeDir, char **configPath, char **controlsPath)
{
    char fromIni[MAX_PATH] = "";
    if ((!writeDir || !writeDir[0]) && configPath && *configPath && (*configPath)[0])
    {
        IniConfig *ini = iniLoad(*configPath);
        if (ini)
        {
            const char *value = iniGetValue(ini, "Paths", "WRITE_DIR");
            if (value)
            {
                snprintf(fromIni, sizeof(fromIni), "%s", value);
                // The ini's quotes, as getString drops them
                size_t length = strlen(fromIni);
                if (length >= 2 && fromIni[0] == '"' && fromIni[length - 1] == '"')
                {
                    memmove(fromIni, fromIni + 1, length - 2);
                    fromIni[length - 2] = '\0';
                }
            }
            iniFree(ini);
        }
        writeDir = fromIni;
    }
    if (!writeDir || !writeDir[0])
        return;
    absoluteRoot(writeDir);
    makeFolders(rootDir, 1);
    if (!isFolder(rootDir))
    {
        log_error("write folder: cannot make %s; the game's folder is written as before", rootDir);
        rootDir[0] = '\0';
        return;
    }
    overlaySettings(configPath, "linuxloader.ini");
    overlaySettings(controlsPath, "controls.ini");
    log_info("write folder: %s", rootDir);
}

void writeDirEnterGame(const char *gameName)
{
    if (!rootDir[0])
        return;
    // The game's short name, as a folder name
    char name[64];
    snprintf(name, sizeof(name), "%s", gameName && gameName[0] ? gameName : "game");
    for (char *c = name; *c; c++)
        if (!isalnum((unsigned char)*c) && *c != '-' && *c != '_' && *c != '.')
            *c = '_';
    GetCurrentDirectoryA(MAX_PATH, gameDir);
    join(gameWriteDir, sizeof(gameWriteDir), rootDir, name);
    makeFolders(gameWriteDir, 1);
    if (!isFolder(gameWriteDir) || _chdir(gameWriteDir) != 0)
    {
        log_error("write folder: cannot use %s; the game's folder is written as before", gameWriteDir);
        return;
    }
    active = 1;
    log_info("write folder: the game writes in %s, reads %s", gameWriteDir, gameDir);
}

#else

#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

void writeDirSettings(const char *writeDir, char **configPath, char **controlsPath)
{
    (void)writeDir;
    (void)configPath;
    (void)controlsPath;
}

void writeDirEnterGame(const char *gameName)
{
    (void)gameName;
}

int writeDirActive(void)
{
    return 0;
}

int writeDirGameFolder(char *buffer, size_t size)
{
    (void)buffer;
    (void)size;
    return 0;
}

const char *writeDirForRead(const char *path, char *buffer, size_t size)
{
    (void)buffer;
    (void)size;
    return path;
}

const char *writeDirForWrite(const char *path, int keep, char *buffer, size_t size)
{
    (void)keep;
    (void)buffer;
    (void)size;
    return path;
}

int writeDirSides(const char *path, char *writeSide, char *gameSide, size_t size)
{
    (void)path;
    (void)writeSide;
    (void)gameSide;
    (void)size;
    return 0;
}

FILE *writeDirFopen(const char *path, const char *mode)
{
    return fopen(path, mode);
}

int writeDirOpen(const char *path, int flags, int mode)
{
    return open(path, flags, mode);
}

int writeDirMkdir(const char *path)
{
    return mkdir(path, 0777);
}

int writeDirRemove(const char *path)
{
    return remove(path);
}

int writeDirRename(const char *from, const char *to)
{
    return rename(from, to);
}

#endif
