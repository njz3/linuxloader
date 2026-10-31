#ifndef _GNU_SOURCE
#define _GNU_SOURCE

#endif
#ifndef __i386__
#define __i386__
#endif
#undef __x86_64__

#include <dirent.h>
#include <libgen.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <stdarg.h>
#include <unistd.h>
#include "../config/config.h"
#include "../mainShared.h"
#include "../hardware/lindbergh/baseBoard.h"
#include "../hardware/lindbergh/pciData.h"
#include "../hardware/lindbergh/securityBoard.h"
#include "../hardware/lindbergh/eeprom.h"
#include "../hardware/lindbergh/cardReader.h"
#include "../hardware/lindbergh/motionBoard.h"
#include "../hardware/lindbergh/touchScreen.h"
#include "../hardware/lindbergh/driveBoard.h"
#include "../hardware/lindbergh/rideBoard.h"
#include "../graphics/shaderCache.h"
#include "../resources/font.h"
#include "../resources/lindberghLogo.h"
#include "../log/log.h"
#include "filesystemShared.h"
#include "writeDir.h"
#include "../hardware/lindbergh/serialPort.h"

#ifdef __linux__
#include <dlfcn.h>
#include <net/if.h>
#include <sys/ioctl.h>
#define REAL_FUNC(name) dlsym(RTLD_NEXT, #name)
#else
#include <direct.h>
#include <io.h>
#include <winsock2.h>
#include <iphlpapi.h>
#include <fcntl.h>
#include <io.h>
#define REAL_FUNC(name) name
#endif

#ifdef __linux__
#define HOOK_FILE_NAME "/dev/zero"
#else
#define HOOK_FILE_NAME "NUL"
#endif

int hooks[DEVICE_COUNT] = {NO_FD, NO_FD, NO_FD, NO_FD, NO_FD, NO_FD};
FILE *fileHooks[10] = {NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL};
FileTypes fileRead[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0};
char envpath[100];

int fontABCidx = 0;
int fontTGAidx = 0;
int logoTGAidx = 0;

extern uint32_t gId;
extern int gGrp;

bool phShowCursorInGame = false;
extern int hummerExtremeShaderFileIndex;
extern bool cachedShaderFilesLoaded;
extern char vf5StageNameAbbr[5];

extern bool eloResponseReady;

bool sprFontXstLoaded = false;

#ifdef _WIN32
// A game inside a Linux disk image (Gaelco's: <image>/home/joc2001/joc2001/salidas/gameport) opens its files by
// absolute paths: those are taken in the image, the first folder from the game's up with home, etc, lib and bin
static char s_linuxRoot[MAX_PATH] = "";

static void findLinuxRoot(void)
{
    static const char *const subdirs[] = {"home", "etc", "lib", "bin"};
    // From the game's folder: with a write folder, the current one is already WRITE_DIR\<game>
    char dir[MAX_PATH];
    if (!writeDirGameFolder(dir, sizeof(dir)) && !_getcwd(dir, sizeof(dir)))
        return;
    for (;;)
    {
        bool image = true;
        for (size_t i = 0; i < sizeof(subdirs) / sizeof(subdirs[0]) && image; i++)
        {
            char probe[MAX_PATH];
            snprintf(probe, sizeof(probe), "%s\\%s", dir, subdirs[i]);
            DWORD attributes = GetFileAttributesA(probe);
            image = attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY);
        }
        if (image)
        {
            strncpy(s_linuxRoot, dir, sizeof(s_linuxRoot) - 1);
            log_info("Linux disk image: absolute paths taken in %s", s_linuxRoot);
            writeDirImageRoot(s_linuxRoot);
            return;
        }
        char *slash = strrchr(dir, '\\');
        if (!slash || slash[1] == '\0') // the drive's root was the last one
            return;
        if (slash == dir + 2 && dir[1] == ':')
            slash[1] = '\0';
        else
            *slash = '\0';
    }
}

static const char *linuxRooted(const char *src, char *buffer, size_t size)
{
    static bool s_searched = false;
    if (!s_searched)
    {
        s_searched = true; // before the game's own chdir
        findLinuxRoot();
    }
    if (s_linuxRoot[0] == '\0' || src[0] != '/' || strncmp(src, "/dev/", 5) == 0 || strncmp(src, "/proc/", 6) == 0 ||
        strncmp(src, "/sys/", 5) == 0)
        return src;
    snprintf(buffer, size, "%s%s", s_linuxRoot, src);
    return buffer;
}
#endif

void ConvertPath(char *dst, const char *src, size_t size)
{
    if (!src || !dst)
        return;
#ifdef _WIN32
    char rooted[MAX_PATH];
    src = linuxRooted(src, rooted, sizeof(rooted));
#endif
    strncpy(dst, src, size - 1);
    dst[size - 1] = '\0';

    for (size_t i = 0; dst[i]; i++)
    {
        if (dst[i] == '/')
            dst[i] = '\\';
    }
}

static int (*_remove)(const char *path) = NULL;
int sharedRemove(const char *path)
{
    if (_remove == NULL)
#ifdef _WIN32
        _remove = writeDirRemove;
#else
        _remove = REAL_FUNC(remove);
#endif

    if (strncmp(path, "/home/disk1/rankingdata/", 24) == 0 && (gGrp == GROUP_OUTRUN || gGrp == GROUP_OUTRUN_TEST))
    {
        path += 12;
#ifdef _WIN32
        char winPath[256];
        ConvertPath(winPath, path, 256);
        path = winPath;
#endif
        log_debug("Removing: %s", path);
        return _remove(path);
    }

    if (strncmp(path, "/tmp/", 5) == 0) // the game folder's tmp, as open, unlink and mkdir take it
    {
        path += 1;
#ifdef _WIN32
        char winPath[256];
        ConvertPath(winPath, path, 256);
        path = winPath;
#endif
        log_info("Removing: %s", path);
        return _remove(path);
    }

    log_debug("Removing: %s", path);
    return _remove(path);
}

int sharedMkdir(const char *path, mode_t mode)
{
#ifdef __linux__
    static int (*_mkdir)(const char *path, mode_t mode) = NULL;
    if (_mkdir == NULL)
        _mkdir = REAL_FUNC(mkdir);
#endif

    if (strncmp(path, "/tmp", 4) == 0)
    {
        path += 1;
    }

    if (strcmp(path, "/home/disk1/rankingdata") == 0 && (gGrp == GROUP_OUTRUN || gGrp == GROUP_OUTRUN_TEST))
    {
#ifdef __linux__
        return _mkdir("./rankingdata", mode);
#else
        return writeDirMkdir(".\\rankingdata");
#endif
    }

    if (strncmp(path, "/home", 5) == 0 || strncmp(path, "/home/disk2", 11) == 0)
        return 0;

#ifdef __linux__
    log_debug("Creating directory: %s", path);
    return _mkdir(path, mode);
#else
    char winPath[MAX_PATH];
    ConvertPath(winPath, path, MAX_PATH);
    log_debug("Creating directory: %s", winPath);
    return writeDirMkdir(winPath);
#endif
}

#ifdef _WIN32
static int translateOpenFlags(int flags)
{
    int winFlags = _O_BINARY;

    // Access mode
    int accMode = flags & O_ACCMODE; // O_ACCMODE is 0x3
    if (accMode == 0)
        winFlags |= _O_RDONLY;
    else if (accMode == 1)
        winFlags |= _O_WRONLY;
    else if (accMode == 2)
        winFlags |= _O_RDWR;

    // Flags mapping
    if (flags & 0x40)
        winFlags |= _O_CREAT;
    if (flags & 0x80)
        winFlags |= _O_EXCL;
    if (flags & 0x200)
        winFlags |= _O_TRUNC;
    if (flags & 0x400)
        winFlags |= _O_APPEND;
    if (flags & 0x80000)
        winFlags |= _O_NOINHERIT;

    return winFlags;
}

static int translateOpenMode(int mode)
{
    int winMode = 0;
    if (mode & 0400)
        winMode |= _S_IREAD;
    if (mode & 0200)
        winMode |= _S_IWRITE;
    return winMode;
}
#endif

int sharedOpen(const char *pathname, int flags, ...)
{
#ifdef __linux__
    static int (*_open)(const char *pathname, int flags, ...) = NULL;
    if (_open == NULL)
        _open = REAL_FUNC(open);
#endif

    va_list args;
    va_start(args, flags);
    int mode = va_arg(args, int);
    va_end(args);

    if (strcmp(pathname, "/dev/lbb") == 0)
    {
        hooks[BASEBOARD] = _open(HOOK_FILE_NAME, flags, mode);
        return hooks[BASEBOARD];
    }

    if (strcmp(pathname, "/dev/i2c/0") == 0)
    {

        hooks[EEPROM] = _open(HOOK_FILE_NAME, flags, mode);
        return hooks[EEPROM];
    }

    if (strcmp(pathname, "/dev/ttyS0") == 0 || strcmp(pathname, "/dev/tts/0") == 0)
    {
        if (getConfig()->emulateDriveboard == 0 && getConfig()->emulateRideboard == 0 && getConfig()->emulateHW210CardReader == 0 &&
            getConfig()->emulateTouchscreen == 0)
#ifdef _WIN32
            return serialPortOpen(getConfig()->serial1Path, flags); // [Paths] SERIAL_1_PATH: a COM port
#else
            return _open(getConfig()->serial1Path, flags, mode);
#endif

        if (hooks[SERIAL0] != NO_FD && getConfig()->emulateHW210CardReader && gId != R_TUNED_SBQW)
            return hooks[SERIAL0];

        hooks[SERIAL0] = _open(HOOK_FILE_NAME, flags, mode);
        log_warn("SERIAL0 Opened %d\n", hooks[SERIAL0]);

        if (getConfig()->emulateHW210CardReader == 1 && gId != R_TUNED_SBQW)
            cardReaderSetFd(0, hooks[SERIAL0], getConfig()->cardFile1);

        return hooks[SERIAL0];
    }

    if (strcmp(pathname, "/dev/ttyS1") == 0 || strcmp(pathname, "/dev/tts/1") == 0)
    {
        // OutRun's drive board is on the first port alone, its motion board on this one
        bool outrunPorts = gGrp == GROUP_OUTRUN || gGrp == GROUP_OUTRUN_TEST;
        if ((getConfig()->emulateDriveboard == 0 || outrunPorts) && getConfig()->emulateMotionboard == 0 &&
            getConfig()->emulateHW210CardReader == 0 && getConfig()->emulateTouchscreen == 0)
#ifdef _WIN32
            return serialPortOpen(getConfig()->serial2Path, flags);
#else
            return _open(getConfig()->serial2Path, flags, mode);
#endif

        if (hooks[SERIAL1] != NO_FD && getConfig()->emulateHW210CardReader && gId != R_TUNED_SBQW)
            return hooks[SERIAL1];

        hooks[SERIAL1] = _open(HOOK_FILE_NAME, flags, mode);
        log_warn("SERIAL1 opened %d\n", hooks[SERIAL1]);

        if (getConfig()->emulateHW210CardReader == 1)
            cardReaderSetFd(1, hooks[SERIAL1], getConfig()->cardFile2);

        return hooks[SERIAL1];
    }

    if (strcmp(pathname, "/var/tmp/warning") == 0)
    {
        pathname = "./tmp/warning";
    }

    if (strncmp(pathname, "/var/tmp", 8) == 0)
        pathname += 5;

    if (strncmp(pathname, "/tmp/", 5) == 0)
        pathname += 1;

    if (strcmp(pathname, "/proc/bus/pci/01/00.0") == 0)
    {
        hooks[PCI_CARD_000] = _open(HOOK_FILE_NAME, flags, mode);
        return hooks[PCI_CARD_000];
    }

#ifdef _WIN32
    if (strcmp(pathname, "/proc/sys/kernel/osrelease") == 0)
        return -1;

    char winPath[MAX_PATH];
    ConvertPath(winPath, pathname, MAX_PATH);

    int winFlags = translateOpenFlags(flags);
    int winMode = translateOpenMode(mode);

    log_debug("Translating open flags: 0x%X -> 0x%X, mode: 0x%X -> 0x%X", flags, winFlags, mode, winMode);

    log_debug("Opening %s", winPath);
    return writeDirOpen(winPath, winFlags, winMode);
#endif

    log_debug("Opening %s", pathname);
    return _open(pathname, flags, mode);
}

int sharedOpen64(const char *pathname, int flags, ...)
{
    va_list args;
    va_start(args, flags);
    int mode = va_arg(args, int);
    va_end(args);

    return sharedOpen(pathname, flags, mode);
}

static FILE *(*_fopen)(const char *restrict pathname, const char *restrict mode) = NULL;
#ifdef _WIN32
// Linux has no text mode: a stream opened "r" or "w" reads and writes the bytes as they are. Windows' would turn CRLF
// into LF, and a file read into a buffer of its size would leave the end unwritten (Dead Heat's CRLF shaders). A 't'
// in the mode (Maximum Tune 3's "rt" for its movie table) is ignored by glibc; it becomes the 'b', since MSVC's C
// library refuses "rtb" and returns no file, where MinGW's accepts it.
const char *sharedBinaryMode(const char *mode, char *buffer, size_t size)
{
    if (!mode || strchr(mode, 'b') || strlen(mode) + 2 > size)
        return mode;
    size_t length = 0;
    for (const char *c = mode; *c; c++)
        buffer[length++] = *c == 't' ? 'b' : *c;
    if (!memchr(buffer, 'b', length))
        buffer[length++] = 'b';
    buffer[length] = 0;
    return buffer;
}
#endif

FILE *sharedFopen(const char *restrict pathname, const char *restrict mode)
{
    if (_fopen == NULL)
#ifdef _WIN32
        _fopen = writeDirFopen; // through the write folder, when there is one
#else
        _fopen = REAL_FUNC(fopen);
#endif
#ifdef _WIN32
    char binary[16];
    mode = sharedBinaryMode(mode, binary, sizeof(binary));
#endif

    if (strcmp(pathname, "/proc/net/route") == 0)
    {
        return NULL;
    }

    if (strcmp(pathname, "/root/lindbergrc") == 0)
    {
        return _fopen("lindbergrc", mode);
    }

    if ((strcmp(pathname, "/usr/lib/boot/logo.tga") == 0))
    {
        if (!getConfig()->disableBuiltinLogos)
        {
            fileRead[FILE_LOGO_TGA] = 0;
            fileHooks[FILE_LOGO_TGA] = _fopen(HOOK_FILE_NAME, mode);
            return fileHooks[FILE_LOGO_TGA];
        }
        else
        {
            return _fopen("logo.tga", mode);
        }
    }

    if (strcmp(pathname, "/usr/lib/boot/logo_red.tga") == 0)
    {
        if (!getConfig()->disableBuiltinLogos)
        {
            fileRead[FILE_LOGO_TGA] = 1;
            fileHooks[FILE_LOGO_TGA] = _fopen(HOOK_FILE_NAME, mode);
            return fileHooks[FILE_LOGO_TGA];
        }
        else
        {
            return _fopen("logo_red.tga", mode);
        }
    }

    if (strcmp(pathname, "/usr/lib/boot/LucidaConsole_12.tga") == 0)
    {
        if (!getConfig()->disableBuiltinFont)
        {
            fileRead[FILE_FONT_TGA] = 0;
            fileHooks[FILE_FONT_TGA] = _fopen(HOOK_FILE_NAME, mode);
            return fileHooks[FILE_FONT_TGA];
        }
        else
        {
            return _fopen("LucidaConsole_12.tga", mode);
        }
    }

    if (strcmp(pathname, "/usr/lib/boot/LucidaConsole_12.abc") == 0)
    {
        if (!getConfig()->disableBuiltinFont)
        {
            fileRead[FILE_FONT_ABC] = 0;
            fileHooks[FILE_FONT_ABC] = _fopen(HOOK_FILE_NAME, mode);
            return fileHooks[FILE_FONT_ABC];
        }
        else
        {
            return _fopen("LucidaConsole_12.abc", mode);
        }
    }

    if (strcmp(pathname, "/proc/cpuinfo") == 0)
    {
        fileRead[CPUINFO] = 0;
        fileHooks[CPUINFO] = _fopen(HOOK_FILE_NAME, mode);
        return fileHooks[CPUINFO];
    }

    if (strcmp(pathname, "/usr/lib/boot/SEGA_KakuGothic-DB-Roman_12.tga") == 0)
    {
        if (!getConfig()->disableBuiltinFont)
        {
            fileRead[FILE_FONT_TGA] = 0;
            fileHooks[FILE_FONT_TGA] = _fopen(HOOK_FILE_NAME, mode);
            return fileHooks[FILE_FONT_TGA];
        }
        else
        {
            return _fopen("SEGA_KakuGothic-DB-Roman_12.tga", mode);
        }
    }

    if (strcmp(pathname, "/usr/lib/boot/SEGA_KakuGothic-DB-Roman_12.abc") == 0)
    {
        if (!getConfig()->disableBuiltinFont)
        {
            fileRead[FILE_FONT_ABC] = 0;
            fileHooks[FILE_FONT_ABC] = _fopen(HOOK_FILE_NAME, mode);
            return fileHooks[FILE_FONT_ABC];
        }
        else
        {
            return _fopen("SEGA_KakuGothic-DB-Roman_12.abc", mode);
        }
    }

    if (strcmp(pathname, "/proc/bus/pci/00/1f.0") == 0)
    {
        fileRead[PCI_CARD_1F0] = 0;
        fileHooks[PCI_CARD_1F0] = _fopen(HOOK_FILE_NAME, mode);
        return fileHooks[PCI_CARD_1F0];
    }

    if (strcmp(pathname, "/var/tmp/warning") == 0)
    {
        return _fopen("warning", "wb");
    }

    char *newPathname;
    if ((newPathname = (char *)strstr(pathname, "/home/disk0")) != NULL)
    {
        memmove(newPathname + 2, newPathname + 11, strlen(newPathname + 11) + 1);
        memcpy(newPathname, "..", 2);
        pathname = newPathname;
    }

    if ((newPathname = (char *)strstr(pathname, "/home/disk1")) != NULL && (gGrp == GROUP_OUTRUN || gGrp == GROUP_OUTRUN_TEST))
    {
        pathname = newPathname + 12;
    }

    if ((strstr(pathname, "asm_lbg") != NULL) || (strstr(pathname, "asm_gl") != NULL))
    {
        return 0;
    }

    if (cachedShaderFilesLoaded)
    {
        int idx;
        if (shaderFileInList(pathname, &idx))
        {
            if (fileHooks[FILE_RW1] == NULL)
            {
                fileRead[FILE_RW1] = idx;
                fileHooks[FILE_RW1] = _fopen(pathname, mode);
                return fileHooks[FILE_RW1];
            }
            else if (fileHooks[FILE_RW2] == NULL)
            {
                fileRead[FILE_RW2] = idx;
                fileHooks[FILE_RW2] = _fopen(pathname, mode);
                return fileHooks[FILE_RW2];
            }
            else
            {
                log_fatal("Error intercepting fopen.");
                exit(1);
            }
        }
    }

    if (strncmp(pathname, "/tmp/", 5) == 0) // the game folder's tmp, as open, unlink and mkdir take it
    {
        pathname += 1;
    }

    if (strncmp(pathname, "/var/tmp/seatini", 9) == 0)
        pathname += 5;

    if(strstr(pathname, "spr_font_xst.gz") != NULL)
        sprFontXstLoaded = true;
            
#ifdef WIN32
    char winPath[MAX_PATH];
    ConvertPath(winPath, pathname, MAX_PATH);
#endif

    if (strcmp(getConfig()->idCardFolder, "") != 0)
    {
        if (gGrp == GROUP_ID5 || gGrp == GROUP_ID4_EXP || gGrp == GROUP_ID4_JAP)
        {
            if (strncmp(pathname, "InidCrd", 7) == 0 || strncmp(pathname, "InidCard", 8) == 0)
            {
                char *fmt = "%s%s";
                char newPathname[MAX_PATH_LENGTH];
                int newPathnameLen = strlen(getConfig()->idCardFolder) + strlen(pathname) + 1;
                char lastChar = getConfig()->idCardFolder[strlen(getConfig()->idCardFolder) - 1];
                if (lastChar != PATH_SEPARATOR)
                {
                    fmt = "%s%c%s";
                    newPathnameLen++;
                }

                snprintf(newPathname, newPathnameLen, fmt, getConfig()->idCardFolder, PATH_SEPARATOR, pathname);
                return fopen(newPathname, mode);
            }
        }
    }

    if (gId == PRIMEVAL_HUNT_SBPP)
    {
        if (strstr(pathname, "/data/lua/texture/start_stage") != NULL)
            phShowCursorInGame = true;
        else if (strstr(pathname, "/data/texture/weapon_select/") != NULL)
            phShowCursorInGame = false;
        else if (strstr(pathname, "/data/texture/stage_select/") != NULL)
            phShowCursorInGame = false;
        else if (strstr(pathname, "/data/texture/name_entry/") != NULL)
            phShowCursorInGame = false;
        else if (strstr(pathname, "/data/texture/game_end/") != NULL)
            phShowCursorInGame = false;
        else if (strstr(pathname, "/data/lua/stage/bonus_0") != NULL)
            phShowCursorInGame = true;
    }
#ifdef __linux__
    log_debug("fopen: %s with mode %s", pathname, mode);
    return _fopen(pathname, mode);
#else
    log_debug("fopen: %s (as %s) with mode %s", pathname, winPath, mode);
    return _fopen(winPath, mode);
#endif
}

static FILE *(*_fopen64)(const char *restrict pathname, const char *restrict mode) = NULL;
FILE *sharedFopen64(const char *pathname, const char *mode)
{
    if (_fopen64 == NULL)
#ifdef _WIN32
        _fopen64 = writeDirFopen;
#else
        _fopen64 = REAL_FUNC(fopen64);
#endif
#ifdef _WIN32
    char binary[16];
    mode = sharedBinaryMode(mode, binary, sizeof(binary));
#endif

    if (strcmp(pathname, "/proc/sys/kernel/osrelease") == 0)
    {
        fileRead[OSRELEASE] = 0;
        fileHooks[OSRELEASE] = _fopen64(HOOK_FILE_NAME, mode);
        return fileHooks[OSRELEASE];
    }

    if (strcmp(pathname, "/usr/lib/boot/logo_red.tga") == 0)
    {
        if (!getConfig()->disableBuiltinLogos)
        {
            fileRead[FILE_LOGO_TGA] = 1;
            fileHooks[FILE_LOGO_TGA] = _fopen64(HOOK_FILE_NAME, mode);
            return fileHooks[FILE_LOGO_TGA];
        }
        else
        {
            return _fopen64("logo_red.tga", mode);
        }
    }

    if (strcmp(pathname, "/usr/lib/boot/logo.tga") == 0)
    {
        if (!getConfig()->disableBuiltinLogos)
        {
            fileRead[FILE_LOGO_TGA] = 0;
            fileHooks[FILE_LOGO_TGA] = _fopen64(HOOK_FILE_NAME, mode);
            return fileHooks[FILE_LOGO_TGA];
        }
        else
        {
            return _fopen64("logo.tga", mode);
        }
    }

    if (strcmp(pathname, "/usr/lib/boot/SEGA_KakuGothic-DB-Roman_12.tga") == 0)
    {
        if (!getConfig()->disableBuiltinFont)
        {
            fileRead[FILE_FONT_TGA] = 0;
            fileHooks[FILE_FONT_TGA] = _fopen64(HOOK_FILE_NAME, mode);
            return fileHooks[FILE_FONT_TGA];
        }
        else
        {
            return _fopen64("SEGA_KakuGothic-DB-Roman_12.tga", mode);
        }
    }

    if (strcmp(pathname, "/usr/lib/boot/SEGA_KakuGothic-DB-Roman_12.abc") == 0)
    {
        if (!getConfig()->disableBuiltinFont)
        {
            fileRead[FILE_FONT_ABC] = 0;
            fileHooks[FILE_FONT_ABC] = _fopen64(HOOK_FILE_NAME, mode);
            return fileHooks[FILE_FONT_ABC];
        }
        else
        {
            return _fopen64("SEGA_KakuGothic-DB-Roman_12.abc", mode);
        }
    }

    int idx;

    if (gGrp == GROUP_HUMMER)
    {
        if (shaderFileInList(pathname, &idx))
        {
            hummerExtremeShaderFileIndex = idx;
        }
#ifdef _WIN32
        char winPath[MAX_PATH];
        ConvertPath(winPath, pathname, MAX_PATH);
        if (strcmp(mode, "r") == 0)
            mode = "rb";

        return _fopen64(winPath, mode);
#endif
        return _fopen64(pathname, mode);
    }

    switch (gId)
    {
        case VIRTUA_FIGHTER_5_FINAL_SHOWDOWN_SBUV_REVA:
        case VIRTUA_FIGHTER_5_FINAL_SHOWDOWN_SBXX_REVB:
        case VIRTUA_FIGHTER_5_FINAL_SHOWDOWN_SBXX_REVB_6000:
            if (getConfig()->GPUVendor != NVIDIA_GPU && getConfig()->GPUVendor != ATI_GPU)
            {
                char *filename = myBasename((char *)pathname);
                if (strstr(filename, "light_") || strstr(filename, "glow_"))
                {
                    char *start = strchr(filename, '_') + 1;
                    char *end = strstr(filename, ".txt");
                    strncpy(vf5StageNameAbbr, start, end - start);
                    vf5StageNameAbbr[end - start] = '\0';
                }
            }
    }

    // The game folder's tmp, as open, unlink and mkdir take it (Maximum Tune 3 saves its settings through C++ streams)
    if (strncmp(pathname, "/tmp/", 5) == 0)
        pathname += 1;

    log_debug("fopen64: %s with mode %s", pathname, mode);
    return _fopen64(pathname, mode);
}

static int (*_fclose)(FILE *stream) = NULL;
int sharedFclose(FILE *stream)
{
    if (_fclose == NULL)
        _fclose = REAL_FUNC(fclose);

    for (int i = 0; i < 9; i++)
    {
        if (fileHooks[i] == stream)
        {
            int r = _fclose(stream);
            fileHooks[i] = NULL;
            fileRead[i] = 0;
            if (stream == fileHooks[FILE_FONT_ABC])
                fontABCidx = 0;
            if (stream == fileHooks[FILE_FONT_TGA])
                fontTGAidx = 0;
            if (stream == fileHooks[FILE_LOGO_TGA])
                logoTGAidx = 0;
            return r;
        }
    }
    return _fclose(stream);
}

#ifdef _WIN32
// The descriptors the loader makes itself (eventfd, epoll): elfLoader/sysBridge.cpp
int sysVirtualFd(int fd);
int sysVirtualRead(int fd, void *buffer, size_t count);
int sysVirtualWrite(int fd, const void *buffer, size_t count);
int sysVirtualClose(int fd);
#endif

#ifndef __linux__
// Games read, write and close their sockets as descriptors, but on Windows they are Winsock handles, not CRT ones:
// read and write go to the network bridge's recv and send (Maximum Heat 3D's link talks with write)
int bridgeSend(SOCKET s, const char *buf, int len, int flags);
int bridgeRecv(SOCKET s, char *buf, int len, int flags);

// The games' sockets as they see them: small descriptors, 900 to 1023, as a socket's on Linux, each standing for a
// Winsock socket. A Winsock handle is a larger number, and a game's FD_SET would put its bit past its 1024-bit fd_set,
// on its stack: Hummer's nsFileDescriptorUtil::lCheckRead overwrote its caller's frame and crashed. A descriptor the C
// library has open stays a file (Ring Riders' model files were once read with recv, which waited)
#define SOCKET_FD_BASE 900
#define SOCKET_FD_COUNT 124
static SOCKET socketFds[SOCKET_FD_COUNT]; // 0: free
static SRWLOCK socketFdLock = SRWLOCK_INIT;

// A C library descriptor's handle, -1 for none, errno kept: _get_osfhandle sets it to EBADF for a free descriptor,
// and Maximum Tune 3's boost::asio reads errno, not the result, after an accept (an accept taken as failed: its
// server closed the connection and never listened again)
static intptr_t osHandleOf(int fd)
{
    int saved = errno;
    intptr_t handle = _get_osfhandle(fd);
    errno = saved;
    return handle;
}

int sharedSocketFdNew(SOCKET s)
{
    int fd = -1;
    AcquireSRWLockExclusive(&socketFdLock);
    for (int i = 0; i < SOCKET_FD_COUNT && fd < 0; i++)
    {
        if (!socketFds[i] && osHandleOf(SOCKET_FD_BASE + i) == -1)
        {
            socketFds[i] = s;
            fd = SOCKET_FD_BASE + i;
        }
    }
    ReleaseSRWLockExclusive(&socketFdLock);
    if (fd < 0)
        log_error("No socket descriptor left (%d in use)", SOCKET_FD_COUNT);
    return fd;
}

SOCKET sharedSocketOf(int fd)
{
    if (fd < SOCKET_FD_BASE || fd >= SOCKET_FD_BASE + SOCKET_FD_COUNT)
        return INVALID_SOCKET;
    AcquireSRWLockShared(&socketFdLock);
    SOCKET s = socketFds[fd - SOCKET_FD_BASE];
    ReleaseSRWLockShared(&socketFdLock);
    return s ? s : INVALID_SOCKET;
}

static void socketFdFree(int fd)
{
    AcquireSRWLockExclusive(&socketFdLock);
    if (fd >= SOCKET_FD_BASE && fd < SOCKET_FD_BASE + SOCKET_FD_COUNT)
        socketFds[fd - SOCKET_FD_BASE] = 0;
    ReleaseSRWLockExclusive(&socketFdLock);
}

static int isWinsockHandle(int fd)
{
    return sharedSocketOf(fd) != INVALID_SOCKET;
}

// The sockets made non-blocking (fcntl's O_NONBLOCK, FIONBIO), which Winsock cannot be asked about: fcntl's F_GETFL
// answers from them
#define NON_BLOCKING_SOCKETS 64
static SOCKET nonBlockingSockets[NON_BLOCKING_SOCKETS];
static SRWLOCK nonBlockingLock = SRWLOCK_INIT;

static void socketNonBlockingNote(SOCKET s, bool on)
{
    AcquireSRWLockExclusive(&nonBlockingLock);
    int free = -1, found = -1;
    for (int i = 0; i < NON_BLOCKING_SOCKETS; i++)
    {
        if (nonBlockingSockets[i] == s && s != 0)
            found = i;
        else if (nonBlockingSockets[i] == 0 && free < 0)
            free = i;
    }
    if (!on && found >= 0)
        nonBlockingSockets[found] = 0;
    else if (on && found < 0 && free >= 0)
        nonBlockingSockets[free] = s;
    ReleaseSRWLockExclusive(&nonBlockingLock);
}

static bool socketNonBlocking(SOCKET s)
{
    bool on = false;
    AcquireSRWLockShared(&nonBlockingLock);
    for (int i = 0; i < NON_BLOCKING_SOCKETS && !on; i++)
        on = nonBlockingSockets[i] == s;
    ReleaseSRWLockShared(&nonBlockingLock);
    return on;
}

// The sockets set O_ASYNC, whose data Linux signals with SIGIO (the link of Gaelco's Tuning Race and Ring Riders reads
// its data channel only in its SIGIO handler): sharedSocketsAsyncReadable tells the loader's SIGIO when to call it
#define ASYNC_SOCKETS 16
static SOCKET asyncSockets[ASYNC_SOCKETS];
static volatile LONG asyncSocketCount = 0;
static SRWLOCK asyncLock = SRWLOCK_INIT;

static void socketAsyncNote(SOCKET s, bool on)
{
    AcquireSRWLockExclusive(&asyncLock);
    int free = -1, found = -1;
    for (int i = 0; i < ASYNC_SOCKETS; i++)
    {
        if (asyncSockets[i] == s && s != 0)
            found = i;
        else if (asyncSockets[i] == 0 && free < 0)
            free = i;
    }
    if (!on && found >= 0)
    {
        asyncSockets[found] = 0;
        InterlockedDecrement(&asyncSocketCount);
    }
    else if (on && found < 0 && free >= 0)
    {
        asyncSockets[free] = s;
        InterlockedIncrement(&asyncSocketCount);
    }
    ReleaseSRWLockExclusive(&asyncLock);
}

static bool socketAsync(SOCKET s)
{
    bool on = false;
    AcquireSRWLockShared(&asyncLock);
    for (int i = 0; i < ASYNC_SOCKETS && !on; i++)
        on = asyncSockets[i] == s;
    ReleaseSRWLockShared(&asyncLock);
    return on;
}

bool sharedSocketsAsync(void)
{
    return asyncSocketCount > 0;
}

// Whether an O_ASYNC socket has data waiting, after waiting for some up to timeoutMs
bool sharedSocketsAsyncReadable(int timeoutMs)
{
    if (asyncSocketCount <= 0)
        return false;
    fd_set readable;
    FD_ZERO(&readable);
    AcquireSRWLockShared(&asyncLock);
    for (int i = 0; i < ASYNC_SOCKETS; i++)
        if (asyncSockets[i])
            FD_SET(asyncSockets[i], &readable);
    ReleaseSRWLockShared(&asyncLock);
    if (readable.fd_count == 0)
        return false;
    struct timeval timeout = {timeoutMs / 1000, (timeoutMs % 1000) * 1000};
    return select(0, &readable, NULL, NULL, &timeout) > 0;
}

#define LINUX_O_RDWR 02
#define LINUX_O_NONBLOCK 04000
#define LINUX_O_ASYNC 020000
#define LINUX_F_GETFL 3
#define LINUX_F_SETFL 4

// fcntl on a socket: F_GETFL its Linux flags, F_SETFL its O_NONBLOCK through FIONBIO and its O_ASYNC. A socket left
// blocking hangs the games that poll it each frame (OutRun 2's and Hummer's link, set non-blocking this way). False:
// not a socket
bool sharedSocketFcntl(int fd, int cmd, int arg, int *result)
{
    SOCKET s = sharedSocketOf(fd);
    if (s == INVALID_SOCKET)
        return false;
    *result = 0;
    if (cmd == LINUX_F_GETFL)
        *result = LINUX_O_RDWR | (socketNonBlocking(s) ? LINUX_O_NONBLOCK : 0) | (socketAsync(s) ? LINUX_O_ASYNC : 0);
    else if (cmd == LINUX_F_SETFL)
    {
        u_long on = (arg & LINUX_O_NONBLOCK) != 0;
        if (ioctlsocket(s, FIONBIO, &on) == 0)
            socketNonBlockingNote(s, on != 0);
        else
            *result = -1;
        if ((arg & LINUX_O_ASYNC) != 0 && !socketAsync(s))
            log_info("Socket %d set O_ASYNC: its data comes to the game's SIGIO handler", fd);
        socketAsyncNote(s, (arg & LINUX_O_ASYNC) != 0);
    }
    return true;
}
#endif

int sharedClose(int fd)
{
#ifdef __linux__
    static int (*_close)(int fd) = NULL;
    if (_close == NULL)
        _close = REAL_FUNC(close);
#else
    if (sysVirtualFd(fd))
        return sysVirtualClose(fd);
    if (serialPortIs(fd))
        return serialPortClose(fd);
#endif
    // The standard streams stay the loader's: the agent's commands and answers, the log. Dead Heat Riders' camera
    // library closes descriptor 0 when no camera opened (its descriptor still at 0); Ring Riders closes stdout
    if (fd >= 0 && fd <= 2)
    {
        log_info("close(%d) by the game: kept open", fd);
        return 0;
    }

    for (size_t i = 0; i < (sizeof hooks / sizeof hooks[0]); i++)
    {
        if (hooks[i] == fd)
        {
            hooks[i] = NO_FD;
            return 0;
        }
    }

#ifndef __linux__
    if (isWinsockHandle(fd))
    {
        log_info(">>> close: socket=%d", fd);
        SOCKET s = sharedSocketOf(fd);
        socketNonBlockingNote(s, false);
        socketAsyncNote(s, false);
        socketFdFree(fd);
        return closesocket(s) == 0 ? 0 : -1;
    }
#endif

    return _close(fd);
}

static char *(*_fgets)(char *str, int n, FILE *stream) = NULL;
// The cpuinfo sharedFgets answers ends after its four lines (Maximum Tune 3 reads it until feof)
int sharedFeof(FILE *stream)
{
    if (stream && stream == fileHooks[CPUINFO])
        return fileRead[CPUINFO] == 4;
    return feof(stream);
}

char *sharedFgets(char *str, int n, FILE *stream)
{
    if (_fgets == NULL)
        _fgets = REAL_FUNC(fgets);

    if (stream == fileHooks[OSRELEASE])
    {
        char *contents = "mvl";
        strcpy(str, contents);
        return str;
    }

    if (stream == fileHooks[CPUINFO])
    {
        char contents[4][256];

        strcpy(contents[0], "processor	: 0");
        strcpy(contents[1], "vendor_id	: GenuineIntel");
        strcpy(contents[2], "model		: 142");
        strcpy(contents[3], "model name	: Intel(R) Pentium(R) CPU 3.00GHz");

        if (getConfig()->lindberghColour == RED || getConfig()->lindberghColour == REDEX)
            strcpy(contents[3], "model name	: Intel(R) Celeron(R) CPU 2.80GHz");

        if (fileRead[CPUINFO] == 4)
            return NULL;

        strcpy(str, contents[fileRead[CPUINFO]++]);
        return str;
    }

    return _fgets(str, n, stream);
}

ssize_t sharedRead(int fd, void *buf, size_t count)
{
#ifdef __linux__
    static ssize_t (*_read)(int fd, void *buf, size_t count) = NULL;
    if (_read == NULL)
        _read = REAL_FUNC(read);
#else
    if (sysVirtualFd(fd))
        return sysVirtualRead(fd, buf, count);
    if (serialPortIs(fd))
        return serialPortRead(fd, buf, count);
#endif

    if (fd < 0)
    {
        errno = EBADF;
        return -1;
    }

    if (fd == hooks[BASEBOARD])
    {
        return baseboardRead(fd, buf, count);
    }

    if (fd == hooks[SERIAL0] && getConfig()->emulateRideboard)
    {
        return rideboardRead(fd, buf, count);
    }

    if (fd == hooks[SERIAL0] && getConfig()->emulateDriveboard)
    {
        return driveboardRead(fd, buf, count);
    }

    if (gGrp == GROUP_VF5 && getConfig()->vf5CabinetMode == TERMINAL)
    {
        if (fd == hooks[SERIAL0])
            return cardReaderRead(fd, buf, count);

        if (fd == hooks[SERIAL1])
            return eloReadTouchPacket(buf, count);
    }

    if ((fd == hooks[SERIAL0] || fd == hooks[SERIAL1]) && getConfig()->emulateHW210CardReader)
    {
        return cardReaderRead(fd, buf, count);
    }

    if (fd == hooks[SERIAL0] || fd == hooks[SERIAL1])
    {
        if (gId == PRIMEVAL_HUNT_SBPP && getConfig()->emulateTouchscreen == 1)
        {
            phRead(fd, buf, count);
            return 1;
        }
        if ((gId == MJ4_EVO_SBTA || gId == MJ4_SBPN_REVG || gId == QUIZ_AXA_SBMS || gId == QUIZ_AXA_SBUR_LIVE) &&
            getConfig()->emulateTouchscreen == 1 && eloResponseReady)
        {
            return eloReadTouchPacket(buf, count);
        }
    }

    if (fd == hooks[PCI_CARD_000])
    {
        memcpy(buf, pci_000, count);
        return count;
    }

#ifndef __linux__
    if (isWinsockHandle(fd))
        return bridgeRecv((SOCKET)fd, (char *)buf, (int)count, 0);
#endif
    return _read(fd, buf, count);
}

static size_t (*_fread)(void *buf, size_t size, size_t count, FILE *stream) = NULL;
size_t sharedFread(void *buf, size_t size, size_t count, FILE *stream)
{
    if (_fread == NULL)
        _fread = REAL_FUNC(fread);

    if (stream == fileHooks[PCI_CARD_1F0])
    {
        memcpy(buf, pci_1f0, size * count);
        return size * count;
    }

    if (stream == fileHooks[FILE_RW1])
    {
        return freadReplace(buf, size, count, fileRead[FILE_RW1]);
    }

    if (stream == fileHooks[FILE_RW2])
    {
        return freadReplace(buf, size, count, fileRead[FILE_RW2]);
    }

    if (stream == fileHooks[FILE_FONT_ABC])
    {
        memcpy(buf, fontABC + fontABCidx, size * count);
        fontABCidx += (size * count);
        return size * count;
    }

    if (stream == fileHooks[FILE_FONT_TGA])
    {
        memcpy(buf, fontTGA + fontTGAidx, size * count);
        fontTGAidx += (size * count);
        return size * count;
    }

    if (stream == fileHooks[FILE_LOGO_TGA])
    {
        if (fileRead[FILE_LOGO_TGA] == 0)
            memcpy(buf, lindberghLogo + logoTGAidx, size * count);
        else
            memcpy(buf, lindberghLogoRed + logoTGAidx, size * count);
        logoTGAidx += (size * count);
        return size * count;
    }

    return _fread(buf, size, count, stream);
}

static long int (*_ftell)(FILE *stream) = NULL;
long int sharedFtell(FILE *stream)
{
    if (_ftell == NULL)
        _ftell = REAL_FUNC(ftell);

    if (stream == fileHooks[FILE_RW1])
    {
        return ftellGetShaderSize(fileRead[FILE_RW1]);
    }
    if (stream == fileHooks[FILE_RW2])
    {
        return ftellGetShaderSize(fileRead[FILE_RW2]);
    }

    if (stream == fileHooks[FILE_FONT_ABC])
    {
        return fontABClen;
    }

    return _ftell(stream);
}

static int (*_fseek)(FILE *stream, long int offset, int whence) = NULL;
int sharedFseek(FILE *stream, long int offset, int whence)
{
    if (_fseek == NULL)
        _fseek = REAL_FUNC(fseek);

    if (stream == fileHooks[FILE_FONT_ABC])
    {
        switch (whence)
        {
            case SEEK_CUR:
                break;
            case SEEK_SET:
                fontABCidx = 0;
                break;
            case SEEK_END:
                fontABCidx = fontABClen;
                break;
        }
        return fontABCidx;
    }

    return _fseek(stream, offset, whence);
}

static void (*_rewind)(FILE *stream) = NULL;
void sharedRewind(FILE *stream)
{
    if (_rewind == NULL)
        _rewind = REAL_FUNC(rewind);

    if (stream == fileHooks[FILE_FONT_ABC])
    {
        fontABCidx = 0;
        return;
    }

    _rewind(stream);
}

ssize_t sharedWrite(int fd, const void *buf, size_t count)
{
#ifdef __linux__
    static ssize_t (*_write)(int fd, const void *buf, size_t count) = NULL;
    if (_write == NULL)
        _write = REAL_FUNC(write);
#else
    if (sysVirtualFd(fd))
        return sysVirtualWrite(fd, buf, count);
    if (serialPortIs(fd))
        return serialPortWrite(fd, buf, count);
#endif

    if (fd < 0)
    {
        errno = EBADF;
        return -1;
    }

    // void *addr = __builtin_return_address(0);
    if (fd == hooks[BASEBOARD])
    {
        return baseboardWrite(fd, buf, count);
    }

    if (fd == hooks[SERIAL0] && getConfig()->emulateRideboard)
    {
        return rideboardWrite(fd, buf, count);
    }

    if (fd == hooks[SERIAL0] && getConfig()->emulateDriveboard)
    {
        return driveboardWrite(fd, buf, count);
    }

    if (fd == hooks[SERIAL1] && getConfig()->emulateDriveboard)
    {
        if (gGrp != GROUP_OUTRUN && gGrp != GROUP_OUTRUN_TEST && gId != R_TUNED_SBQW)
            return driveboardWrite(fd, buf, count);
    }

    if (gGrp == GROUP_VF5 && getConfig()->vf5CabinetMode == (VF5CabinetMode)TERMINAL)
    {
        if (fd == hooks[SERIAL0])
            return cardReaderWrite(fd, buf, count);

        if (fd == hooks[SERIAL1])
            return eloWriteTouchPacket(buf, count);
    }

    if (fd == hooks[SERIAL1] && (gId == MJ4_EVO_SBTA || gId == MJ4_SBPN_REVG || gId == QUIZ_AXA_SBMS || gId == QUIZ_AXA_SBUR_LIVE))
    {
        return eloWriteTouchPacket(buf, count);
    }

    if ((fd == hooks[SERIAL0] || fd == hooks[SERIAL1]) && getConfig()->emulateHW210CardReader)
    {
        return cardReaderWrite(fd, buf, count);
    }

#ifndef __linux__
    if (isWinsockHandle(fd))
        return bridgeSend((SOCKET)fd, (const char *)buf, (int)count, 0);
#endif
    return _write(fd, buf, count);
}


#ifndef __linux__
// Linux socket ioctls. FIONBIO/FIONREAD map to ioctlsocket(): without FIONBIO the game's UDP sockets stay blocking
// and its per-frame recvfrom() polling hangs the main loop (SEGA Race TV link setup). Windows has no ifreq interface:
// report one "eth0" with the IP the game's link setup expects (SRTV_IPADDRESS for SEGA Race TV, the cabinet's address
// on a link for OutRun 2, Hummer and Initial D, else loopback), so the IP lookups line up with the inet_addr()
// redirections.
#define LINUX_FIONREAD 0x541b
#define LINUX_FIONBIO 0x5421
#define LINUX_SIOCGIFFLAGS 0x8913
#define LINUX_SIOCGIFADDR 0x8915
#define LINUX_SIOCGIFBRDADDR 0x8919
#define LINUX_SIOCGIFNETMASK 0x891b
#define LINUX_SIOCGIFHWADDR 0x8927
#define LINUX_SIOCGMIIPHY 0x89F0
#define LINUX_SIOCGMIIREG 0x89F1

typedef struct
{
    char ifrName[16];
    union
    {
        struct
        {
            uint16_t family;
            uint16_t port;
            uint32_t addr;
            uint8_t zero[8];
        } in;
        struct
        {
            uint16_t family;
            uint8_t data[14];
        } hw;
        int16_t flags;
    } u;
} LinuxIfreq;

// eth0 as the games see it: its address and a MAC. Dead Heat's engine sets the address itself (sudo ifconfig eth0
// 192.168.GROUP.ID ...) and reads it back: the one it set is kept, as Linux would
static char eth0Set[16] = "";

// Maximum Heat 3D and Dead Heat Riders keep their network group out of the cabinets' network (32 to 63, NET GROUP 32
// as it comes; Dead Heat Riders 91 to 99), so their address never is in it: 192.168.G.H, H = 105 + 10 * TOTAL
// MACHINES + PCB ID - 1 (Maximum Heat 3D), 10 * TOTAL + PCB ID (Dead Heat Riders, its TOTAL set to 4 by its test
// menu). Their link takes the other cabinets' status only from its own /24 (clNet::receive<clProtocolStatusData>), and
// they come from their PCs' addresses (192.168.1.x): each cabinet stays alone ("ID:2 CHECKING...", LINKED= 1). On a
// link (TOTAL MACHINES 2 or more), the address goes into this PC's network, as Dead Heat's with NET GROUP 1:
// 192.168.32.125 becomes 192.168.1.125. Their group, compared between cabinets only, stays their own
static void namcoLinkAddress(char *address, size_t size)
{
    uint32_t game = ntohl(inet_addr(address)), lan, mask;
    int low = (int)(game & 0xFF), total;
    const char *name;
    if (gId == MAXIMUM_HEAT_3D)
        name = "Maximum Heat 3D", total = low >= 105 ? (low - 105) / 10 : 0;
    else if (gId == DEAD_HEAT_RIDERS)
        name = "Dead Heat Riders", total = (low - 1) / 10;
    else
        return;
    if (sharedLocalLinkAddress() || total < 2 || !sharedLanAddress(&lan, &mask) || ((ntohl(lan) ^ game) & 0xFFFFFF00) == 0)
        return;
    game = (ntohl(lan) & 0xFFFFFF00) | (game & 0xFF);
    char moved[16];
    snprintf(moved, sizeof(moved), "%u.%u.%u.%u", game >> 24, (game >> 16) & 0xFF, (game >> 8) & 0xFF, game & 0xFF);
    log_warn("Link: %s's address %s taken as %s, in this PC's network", name, address, moved);
    snprintf(address, size, "%s", moved);
}

void sharedEth0Set(const char *ifconfigArguments)
{
    char address[16] = "";
    if (sscanf(ifconfigArguments, "%15[0-9.]", address) == 1 && inet_addr(address) != INADDR_NONE)
    {
        namcoLinkAddress(address, sizeof(address));
        strcpy(eth0Set, address);
    }
}

const char *sharedEth0SetAddress(void)
{
    return eth0Set[0] ? eth0Set : NULL;
}

// Whether an address is one of this PC's: a socket binds to it
static bool isLocalAddress(const char *address)
{
    bool local = false;
    WSADATA data;
    if (address[0] && inet_addr(address) != INADDR_NONE && WSAStartup(MAKEWORD(2, 2), &data) == 0)
    {
        SOCKET probe = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        struct sockaddr_in in = {0};
        in.sin_family = AF_INET;
        in.sin_addr.s_addr = inet_addr(address);
        local = probe != INVALID_SOCKET && bind(probe, (struct sockaddr *)&in, sizeof(in)) == 0;
        if (probe != INVALID_SOCKET)
            closesocket(probe);
        WSACleanup();
    }
    return local;
}

// SEGA Race TV's address (SRTV_IPADDRESS, empty: loopback) must be one of the PC's, or the game dies in
// klib::lan::GetLanParam: one no socket binds to (the default file's 192.168.1.2 on another network) becomes loopback
const char *sharedSrtvAddress(void)
{
    static char address[16] = "";
    if (!address[0])
    {
        snprintf(address, sizeof(address), "%s", getConfig()->srtvIP[0] ? getConfig()->srtvIP : "127.0.0.1");
        if (!isLocalAddress(address))
        {
            log_warn("SRTV_IPADDRESS %s is not one of this PC's addresses: 127.0.0.1 taken instead", address);
            strcpy(address, "127.0.0.1");
        }
        else if (strcmp(address, "127.0.0.1") != 0)
            log_warn("Link: this cabinet's address is %s (SRTV_IPADDRESS)", address);
    }
    return address;
}

// The link on one PC: LOCAL_LINK_ADDRESS, a loopback address other than 127.0.0.1. NULL otherwise
static const char *localLinkConfigured(void)
{
    const char *address = getConfig()->localLinkIP;
    return strncmp(address, "127.", 4) == 0 && strcmp(address, "127.0.0.1") != 0 && inet_addr(address) != INADDR_NONE
               ? address
               : NULL;
}

// The cabinet's address on a link of two PCs, as the game's own keys give it ([Network], with the network patches):
// OR2_IPADDRESS for OutRun 2, the IP_CAB1 to 4 that is this PC's for Hummer, the ID_IP_SEAT_1 or 2 that is this PC's
// for Initial D 4 Export and Initial D 5. eth0 is then that address, as the game's own on a cabinet. NULL: none is
// this PC's, eth0 stays loopback and the game plays alone
static const char *linkAddress(void)
{
    static bool chosen = false;
    static char address[16] = "";
    if (chosen)
        return address[0] ? address : NULL;
    chosen = true;
    EmulatorConfig *config = getConfig();
    // The link on one PC: every 127.x address is this PC's, LOCAL_LINK_ADDRESS says which one this copy is
    if (localLinkConfigured())
    {
        snprintf(address, sizeof(address), "%s", config->localLinkIP);
        log_warn("Link on one PC: this copy's address is %s (LOCAL_LINK_ADDRESS)", address);
        return address;
    }
    if (!config->enableNetworkPatches)
        return NULL;
    struct
    {
        const char *key, *value;
    } candidates[4] = {{NULL, NULL}};
    if (gGrp == GROUP_OUTRUN || gGrp == GROUP_OUTRUN_TEST)
        candidates[0].key = "OR2_IPADDRESS", candidates[0].value = config->or2IP;
    else if (gGrp == GROUP_HUMMER)
    {
        const char *cabs[4] = {config->IpCab1, config->IpCab2, config->IpCab3, config->IpCab4};
        static const char *const keys[4] = {"IP_CAB1", "IP_CAB2", "IP_CAB3", "IP_CAB4"};
        for (int i = 0; i < 4; i++)
            candidates[i].key = keys[i], candidates[i].value = cabs[i];
    }
    else if (gGrp == GROUP_ID4_EXP || gGrp == GROUP_ID4_JAP || gGrp == GROUP_ID5)
    {
        candidates[0].key = "ID_IP_SEAT_1", candidates[0].value = config->idIpSeat1;
        candidates[1].key = "ID_IP_SEAT_2", candidates[1].value = config->idIpSeat2;
    }
    for (int i = 0; i < 4 && candidates[i].key; i++)
    {
        if (isLocalAddress(candidates[i].value) && strcmp(candidates[i].value, "127.0.0.1") != 0)
        {
            snprintf(address, sizeof(address), "%s", candidates[i].value);
            log_warn("Link: this cabinet's address is %s (%s)", address, candidates[i].key);
            return address;
        }
    }
    if (candidates[0].key)
        log_info("Link: none of %s... is one of this PC's addresses, eth0 is 127.0.0.1 (the game plays alone)",
                 candidates[0].key);
    return NULL;
}

static const char *eth0Address(void)
{
    // The link on one PC: this copy's address, before the one a Namco game sets (its peers are in that /24)
    if (localLinkConfigured() && linkAddress())
        return linkAddress();
    if (getConfig()->crc32 == SEGA_RACE_TV_SBPF && strcmp(getConfig()->srtvIP, "") != 0)
        return sharedSrtvAddress();
    if (eth0Set[0])
        return eth0Set;
    const char *link = linkAddress();
    return link ? link : "127.0.0.1";
}

const char *sharedLocalLinkAddress(void)
{
    return localLinkConfigured() ? linkAddress() : NULL;
}

static const uint8_t eth0Mac[6] = {0x00, 0xD0, 0xF1, 0x00, 0x00, 0x01}; // SEGA OUI

// Maximum Tune 3 and 3DX+ run in Namco's debug mode under the loader (started without an argument: seqMain(argc ==
// 1)), where they set no address (no sudo ifconfig) and only read eth0's from ifconfig.pl: on a link, TOTAL MACHINE 2
// or more in their test menu (TotalMachine 1 or more in tmp/data/testmode.lua, 3DX+'s tmp/data/etc/testmode.lua), eth0
// is this PC's address on its network. Their link takes the other cabinets' status from its /24 only
// (clNet::receive<clProtocolStatusData>) and connects to their sources, the other PCs. Alone, it stays as before (no
// network)
static bool maximumTuneLinked(void)
{
    if ((gId != WANGAN_MIDNIGHT_MT3 && gId != WANGAN_MIDNIGHT_MT3DX) || eth0Set[0] || sharedLocalLinkAddress())
        return false;
    FILE *file =
        writeDirFopen(gId == WANGAN_MIDNIGHT_MT3DX ? "tmp\\data\\etc\\testmode.lua" : "tmp\\data\\testmode.lua", "r");
    if (!file)
        return false;
    char line[256];
    int total = 0;
    while (fgets(line, sizeof(line), file))
        if (sscanf(line, " TotalMachine = %d", &total) == 1)
            break;
    fclose(file);
    return total >= 1;
}

// Namco's ifconfig.pl (Dead Heat's engine), which the game runs through sudo perl into a file it reads: eth0 as
// "interface address mask MAC link", the same as the ioctls below. /tmp is the game folder's tmp, as the other calls
// take it (Maximum Tune 3's /tmp/ifconfig.txt went to the drive's root)
int sharedNamcoIfconfig(const char *path)
{
    uint32_t lan, mask;
    if (maximumTuneLinked() && sharedLanAddress(&lan, &mask))
    {
        struct in_addr in;
        in.s_addr = lan;
        snprintf(eth0Set, sizeof(eth0Set), "%s", inet_ntoa(in));
        log_warn("Link: Maximum Tune's address is this PC's, %s", eth0Set);
    }
    unsigned a[4] = {127, 0, 0, 1};
    sscanf(eth0Address(), "%u.%u.%u.%u", &a[0], &a[1], &a[2], &a[3]);
    if (strncmp(path, "/tmp/", 5) == 0)
        path++;
    char winPath[MAX_PATH];
    ConvertPath(winPath, path, MAX_PATH);
    FILE *file = writeDirFopen(winPath, "w");
    if (!file)
        return 1 << 8;
    // The copies of the loader on one PC: one MAC each, as SIOCGIFHWADDR gives it
    fprintf(file, "0 %u %u %u %u 255 255 255 0 %u %u %u %u %u %u 1\n", a[0], a[1], a[2], a[3], eth0Mac[0], eth0Mac[1],
            eth0Mac[2], eth0Mac[3], eth0Mac[4], sharedLocalLinkAddress() ? a[3] : eth0Mac[5]);
    fclose(file);
    return 0;
}

static int windowsSocketIoctl(int fd, unsigned long int request, void *argp)
{
    if (request == LINUX_FIONBIO || request == LINUX_FIONREAD)
    {
        u_long value = (request == LINUX_FIONBIO && argp) ? (u_long)(*(int *)argp != 0) : 0;
        SOCKET s = sharedSocketOf(fd);
        if (s == INVALID_SOCKET || ioctlsocket(s, request == LINUX_FIONBIO ? FIONBIO : FIONREAD, &value) != 0)
            return -1;
        if (request == LINUX_FIONBIO)
            socketNonBlockingNote(s, value != 0);
        if (request == LINUX_FIONREAD && argp)
            *(int *)argp = (int)value;
        return 0;
    }

    LinuxIfreq *ifr = (LinuxIfreq *)argp;
    if (!ifr)
        return 0;

    const char *ip = eth0Address();

    switch (request)
    {
        case LINUX_SIOCGIFADDR:
        case LINUX_SIOCGIFNETMASK:
        case LINUX_SIOCGIFBRDADDR:
        {
            uint32_t addr = inet_addr(ip);
            uint32_t mask = htonl(0xFFFFFF00);
            memset(&ifr->u, 0, sizeof(ifr->u));
            ifr->u.in.family = AF_INET;
            if (request == LINUX_SIOCGIFADDR)
                ifr->u.in.addr = addr;
            else if (request == LINUX_SIOCGIFNETMASK)
                ifr->u.in.addr = mask;
            else
                ifr->u.in.addr = addr | ~mask;
            return 0;
        }
        case LINUX_SIOCGIFFLAGS:
            ifr->u.flags = 0x1 | 0x2 | 0x40 | 0x1000; // IFF_UP | IFF_BROADCAST | IFF_RUNNING | IFF_MULTICAST
            return 0;
        case LINUX_SIOCGIFHWADDR:
        {
            memset(&ifr->u, 0, sizeof(ifr->u));
            ifr->u.hw.family = 1; // ARPHRD_ETHER
            memcpy(ifr->u.hw.data, eth0Mac, sizeof(eth0Mac));
            // The copies of the loader on one PC: one MAC each, from the last number of their address. Gaelco's
            // cabinets on a network: one each, from the PC's address there (Ring Riders numbers its cabinets in the
            // order of their MACs)
            uint32_t lan, lanMask;
            if (sharedLocalLinkAddress())
                ifr->u.hw.data[5] = (uint8_t)(ntohl(inet_addr(ip)) & 0xFF);
            else if (gGrp == GROUP_GAELCO && sharedLanAddress(&lan, &lanMask))
            {
                ifr->u.hw.data[4] = (uint8_t)(ntohl(lan) >> 8);
                ifr->u.hw.data[5] = (uint8_t)ntohl(lan);
            }
            return 0;
        }
        // The cable's state, from the PHY's registers (mii_ioctl_data after the name: phy_id, reg_num, val_in,
        // val_out). Ring Riders links only with its cable in: BMSR (register 1) with the link up and autonegotiation
        // done, as a 100 Mb/s PHY gives it. SIOCGMIIPHY reads the register asked as well
        case LINUX_SIOCGMIIPHY:
        case LINUX_SIOCGMIIREG:
        {
            uint16_t *mii = (uint16_t *)((char *)ifr + 16);
            if (request == LINUX_SIOCGMIIPHY)
                mii[0] = 1;
            mii[3] = mii[1] == 1 ? 0x782D : 0;
            return 0;
        }
        default:
            return 0;
    }
}

// This PC's address on its network, and its mask: the adapter Windows reaches the Internet through, so not a VPN's
bool sharedLanAddress(uint32_t *address, uint32_t *mask)
{
    static int found = -1;
    static uint32_t lanAddress, lanMask;
    if (found < 0)
    {
        found = 0;
        DWORD index = 0;
        ULONG size = 0;
        if (GetBestInterface(inet_addr("8.8.8.8"), &index) == NO_ERROR &&
            GetAdaptersInfo(NULL, &size) == ERROR_BUFFER_OVERFLOW)
        {
            IP_ADAPTER_INFO *adapters = (IP_ADAPTER_INFO *)malloc(size);
            if (adapters && GetAdaptersInfo(adapters, &size) == NO_ERROR)
            {
                for (IP_ADAPTER_INFO *adapter = adapters; adapter && !found; adapter = adapter->Next)
                {
                    if (adapter->Index != index)
                        continue;
                    lanAddress = inet_addr(adapter->IpAddressList.IpAddress.String);
                    lanMask = inet_addr(adapter->IpAddressList.IpMask.String);
                    found = lanAddress != 0 && lanAddress != INADDR_NONE;
                    if (found)
                        log_info("This PC on its network: %s, mask %s", adapter->IpAddressList.IpAddress.String,
                                 adapter->IpAddressList.IpMask.String);
                }
            }
            free(adapters);
        }
    }
    if (found)
    {
        *address = lanAddress;
        *mask = lanMask;
    }
    return found == 1;
}
#endif

int sharedIoctl(int fd, unsigned long int request, ...)
{
    va_list args;
    va_start(args, request);
    void *argp = va_arg(args, void *);
    va_end(args);

#ifdef __linux__
    static int (*_ioctl)(int fd, int request, void *data) = NULL;
    if (_ioctl == NULL)
        _ioctl = REAL_FUNC(ioctl);
#else
    if (serialPortIs(fd))
        return serialPortIoctl(fd, request, argp);
#endif

    if (fd == hooks[EEPROM])
    {
        if (request == 0xC04064A0)
#ifdef __linux__
            return _ioctl(fd, request, argp);
#else
            return 0;
#endif
        return eepromIoctl(fd, request, argp);
    }

    if (fd == hooks[BASEBOARD])
    {
        return baseboardIoctl(fd, request, argp);
    }

    // The emulated drive board's replies: TCFLSH empties them (FIONREAD, below, counts them)
    if (fd == hooks[SERIAL0] && getConfig()->emulateDriveboard && request == 0x540B)
        return driveBoardioctl(fd, request, argp);

    if ((fd == hooks[SERIAL0] || fd == hooks[SERIAL1]) && request == 0x541b)
    {
        uint8_t d = 1;
        if ((gId == VIRTUA_FIGHTER_5_FINAL_SHOWDOWN_SBUV_REVA || gId == VIRTUA_FIGHTER_5_FINAL_SHOWDOWN_SBXX_REVB ||
             gId == VIRTUA_FIGHTER_5_FINAL_SHOWDOWN_SBXX_REVB_6000 || gId == VIRTUA_FIGHTER_5_SBQU_R ||
             gId == VIRTUA_FIGHTER_5_SBQU_R_REVD || gId == VIRTUA_FIGHTER_5_SBQU_R_REVG) ||
            ((gId == R_TUNED_SBQW || gId == MJ4_SBPN_REVG || gId == MJ4_EVO_SBTA) && fd == hooks[SERIAL1]))
        {
            memcpy(argp, &d, sizeof(uint8_t));
        }
        else if (getConfig()->emulateDriveboard)
        {
            return driveBoardioctl(fd, request, argp);
        }
        return 0;
    }
#ifdef __linux__
    if ((gGrp == GROUP_ID4_EXP || gId == INITIALD_5_EXP20_SBTS || gId == INITIALD_5_EXP20_SBTS_REVA || gGrp == GROUP_ID_SERVERBOX) &&
        (request == SIOCGIFFLAGS || request == SIOCGIFADDR || request == SIOCGIFNETMASK) && getConfig()->enableNetworkPatches &&
        strcmp(getConfig()->nicName, "") != 0)
    {
        struct ifreq *ifr = (struct ifreq *)argp;
        strncpy(ifr->ifr_name, getConfig()->nicName, IFNAMSIZ);
    }
    return _ioctl(fd, request, argp);
#else
    return windowsSocketIoctl(fd, request, argp);
#endif
}

#define LINUX_FD_SETSIZE 1024
#define LINUX_NFDBITS (8 * sizeof(unsigned long))
#define LINUX_FD_ISSET_BIT(n, p) (((const unsigned long *)(p))[(n) / LINUX_NFDBITS] & (1UL << ((n) % LINUX_NFDBITS)))
#define LINUX_FD_SET_BIT(n, p) (((unsigned long *)(p))[(n) / LINUX_NFDBITS] |= (1UL << ((n) % LINUX_NFDBITS)))
#define LINUX_FD_CLR_BIT(n, p) (((unsigned long *)(p))[(n) / LINUX_NFDBITS] &= ~(1UL << ((n) % LINUX_NFDBITS)))
#define LINUX_FD_ZERO_BIT(p) memset((void *)(p), 0, LINUX_FD_SETSIZE / 8)

static int linuxFdIsSet(int nfds, int fd, const fd_set *set)
{
    if (set == NULL || fd < 0 || fd >= nfds || fd >= LINUX_FD_SETSIZE)
        return 0;
#ifdef __linux__
    return FD_ISSET(fd, set) != 0;
#else
    return LINUX_FD_ISSET_BIT(fd, set) != 0;
#endif
}

#ifdef WIN32
// A select on the COM ports a board is passed to: those with bytes waiting to be read, those to write to (at once),
// waited for until the timeout; -1 when the sets hold none of them
static int serialPortSelect(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout)
{
    unsigned long wantRead[32] = {0}, wantWrite[32] = {0};
    bool any = false;
    for (int fd = 0; fd < nfds && fd < 1024; fd++)
    {
        if (!serialPortIs(fd))
            continue;
        if (readfds && LINUX_FD_ISSET_BIT(fd, readfds))
            LINUX_FD_SET_BIT(fd, wantRead), any = true;
        if (writefds && LINUX_FD_ISSET_BIT(fd, writefds))
            LINUX_FD_SET_BIT(fd, wantWrite), any = true;
    }
    if (!any)
        return -1;
    ULONGLONG deadline = timeout ? GetTickCount64() + (ULONGLONG)timeout->tv_sec * 1000 + timeout->tv_usec / 1000 : 0;
    for (;;)
    {
        int ready = 0;
        if (readfds)
            LINUX_FD_ZERO_BIT(readfds);
        if (writefds)
            LINUX_FD_ZERO_BIT(writefds);
        if (exceptfds)
            LINUX_FD_ZERO_BIT(exceptfds);
        for (int fd = 0; fd < nfds && fd < 1024; fd++)
        {
            if (LINUX_FD_ISSET_BIT(fd, wantRead) && serialPortPending(fd) > 0)
                LINUX_FD_SET_BIT(fd, readfds), ready++;
            if (LINUX_FD_ISSET_BIT(fd, wantWrite))
                LINUX_FD_SET_BIT(fd, writefds), ready++;
        }
        if (ready || (timeout && GetTickCount64() >= deadline))
            return ready;
        Sleep(1);
    }
}
#endif

#ifdef WIN32
// A pipe of the C library's (pipe()): select takes it with the sockets
static bool isPipeFd(int fd)
{
    intptr_t handle = osHandleOf(fd);
    return handle != -1 && GetFileType((HANDLE)handle) == FILE_TYPE_PIPE;
}

// A pipe readable (data waiting, or its other end closed) and writable (always, its buffer being large)
static bool pipeReady(int fd, int set)
{
    if (set != 0)
        return set == 1;
    DWORD waiting = 0;
    return !PeekNamedPipe((HANDLE)osHandleOf(fd), NULL, 0, NULL, &waiting, NULL) || waiting > 0;
}

// A select with pipes among the sockets: Maximum Tune 3's boost::asio (select_reactor) waits on its sockets and on a
// pipe written to wake it when it has something to send. Winsock's select knows no pipe: the sockets are polled with
// it and the pipes with PeekNamedPipe, every millisecond, until one is ready or the time is out (none: no end)
static int pipeSocketSelect(int *fds, int count, unsigned long **sets, struct timeval *timeout)
{
    ULONGLONG deadline = timeout ? GetTickCount64() + (ULONGLONG)timeout->tv_sec * 1000 + timeout->tv_usec / 1000 : 0;
    for (;;)
    {
        fd_set winSets[3];
        bool sockets = false;
        for (int i = 0; i < 3; i++)
        {
            FD_ZERO(&winSets[i]);
            for (int k = 0; k < count; k++)
                if (sets[i] && LINUX_FD_ISSET_BIT(fds[k], sets[i]) && !isPipeFd(fds[k]))
                {
                    FD_SET(sharedSocketOf(fds[k]), &winSets[i]);
                    sockets = true;
                }
        }
        bool pipeWaiting = false;
        for (int i = 0; i < 2; i++)
            for (int k = 0; k < count && !pipeWaiting; k++)
                pipeWaiting = sets[i] && LINUX_FD_ISSET_BIT(fds[k], sets[i]) && isPipeFd(fds[k]) && pipeReady(fds[k], i);
        struct timeval now = {0, pipeWaiting ? 0 : 1000};
        if (sockets)
        {
            if (select(0, &winSets[0], &winSets[1], &winSets[2], &now) == SOCKET_ERROR)
                return -1;
        }
        else if (!pipeWaiting)
            Sleep(1);
        int ready = 0;
        unsigned long chosen[3][1024 / (8 * sizeof(unsigned long))] = {{0}};
        for (int i = 0; i < 3; i++)
            for (int k = 0; k < count && sets[i]; k++)
            {
                if (!LINUX_FD_ISSET_BIT(fds[k], sets[i]))
                    continue;
                bool isReady = isPipeFd(fds[k]) ? pipeReady(fds[k], i) : FD_ISSET(sharedSocketOf(fds[k]), &winSets[i]) != 0;
                if (isReady)
                {
                    LINUX_FD_SET_BIT(fds[k], chosen[i]);
                    ready++;
                }
            }
        if (ready || (timeout && GetTickCount64() >= deadline))
        {
            for (int i = 0; i < 3; i++)
                for (int k = 0; k < count && sets[i]; k++)
                {
                    if (LINUX_FD_ISSET_BIT(fds[k], chosen[i]))
                        LINUX_FD_SET_BIT(fds[k], sets[i]);
                    else
                        LINUX_FD_CLR_BIT(fds[k], sets[i]);
                }
            return ready;
        }
    }
}

// A select on the game's sockets (Initial D's link waits for its datagrams so, cUDPSocket::receiveFrom): their bits in
// the game's sets as Winsock sockets, Winsock's select, the ready ones set back; pipes among them, pipeSocketSelect.
// -1 when the sets hold no socket nor pipe
#define SOCKET_SELECT_MAX 64
static int socketSelect(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout)
{
    unsigned long *sets[3] = {(unsigned long *)readfds, (unsigned long *)writefds, (unsigned long *)exceptfds};
    int fds[SOCKET_SELECT_MAX], count = 0;
    bool any = false, pipes = false;
    for (int fd = 0; fd < nfds && fd < 1024 && count < SOCKET_SELECT_MAX; fd++)
    {
        bool wanted = false;
        for (int i = 0; i < 3; i++)
            wanted = wanted || (sets[i] && LINUX_FD_ISSET_BIT(fd, sets[i]));
        if (!wanted)
            continue;
        if (isPipeFd(fd))
            pipes = true;
        else if (!isWinsockHandle(fd))
            return -1; // a file or a device among them: not this select's
        fds[count++] = fd;
        any = true;
    }
    if (!any)
        return -1;
    if (pipes)
        return pipeSocketSelect(fds, count, sets, timeout);
    fd_set winSets[3];
    for (int i = 0; i < 3; i++)
    {
        FD_ZERO(&winSets[i]);
        for (int k = 0; k < count; k++)
            if (sets[i] && LINUX_FD_ISSET_BIT(fds[k], sets[i]))
                FD_SET(sharedSocketOf(fds[k]), &winSets[i]);
    }
    int ready = select(0, readfds ? &winSets[0] : NULL, writefds ? &winSets[1] : NULL, exceptfds ? &winSets[2] : NULL,
                       (const struct timeval *)timeout);
    if (ready == SOCKET_ERROR)
        return -1;
    for (int i = 0; i < 3; i++)
        for (int k = 0; k < count && sets[i]; k++)
        {
            if (FD_ISSET(sharedSocketOf(fds[k]), &winSets[i]))
                LINUX_FD_SET_BIT(fds[k], sets[i]);
            else
                LINUX_FD_CLR_BIT(fds[k], sets[i]);
        }
    return ready;
}
#endif

static int (*_select)(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout) = NULL;
int sharedSelect(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout)
{
#ifdef __linux__
    if (_select == NULL)
        _select = REAL_FUNC(select);
#endif

    if (linuxFdIsSet(nfds, hooks[BASEBOARD], readfds) || linuxFdIsSet(nfds, hooks[BASEBOARD], writefds))
    {
        return baseboardSelect(nfds, readfds, writefds, exceptfds, timeout);
    }

#ifdef WIN32
    sharedSigioPoll();
    int serialReady = serialPortSelect(nfds, readfds, writefds, exceptfds, timeout);
    if (serialReady >= 0)
        return serialReady;
    int socketsReady = socketSelect(nfds, readfds, writefds, exceptfds, timeout);
    if (socketsReady >= 0)
        return socketsReady;
#endif

    if ((getConfig()->emulateHW210CardReader == 1 || getConfig()->emulateDriveboard == 1) &&
        (gGrp != GROUP_ID5 && gGrp != GROUP_ID4_EXP && gGrp != GROUP_ID4_JAP))
    {
        if (linuxFdIsSet(nfds, hooks[SERIAL0], readfds) || linuxFdIsSet(nfds, hooks[SERIAL0], writefds) ||
            linuxFdIsSet(nfds, hooks[SERIAL1], readfds) || linuxFdIsSet(nfds, hooks[SERIAL1], writefds))
            return 1;
    }

#ifdef __linux__
    return _select(nfds, readfds, writefds, exceptfds, timeout);
#endif
    return 0;
}

#ifdef __linux__
DIR *(*_opendir)(const char *) = NULL;
DIR *opendir(const char *dirname)
{
    if (_opendir == NULL)
        _opendir = REAL_FUNC(opendir);
    log_debug("Opendir %s\n", dirname);

    if (strcmp(dirname, "/tmp/") == 0 && gGrp == GROUP_ID5)
    {
        return _opendir(dirname + 1);
    }

    if (strcmp(getConfig()->idCardFolder, "") != 0 && strcmp(dirname, ".") == 0)
    {
        if (gGrp == GROUP_ID5 || gGrp == GROUP_ID4_EXP || gGrp == GROUP_ID4_JAP)
        {
            char *newDirName = getConfig()->idCardFolder;
            char lastChar = newDirName[strlen(newDirName) - 1];
            if (lastChar == PATH_SEPARATOR)
            {
                newDirName[strlen(newDirName) - 1] = '\0';
            }
            return _opendir(newDirName);
        }
    }

    if (strcmp(dirname, "/home/disk1/rankingdata") == 0 && (gGrp == GROUP_OUTRUN || gGrp == GROUP_OUTRUN_TEST))
        return _opendir("./rankingdata");

    return _opendir(dirname);
}

int remove(const char *path)
{
    return sharedRemove(path);
}

int mkdir(const char *path, mode_t mode)
{
    return sharedMkdir(path, mode);
}

static int (*___xstat64)(int ver, const char *path, struct stat64 *stat_buf) = NULL;

int __xstat64(int ver, const char *path, struct stat64 *stat_buf)
{
    if (___xstat64 == NULL)
        ___xstat64 = REAL_FUNC(__xstat64);

    if (strcmp("/var/tmp/warning", path) == 0)
    {
        return ___xstat64(ver, "warning", stat_buf);
    }
    return ___xstat64(ver, path, stat_buf);
}

int open(const char *pathname, int flags, ...)
{
    va_list args;
    va_start(args, flags);
    int mode = va_arg(args, int);
    va_end(args);
    return sharedOpen(pathname, flags, mode);
}

int open64(const char *pathname, int flags, ...)
{
    va_list args;
    va_start(args, flags);
    int mode = va_arg(args, int);
    va_end(args);
    return sharedOpen64(pathname, flags, mode);
}

FILE *fopen(const char *restrict pathname, const char *restrict mode)
{
    return sharedFopen(pathname, mode);
}

FILE *fopen64(const char *pathname, const char *mode)
{
    return sharedFopen64(pathname, mode);
}

int fclose(FILE *stream)
{
    return sharedFclose(stream);
}

static int (*_openat)(int dirfd, const char *pathname, int flags) = NULL;
int openat(int dirfd, const char *pathname, int flags)
{
    if (_openat == NULL)
        _openat = REAL_FUNC(openat);

    if (strcmp(pathname, "/dev/ttyS0") == 0 || strcmp(pathname, "/dev/ttyS1") == 0 || strcmp(pathname, "/dev/tts/0") == 0 ||
        strcmp(pathname, "/dev/tts/1") == 0)
    {
        return sharedOpen(pathname, flags);
    }

    return _openat(dirfd, pathname, flags);
}

int close(int fd)
{
    return sharedClose(fd);
}

char *fgets(char *str, int n, FILE *stream)
{
    return sharedFgets(str, n, stream);
}

ssize_t read(int fd, void *buf, size_t count)
{
    return sharedRead(fd, buf, count);
}

size_t fread(void *buf, size_t size, size_t count, FILE *stream)
{
    return sharedFread(buf, size, count, stream);
}

long int ftell(FILE *stream)
{
    return sharedFtell(stream);
}

int fseek(FILE *stream, long int offset, int whence)
{
    return sharedFseek(stream, offset, whence);
}

void rewind(FILE *stream)
{
    return sharedRewind(stream);
}

ssize_t write(int fd, const void *buf, size_t count)
{
    return sharedWrite(fd, buf, count);
}

int ioctl(int fd, unsigned long int request, ...)
{
    va_list args;
    va_start(args, request);
    void *argp = va_arg(args, void *);
    va_end(args);
    return sharedIoctl(fd, request, argp);
}

int select(int nfds, fd_set *restrict readfds, fd_set *restrict writefds, fd_set *restrict exceptfds, struct timeval *restrict timeout)
{
    return sharedSelect(nfds, readfds, writefds, exceptfds, timeout);
}
#endif
