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
static char imageRoot[MAX_PATH] = "";    // a Linux disk image's root (Gaelco's), gameDir being inside it
static int active = 0;
// WRITE_DIR asked for but unusable (not made, or a file not written in it): nothing is kept. The settings and the
// game's files are read where they are (the loader's folder, the game's, the current one staying the game's), and
// never written there, those folders being the cabinet's templates (two emulators started at once by accident must
// not change them). Every write is refused (folders made, removed, renamed included), but for the files a game
// cannot start without (writeDirTempWrites: [Paths] TEMP_WRITES, the game's module, the loader's boards' memories):
// those go to a copy of their own in Windows' temporary folder (shadowOf), read back for the rest of the run (an
// EEPROM's settings the loader writes at start, Namco's ifconfig.txt) and removed when the loader ends
static int refused = 0;

// A Linux disk image's files outside the game's folder (Gaelco's /home/save0/switch.bin, the image's root being the
// one of their absolute paths): in WRITE_DIR\<game>\linuxroot, as in the image
#define IMAGE_FOLDER "linuxroot"

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

// A folder that can be written: made, and a file created, written and removed in it. Else the reason, for the log
static int usable(const char *folder, char *reason, size_t size)
{
    if (!isFolder(folder))
    {
        snprintf(reason, size, "it cannot be made (error %lu)", GetLastError());
        return 0;
    }
    char probe[MAX_PATH];
    snprintf(probe, sizeof(probe), "%s\\.linuxloader-probe-%lu.tmp", folder, GetCurrentProcessId());
    HANDLE file = CreateFileA(probe, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_TEMPORARY, NULL);
    if (file == INVALID_HANDLE_VALUE)
    {
        snprintf(reason, size, "a file cannot be created in it (error %lu)", GetLastError());
        return 0;
    }
    DWORD written = 0;
    BOOL wrote = WriteFile(file, "linuxloader", 11, &written, NULL) && written == 11;
    DWORD error = GetLastError();
    CloseHandle(file);
    if (!DeleteFileA(probe))
    {
        snprintf(reason, size, "a file written in it cannot be removed (error %lu)", GetLastError());
        return 0;
    }
    if (!wrote)
    {
        snprintf(reason, size, "a file cannot be written in it (error %lu)", error);
        return 0;
    }
    return 1;
}

static char refusedFolder[MAX_PATH] = "";

static void refuse(const char *folder, const char *reason)
{
    refused = 1;
    snprintf(refusedFolder, sizeof(refusedFolder), "%s", folder);
    log_error("write folder %s refused: %s. Nothing is written: the settings and the game saves are read from the usual "
              "folders and not written",
              folder, reason);
}

// A write refused, in the log once for each path, up to REFUSED_LOGGED of them: the names to give TEMP_WRITES for a
// game that does not start
#define REFUSED_LOGGED 300
static char *refusedLogged[REFUSED_LOGGED];
static int refusedLoggedCount = 0;
static SRWLOCK refusedLock = SRWLOCK_INIT;

static void refusedWrite(const char *what, const char *path)
{
    AcquireSRWLockExclusive(&refusedLock);
    int seen = 0;
    for (int i = 0; i < refusedLoggedCount && !seen; i++)
        seen = _stricmp(refusedLogged[i], path) == 0;
    if (!seen && refusedLoggedCount < REFUSED_LOGGED)
    {
        refusedLogged[refusedLoggedCount++] = _strdup(path);
        log_warn("write folder refused: %s %s not written%s", what, path,
                 refusedLoggedCount == REFUSED_LOGGED ? " (the next ones are not logged)" : "");
    }
    ReleaseSRWLockExclusive(&refusedLock);
}

// The files still written while refused: name patterns separated by ';'. The loader's own files a game reads back:
// Namco's dongle image (namcoHasp.c) and eth0 as its ifconfig.pl gives it (filesystemShared.c)
static char tempWrites[2048] = "hasp.bin;ifconfig.txt";

void writeDirTempWrites(const char *patterns)
{
    if (!patterns)
        return;
    char copy[1024];
    snprintf(copy, sizeof(copy), "%s", patterns);
    for (char *pattern = strtok(copy, ";"); pattern; pattern = strtok(NULL, ";"))
    {
        while (*pattern == ' ' || *pattern == '\t' || *pattern == '"')
            pattern++;
        char *end = pattern + strlen(pattern);
        while (end > pattern && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '"'))
            *--end = '\0';
        // a name: the last part of a path given (EEPROM_PATH)
        for (char *c = pattern; *c; c++)
            if (*c == '/' || *c == '\\')
                pattern = c + 1;
        if (!pattern[0])
            continue;
        size_t length = strlen(tempWrites);
        if (length + strlen(pattern) + 2 < sizeof(tempWrites))
            snprintf(tempWrites + length, sizeof(tempWrites) - length, "%s%s", length ? ";" : "", pattern);
    }
}

// name matches pattern, without case: * any run of characters, ? any one
static int nameMatches(const char *pattern, const char *name)
{
    if (*pattern == '\0')
        return *name == '\0';
    if (*pattern == '*')
        return nameMatches(pattern + 1, name) || (*name && nameMatches(pattern, name + 1));
    if (*name && (*pattern == '?' || tolower((unsigned char)*pattern) == tolower((unsigned char)*name)))
        return nameMatches(pattern + 1, name + 1);
    return 0;
}

// Whether a file is one still written while refused
static int essential(const char *path)
{
    const char *name = path;
    for (const char *c = path; *c; c++)
        if (*c == '/' || *c == '\\')
            name = c + 1;
    char patterns[sizeof(tempWrites)];
    snprintf(patterns, sizeof(patterns), "%s", tempWrites);
    char *context = NULL;
    for (char *pattern = strtok_s(patterns, ";", &context); pattern; pattern = strtok_s(NULL, ";", &context))
        if (nameMatches(pattern, name))
            return 1;
    return 0;
}

// The copies of the files written while the write folder is refused, by full path
#define SHADOW_FILES 256
static struct
{
    char path[MAX_PATH];
    char copy[MAX_PATH];
} shadows[SHADOW_FILES];
static int shadowCount = 0;
static SRWLOCK shadowLock = SRWLOCK_INIT;
static char shadowFolder[MAX_PATH] = ""; // %TEMP%\linuxloader-nosave-<pid>

#define SHADOW_PREFIX "linuxloader-nosave-"

// A folder of copies and what is in it removed
static void shadowFolderRemove(const char *folder)
{
    char pattern[MAX_PATH], file[MAX_PATH];
    snprintf(pattern, sizeof(pattern), "%s\\*", folder);
    WIN32_FIND_DATAA found;
    HANDLE search = FindFirstFileA(pattern, &found);
    if (search != INVALID_HANDLE_VALUE)
    {
        do
        {
            if (!(found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
            {
                snprintf(file, sizeof(file), "%s\\%s", folder, found.cFileName);
                DeleteFileA(file);
            }
        } while (FindNextFileA(search, &found));
        FindClose(search);
    }
    RemoveDirectoryA(folder);
}

static void shadowsRemove(void)
{
    if (shadowFolder[0])
        shadowFolderRemove(shadowFolder);
}

// The folders of copies a loader left when it ended without its atexit (the agent's quit, a crash): those whose
// process is no longer running
static void shadowFoldersStale(const char *temp)
{
    char pattern[MAX_PATH], folder[MAX_PATH];
    snprintf(pattern, sizeof(pattern), "%s" SHADOW_PREFIX "*", temp);
    WIN32_FIND_DATAA found;
    HANDLE search = FindFirstFileA(pattern, &found);
    if (search == INVALID_HANDLE_VALUE)
        return;
    do
    {
        DWORD pid = (DWORD)strtoul(found.cFileName + strlen(SHADOW_PREFIX), NULL, 10);
        HANDLE process = pid ? OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid) : NULL;
        DWORD code = 0;
        int running = process && GetExitCodeProcess(process, &code) && code == STILL_ACTIVE;
        if (process)
            CloseHandle(process);
        if (!running && (found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
        {
            snprintf(folder, sizeof(folder), "%s%s", temp, found.cFileName);
            shadowFolderRemove(folder);
        }
    } while (FindNextFileA(search, &found));
    FindClose(search);
}

// %TEMP%\linuxloader-nosave-<pid>, made the first time; the stale ones removed then
static const char *shadowFolderMade(void)
{
    if (shadowFolder[0])
        return shadowFolder;
    char temp[MAX_PATH];
    DWORD length = GetTempPathA(sizeof(temp), temp);
    if (!length || length >= sizeof(temp) - 40)
        return NULL;
    shadowFoldersStale(temp);
    snprintf(shadowFolder, sizeof(shadowFolder), "%s" SHADOW_PREFIX "%lu", temp, GetCurrentProcessId());
    if (!CreateDirectoryA(shadowFolder, NULL) && GetLastError() != ERROR_ALREADY_EXISTS)
    {
        shadowFolder[0] = '\0';
        return NULL;
    }
    atexit(shadowsRemove);
    return shadowFolder;
}

// The copy of a file written: made the first time when create (from the file itself when keep, empty otherwise), in
// %TEMP%\linuxloader-nosave-<pid>. NULL: none (not made, or none could be)
static const char *shadowOf(const char *path, int create, int keep)
{
    char full[MAX_PATH];
    if (!path || !_fullpath(full, path, sizeof(full)))
        return NULL;
    const char *copy = NULL;
    AcquireSRWLockExclusive(&shadowLock);
    for (int i = 0; i < shadowCount && !copy; i++)
        if (shadows[i].copy[0] && _stricmp(shadows[i].path, full) == 0)
            copy = shadows[i].copy;
    if (!copy && create && shadowCount < SHADOW_FILES)
    {
        const char *folder = shadowFolderMade();
        char *slot = shadows[shadowCount].copy;
        if (folder && GetTempFileNameA(folder, "w", 0, slot))
        {
            if (keep && exists(full) && !isFolder(full))
                CopyFileA(full, slot, FALSE);
            SetFileAttributesA(slot, FILE_ATTRIBUTE_TEMPORARY);
            snprintf(shadows[shadowCount].path, MAX_PATH, "%s", full);
            shadowCount++;
            copy = slot;
            log_warn("write folder refused: %s written in a copy, %s, removed when the loader ends", full, copy);
        }
        else
            slot[0] = '\0';
    }
    ReleaseSRWLockExclusive(&shadowLock);
    return copy;
}

static void shadowForget(const char *path)
{
    char full[MAX_PATH];
    if (!_fullpath(full, path, sizeof(full)))
        return;
    AcquireSRWLockExclusive(&shadowLock);
    for (int i = 0; i < shadowCount; i++)
        if (shadows[i].copy[0] && _stricmp(shadows[i].path, full) == 0)
        {
            DeleteFileA(shadows[i].copy);
            shadows[i].copy[0] = '\0';
        }
    ReleaseSRWLockExclusive(&shadowLock);
}

// The game's side of a part relativePart gives: in the game's folder, or in the Linux disk image
static void gameSideOf(char *out, size_t size, const char *relative)
{
    size_t length = strlen(IMAGE_FOLDER);
    if (imageRoot[0] && _strnicmp(relative, IMAGE_FOLDER, length) == 0 && (relative[length] == '\\' || !relative[length]))
        join(out, size, imageRoot, relative[length] ? relative + length + 1 : "");
    else
        join(out, size, gameDir, relative);
}

// The part of path in the game's folder: a relative path, or an absolute one in WRITE_DIR\<game>, in the game's
// folder, or in the Linux disk image it is in (IMAGE_FOLDER\...). NULL for the others: devices, root-relative Linux
// paths, other folders.
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
    char inImage[MAX_PATH];
    if (isAbsolute(normal))
    {
        rest = inside(normal, gameWriteDir);
        if (!rest)
            rest = inside(normal, gameDir);
        if (!rest && imageRoot[0] && (rest = inside(normal, imageRoot)) != NULL)
        {
            snprintf(inImage, sizeof(inImage), "%s%s%s", IMAGE_FOLDER, rest[0] ? "\\" : "", rest);
            rest = inImage;
        }
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

const char *writeDirRefused(void)
{
    return refused ? refusedFolder : NULL;
}

int writeDirGameFolder(char *buffer, size_t size)
{
    if (!active)
        return 0;
    snprintf(buffer, size, "%s", gameDir);
    return 1;
}

void writeDirImageRoot(const char *root)
{
    snprintf(imageRoot, sizeof(imageRoot), "%s", root);
    if (active)
        log_info("write folder: the Linux disk image's files written in %s\\%s", gameWriteDir, IMAGE_FOLDER);
}

const char *writeDirForRead(const char *path, char *buffer, size_t size)
{
    char relative[MAX_PATH], gameSide[MAX_PATH];
    if (refused)
    {
        const char *copy = shadowOf(path, 0, 0);
        return copy ? copy : path;
    }
    if (!relativePart(path, relative, sizeof(relative)))
        return path;
    join(buffer, size, gameWriteDir, relative);
    if (exists(buffer))
        return buffer;
    gameSideOf(gameSide, sizeof(gameSide), relative);
    if (exists(gameSide))
        snprintf(buffer, size, "%s", gameSide);
    return buffer;
}

const char *writeDirForWrite(const char *path, int keep, char *buffer, size_t size)
{
    char relative[MAX_PATH], gameSide[MAX_PATH];
    if (refused)
    {
        const char *copy = essential(path) ? shadowOf(path, 1, keep) : NULL;
        if (!copy)
        {
            refusedWrite("file", path);
            errno = EACCES;
        }
        return copy;
    }
    if (!relativePart(path, relative, sizeof(relative)))
        return path;
    join(buffer, size, gameWriteDir, relative);
    makeFolders(buffer, 0);
    if (keep && !exists(buffer))
    {
        gameSideOf(gameSide, sizeof(gameSide), relative);
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
    if (refused || !relativePart(path, relative, sizeof(relative)))
        return 0;
    join(writeSide, size, gameWriteDir, relative);
    gameSideOf(gameSide, size, relative);
    return 1;
}

FILE *writeDirFopen(const char *path, const char *mode)
{
    char buffer[MAX_PATH];
    if (!active)
        return fopen(path, mode);
    int write = strchr(mode, 'w') || strchr(mode, 'a') || strchr(mode, '+');
    // Refused: r+ needs the file, as anywhere (EEPROM code makes it with w+ when r+ fails)
    if (refused && write && strchr(mode, 'r') && !exists(path) && !shadowOf(path, 0, 0))
    {
        errno = ENOENT;
        return NULL;
    }
    const char *target = write ? writeDirForWrite(path, strchr(mode, 'w') == NULL, buffer, sizeof(buffer))
                               : writeDirForRead(path, buffer, sizeof(buffer));
    return target ? fopen(target, mode) : NULL; // NULL: a write refused
}

int writeDirOpen(const char *path, int flags, int mode)
{
    char buffer[MAX_PATH];
    if (!active)
        return _open(path, flags, mode);
    int write = (flags & (_O_WRONLY | _O_RDWR | _O_CREAT | _O_APPEND)) != 0;
    if (refused && write && !(flags & _O_CREAT) && !exists(path) && !shadowOf(path, 0, 0))
    {
        errno = ENOENT;
        return -1;
    }
    const char *target = write ? writeDirForWrite(path, (flags & _O_TRUNC) == 0, buffer, sizeof(buffer))
                               : writeDirForRead(path, buffer, sizeof(buffer));
    return target ? _open(target, flags, mode) : -1; // -1: a write refused
}

int writeDirMkdir(const char *path)
{
    char buffer[MAX_PATH];
    if (refused)
    {
        if (exists(path))
            errno = EEXIST;
        else
        {
            refusedWrite("folder", path);
            errno = EACCES;
        }
        return -1;
    }
    return _mkdir(writeDirForWrite(path, 0, buffer, sizeof(buffer)));
}

// A folder of the game's, in WRITE_DIR\<game>: made there when it is only in the game's folder (OutRun 2's chdir into
// shader\fs\psd before it reads its shaders by their bare names), so that the current folder stays in WRITE_DIR\<game>
// and the names read next are looked for from there, then in the game's folder
int writeDirChdir(const char *path)
{
    char relative[MAX_PATH], writeSide[MAX_PATH], gameSide[MAX_PATH];
    if (refused || !relativePart(path, relative, sizeof(relative)))
        return _chdir(path);
    join(writeSide, sizeof(writeSide), gameWriteDir, relative);
    gameSideOf(gameSide, sizeof(gameSide), relative);
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
    if (refused)
    {
        // A copy is removed (the file is read where it is again); the files of those folders never are
        if (shadowOf(path, 0, 0))
        {
            shadowForget(path);
            return 0;
        }
        if (!exists(path))
            errno = ENOENT;
        else
        {
            refusedWrite("removal of", path);
            errno = EACCES;
        }
        return -1;
    }
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
    if (refused)
    {
        // A file still written: its copy gets the content, the source stays as it is. Any other: refused
        const char *copy = shadowOf(from, 0, 0);
        const char *read = copy ? copy : exists(from) ? from : NULL;
        if (!read)
        {
            errno = ENOENT;
            return -1;
        }
        const char *written = essential(to) ? shadowOf(to, 1, 0) : NULL;
        if (!written || !CopyFileA(read, written, FALSE))
        {
            refusedWrite("rename to", to);
            errno = EACCES;
            return -1;
        }
        return 0;
    }
    const char *readFrom = writeDirForRead(from, source, sizeof(source));
    const char *writeTo = writeDirForWrite(to, 0, target, sizeof(target));
    int fromGame = ((gameDir[0] && inside(readFrom, gameDir)) || (imageRoot[0] && inside(readFrom, imageRoot))) &&
                   !inside(readFrom, gameWriteDir);
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
    char reason[128];
    if (!usable(rootDir, reason, sizeof(reason)))
    {
        refuse(rootDir, reason);
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
    if (refused)
    {
        active = 1; // the writes go through the write folder's functions, to be lost
        return;
    }
    join(gameWriteDir, sizeof(gameWriteDir), rootDir, name);
    makeFolders(gameWriteDir, 1);
    char reason[128];
    if (!usable(gameWriteDir, reason, sizeof(reason)))
    {
        refuse(gameWriteDir, reason);
        active = 1;
        return;
    }
    if (_chdir(gameWriteDir) != 0)
    {
        snprintf(reason, sizeof(reason), "it cannot be made the current folder (errno %d)", errno);
        refuse(gameWriteDir, reason);
        active = 1;
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

const char *writeDirRoot(void)
{
    return rootDir;
}

int writeDirRootFor(const char *writeDir, const char *configPath, char *out, size_t size)
{
    char fromIni[MAX_PATH] = "";
    if ((!writeDir || !writeDir[0]) && configPath && configPath[0])
    {
        IniConfig *ini = iniLoad(configPath);
        if (ini)
        {
            iniPath(ini, "WRITE_DIR", fromIni, sizeof(fromIni));
            iniFree(ini);
        }
        writeDir = fromIni;
    }
    out[0] = '\0';
    if (!writeDir || !writeDir[0])
        return 0;
    absoluteFolder(out, size, writeDir);
    return 1;
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

void writeDirImageRoot(const char *root)
{
    (void)root;
}

const char *writeDirRoot(void)
{
    return "";
}

int writeDirRootFor(const char *writeDir, const char *configPath, char *out, size_t size)
{
    (void)writeDir;
    (void)configPath;
    if (size)
        out[0] = '\0';
    return 0;
}

void writeDirTempWrites(const char *patterns)
{
    (void)patterns;
}

const char *writeDirRefused(void)
{
    return NULL;
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
