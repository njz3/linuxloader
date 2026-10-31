#pragma once

// The write folder (--write-dir PATH, or [Paths] WRITE_DIR): the loader's folder and the game's are only read, and
// everything written goes there. Its root holds the user's settings (linuxloader.ini, controls.ini, copied there the
// first time); WRITE_DIR\<game> holds what the game and the loader write while it runs: NVRAM (eeprom.bin, sram.bin,
// cards), saves, logs, temporary files, compiled shaders. A file read is taken from WRITE_DIR\<game> when it is there,
// else from the game's folder; a file opened to be modified is copied there first. With a write filter on the disk
// (UWF), WRITE_DIR is the one folder left writable, and the games' folders can be on a read-only network share.
// The seed folder (--seed-dir PATH, or [Paths] SEED_DIR), with a write folder only: <seed>\<game> holds the files
// WRITE_DIR\<game> starts with in place of the game folder's (a link cabinet's NVRAM, its LINK ID set in the test
// menu), copied there when the game starts, a file already in WRITE_DIR\<game> never replaced.
// A write folder that cannot be made or written in (checked at start with a file written and removed) is refused:
// nothing is kept, and nothing is ever written in the loader's or the game's folder instead (the cabinet's templates).
// The files are read where they are; one written goes to a copy in %TEMP%\linuxloader-nosave-<pid>, read back during
// the run and removed after it. A notice says so over the game for 20 s (graphics/overlayMsgs.c).
// Windows only: elsewhere the paths go through unchanged.

#include <stddef.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C"
{
#endif

    // Before the settings are read. writeDir, seedDir: the command line's, else [Paths] WRITE_DIR and SEED_DIR of
    // *configPath, relative to the loader's folder. When there is a write folder, *configPath and *controlsPath become
    // WRITE_DIR's linuxloader.ini and controls.ini.
    void writeDirSettings(const char *writeDir, const char *seedDir, char **configPath, char **controlsPath);
    // Once the game is known, before it runs: WRITE_DIR\<game> made and made the current folder, <seed>\<game>'s files
    // copied there where it has none of theirs.
    void writeDirEnterGame(const char *gameName);
    int writeDirActive(void);
    // WRITE_DIR asked for but unusable (it cannot be made, or a file cannot be written in it): its path, else NULL.
    // Nothing is then written anywhere: the settings and the game's files are read where they are, every write is
    // lost (never made in the loader's or the game's folder)
    const char *writeDirRefused(void);
    // The game's folder, the current one before writeDirEnterGame: 0 when the write folder is not in use
    int writeDirGameFolder(char *buffer, size_t size);
    // The root of the Linux disk image the game's folder is in (Gaelco's games): its files outside the game's folder
    // are written in WRITE_DIR\<game>\linuxroot
    void writeDirImageRoot(const char *root);

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
    int writeDirChdir(const char *path);
    int writeDirRemove(const char *path);
    int writeDirRename(const char *from, const char *to);

#ifdef __cplusplus
}
#endif
