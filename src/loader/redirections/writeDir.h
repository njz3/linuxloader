#pragma once

// The write folder (--write-dir PATH, or [Paths] WRITE_DIR): the loader's folder and the game's are only read, and
// everything written goes there. Its root holds the user's settings (linuxloader.ini, controls.ini, copied there the
// first time); WRITE_DIR\<game> holds what the game and the loader write while it runs: NVRAM (eeprom.bin, sram.bin,
// cards), saves, logs, temporary files, compiled shaders. A file read is taken from WRITE_DIR\<game> when it is there,
// else from the game's folder; a file opened to be modified is copied there first. With a write filter on the disk
// (UWF), WRITE_DIR is the one folder left writable, and the games' folders can be on a read-only network share.
// Windows only: elsewhere the paths go through unchanged.

#include <stddef.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C"
{
#endif

    // Before the settings are read. writeDir: the command line's, else [Paths] WRITE_DIR of *configPath. When there is
    // one, *configPath and *controlsPath become WRITE_DIR's linuxloader.ini and controls.ini.
    void writeDirSettings(const char *writeDir, char **configPath, char **controlsPath);
    // Once the game is known, before it runs: WRITE_DIR\<game> made and made the current folder.
    void writeDirEnterGame(const char *gameName);
    int writeDirActive(void);

    // The path to read: WRITE_DIR\<game>'s file when it is there, else the game folder's. Paths other than the game's
    // (devices, other folders) come back as they are.
    const char *writeDirForRead(const char *path, char *buffer, size_t size);
    // The path to write, in WRITE_DIR\<game>, its folders made; keep: its content kept (a, r+, no O_TRUNC), the game
    // folder's file copied first.
    const char *writeDirForWrite(const char *path, int keep, char *buffer, size_t size);
    // A folder's two sides: WRITE_DIR\<game>'s and the game folder's. 0 for a path outside the game's.
    int writeDirSides(const char *path, char *writeSide, char *gameSide, size_t size);

    // The CRT's, through the write folder
    FILE *writeDirFopen(const char *path, const char *mode);
    int writeDirOpen(const char *path, int flags, int mode); // _O_* flags
    int writeDirMkdir(const char *path);
    int writeDirRemove(const char *path);
    int writeDirRename(const char *from, const char *to);

#ifdef __cplusplus
}
#endif
