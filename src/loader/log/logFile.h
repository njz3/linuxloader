#pragma once

// LOG_FILE ([System], true by default): the lines of a game's run, the console's, also in a text file. The process the
// front end or the picker starts (mainW.cpp's segabootSupervise) gets the output of the game's processes (the game,
// its test menu, the game again) through a pipe, and copies it to its console and to linuxloader.log: what a game that
// crashes wrote last is kept. The file is next to linuxloader.exe, or at the root of the write folder when there is
// one (WRITE_DIR: the loader's folder is then never written), in Windows' temporary folder when it cannot be made
// there. The previous run's becomes linuxloader.old.log. Windows only: elsewhere these do nothing.

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

    // The file a run writes: at writeRoot, the write folder's root, else next to the program ("" or NULL for none)
    void logFilePath(const char *writeRoot, char *path, size_t size);
    // The settings file LOG_FILE is read from: the write folder's linuxloader.ini when there is one, else settingsPath
    // (the loader's linuxloader.ini, or the one given)
    void logFileSettings(const char *settingsPath, const char *writeRoot, char *path, size_t size);
    // Whether a run started now writes it: LOG_FILE of that file, the command line's --set over it
    bool logFileWanted(const char *settingsPath, const char *writeRoot);
    // This process's standard output and error, and the game's processes' it starts, go through the pipe: copied to the
    // console (or wherever they went) and to the file, the previous one kept as .old.log. False when the file cannot be
    // made anywhere: nothing changed
    bool logFileStart(const char *writeRoot);
    // The pipe closed and emptied into the file, before this process ends
    void logFileStop(void);

#ifdef __cplusplus
}
#endif
