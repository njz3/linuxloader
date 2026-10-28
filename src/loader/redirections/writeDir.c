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
static char seedRoot[MAX_PATH] = "";     // SEED_DIR: <seed>\<game> holds WRITE_DIR\<game>'s first files
static char gameWriteDir[MAX_PATH] = ""; // WRITE_DIR\<game>, the current folder while the game runs
static char gameDir[MAX_PATH] = "";      // the game's folder, only read
static char currentSub[MAX_PATH] = "";   // where the game's chdir took it, in WRITE_DIR\<game> ("": its root)
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

// "a\b\..\c\.\d" as "a\c\d", in place; 0 when it climbs above where it starts
static int normalise(char *path)
{
    char out[MAX_PATH] = "";
    size_t length = 0;
    const char *part = path;
    while (*part)
    {
        const char *end = strchr(part, '\\');
        size_t size = end ? (size_t)(end - part) : strlen(part);
        if (size == 2 && part[0] == '.' && part[1] == '.')
        {
            if (!length)
                return 0;
            char *last = strrchr(out, '\\');
            length = last ? (size_t)(last - out) : 0;
            out[length] = '\0';
        }
        else if (size && !(size == 1 && part[0] == '.') && length + size + 2 < sizeof(out))
        {
            if (length)
                out[length++] = '\\';
            memcpy(out + length, part, size);
            length += size;
            out[length] = '\0';
        }
        part += size;
        while (*part == '\\')
            part++;
    }
    strcpy(path, out);
    return 1;
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
    // A relative path from where the game's chdir took it
    char combined[MAX_PATH];
    if (currentSub[0] && !isAbsolute(normal))
        snprintf(combined, sizeof(combined), "%s\\%s", currentSub, rest);
    else
        snprintf(combined, sizeof(combined), "%s", rest);
    if (!normalise(combined))
        snprintf(combined, sizeof(combined), "%s", rest); // above the game's folder (..\JenTest): as it is
    snprintf(buffer, size, "%s", combined);
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

// A folder of the game's, in WRITE_DIR\<game>: made there when it is only in the game's folder (OutRun 2's chdir into
// shader\fs\psd before it reads its shaders by their bare names), so that the current folder stays in WRITE_DIR\<game>
// and the names read next are looked for from there, then in the game's folder
int writeDirChdir(const char *path)
{
    char relative[MAX_PATH], writeSide[MAX_PATH], gameSide[MAX_PATH];
    if (!relativePart(path, relative, sizeof(relative)))
        return _chdir(path);
    join(writeSide, sizeof(writeSide), gameWriteDir, relative);
    join(gameSide, sizeof(gameSide), gameDir, relative);
    if (!isFolder(writeSide) && isFolder(gameSide))
        makeFolders(writeSide, 1);
    if (_chdir(writeSide) != 0)
        return -1;
    snprintf(currentSub, sizeof(currentSub), "%s", relative);
    return 0;
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

// WRITE_DIR or SEED_DIR, relative to the loader's folder
static void absoluteFolder(char *out, size_t size, const char *folder)
{
    if (isAbsolute(folder))
    {
        snprintf(out, size, "%s", folder);
    }
    else
    {
        char exe[MAX_PATH];
        GetModuleFileNameA(NULL, exe, MAX_PATH);
        char *slash = strrchr(exe, '\\');
        if (slash)
            *slash = '\0';
        snprintf(out, size, "%s\\%s", exe, folder);
    }
    for (char *c = out; *c; c++)
        if (*c == '/')
            *c = '\\';
    size_t length = strlen(out);
    while (length > 3 && out[length - 1] == '\\')
        out[--length] = '\0';
}

// [Paths] KEY of the ini, its quotes dropped as getString does
static void iniPath(IniConfig *ini, const char *key, char *out, size_t size)
{
    const char *value = iniGetValue(ini, "Paths", key);
    if (!value)
        return;
    snprintf(out, size, "%s", value);
    size_t length = strlen(out);
    if (length >= 2 && out[0] == '"' && out[length - 1] == '"')
    {
        memmove(out, out + 1, length - 2);
        out[length - 2] = '\0';
    }
}

// The files of from, its folders' too, copied into to where it has none of the same name: a file already in the write
// folder is never replaced. relative: from's place in <seed>\<game>, for the log. The count copied.
static int seedFolder(const char *from, const char *to, const char *relative)
{
    char pattern[MAX_PATH];
    join(pattern, sizeof(pattern), from, "*");
    WIN32_FIND_DATAA found;
    HANDLE find = FindFirstFileA(pattern, &found);
    if (find == INVALID_HANDLE_VALUE)
        return 0;
    int copied = 0;
    do
    {
        if (strcmp(found.cFileName, ".") == 0 || strcmp(found.cFileName, "..") == 0)
            continue;
        char source[MAX_PATH], target[MAX_PATH], name[MAX_PATH];
        join(source, sizeof(source), from, found.cFileName);
        join(target, sizeof(target), to, found.cFileName);
        if (relative[0])
            snprintf(name, sizeof(name), "%s\\%s", relative, found.cFileName);
        else
            snprintf(name, sizeof(name), "%s", found.cFileName);
        if (found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            CreateDirectoryA(target, NULL);
            copied += seedFolder(source, target, name);
        }
        else if (!exists(target))
        {
            if (CopyFileA(source, target, TRUE))
            {
                log_info("write folder: %s seeded from %s", name, source);
                copied++;
            }
            else
            {
                log_warn("write folder: %s could not be seeded from %s (error %lu)", name, source, GetLastError());
            }
        }
    } while (FindNextFileA(find, &found));
    FindClose(find);
    return copied;
}

// The settings file at WRITE_DIR's root, copied there from its first place the first time. Changed in its first place
// after that copy (later than it), it is not read: said in the log, as such a change otherwise goes unnoticed
static void overlaySettings(char **path, const char *name)
{
    char overlay[MAX_PATH];
    join(overlay, sizeof(overlay), rootDir, name);
    if (!exists(overlay) && *path && (*path)[0] && exists(*path))
    {
        if (CopyFileA(*path, overlay, TRUE))
            log_info("write folder: %s copied from %s", name, *path);
    }
    else if (*path && (*path)[0])
    {
        WIN32_FILE_ATTRIBUTE_DATA first, copy;
        if (GetFileAttributesExA(*path, GetFileExInfoStandard, &first) &&
            GetFileAttributesExA(overlay, GetFileExInfoStandard, &copy) &&
            CompareFileTime(&first.ftLastWriteTime, &copy.ftLastWriteTime) > 0)
            log_warn("write folder: %s, the one read, is older than %s: the changes made there since are not read (copy "
                     "that file over it, or change the one in the write folder)",
                     overlay, *path);
    }
    *path = _strdup(overlay);
}

void writeDirSettings(const char *writeDir, const char *seedDir, char **configPath, char **controlsPath)
{
    char writeFromIni[MAX_PATH] = "", seedFromIni[MAX_PATH] = "";
    int writeAsked = writeDir && writeDir[0], seedAsked = seedDir && seedDir[0];
    if ((!writeAsked || !seedAsked) && configPath && *configPath && (*configPath)[0])
    {
        IniConfig *ini = iniLoad(*configPath);
        if (ini)
        {
            iniPath(ini, "WRITE_DIR", writeFromIni, sizeof(writeFromIni));
            iniPath(ini, "SEED_DIR", seedFromIni, sizeof(seedFromIni));
            iniFree(ini);
        }
        if (!writeAsked)
            writeDir = writeFromIni;
        if (!seedAsked)
            seedDir = seedFromIni;
    }
    if (!writeDir || !writeDir[0])
    {
        if (seedDir && seedDir[0])
            log_warn("seed folder %s: only used with a write folder (--write-dir or [Paths] WRITE_DIR)", seedDir);
        return;
    }
    absoluteFolder(rootDir, sizeof(rootDir), writeDir);
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
    if (seedDir && seedDir[0])
    {
        absoluteFolder(seedRoot, sizeof(seedRoot), seedDir);
        if (isFolder(seedRoot))
        {
            log_info("seed folder: %s", seedRoot);
        }
        else
        {
            log_warn("seed folder: %s not found, the game's files are copied as without it", seedRoot);
            seedRoot[0] = '\0';
        }
    }
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
    // The cabinet's templates (a link cabinet's NVRAM, its LINK ID set), in place of the game folder's files: copied
    // once, before the game reads or lists anything
    if (seedRoot[0])
    {
        char seedGame[MAX_PATH];
        join(seedGame, sizeof(seedGame), seedRoot, name);
        if (isFolder(seedGame))
            log_info("seed folder: %d file(s) of %s seeded", seedFolder(seedGame, gameWriteDir, ""), seedGame);
        else
            log_info("seed folder: no %s, nothing to seed", seedGame);
    }
}

#else

#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

void writeDirSettings(const char *writeDir, const char *seedDir, char **configPath, char **controlsPath)
{
    (void)writeDir;
    (void)seedDir;
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

int writeDirChdir(const char *path)
{
    return chdir(path);
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
