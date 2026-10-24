#pragma once

// The options of one game: the Tab window's "Game options" page, in the settings file's [Game.<short name>] section,
// read over the general keys and for that game only (gameOptions.c, the same layout as windowsloader's).

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    GAME_OPTION_SWITCH, // a check box: true / false
    GAME_OPTION_CHOICE, // a list: one of values, written as it is
    GAME_OPTION_RANGE   // a slider: an integer from min to max
} GameOptionKind;

typedef struct
{
    const char *key;   // in [Game.<short name>], in capitals
    const char *label; // English, translated by the interface
    const char *help;
    GameOptionKind kind;
    size_t field; // offsetof(EmulatorConfig, ...): an int, or a float when isFloat
    bool isFloat;
    const char *const *values;      // GAME_OPTION_CHOICE: the values written, NULL-terminated; the field holds the index
    const char *const *valueLabels; // and what the list shows
    int min, max;                   // GAME_OPTION_RANGE
    bool nextStart;                 // takes effect at the next start (else at once)
    bool (*applies)(void);          // an option of the game being run
} GameOption;

extern const GameOption gameOptionTable[];
extern const int gameOptionCount;

// [Game.<short name>] of the game being run, or "" for no game
const char *gameOptionSection(void);
int gameOptionValue(const GameOption *option);
void gameOptionSetValue(const GameOption *option, int value);
// The value as the settings file writes it; and read from it (false when the text is not one)
void gameOptionFormat(const GameOption *option, char *buffer, size_t size);
bool gameOptionParse(const GameOption *option, const char *text);
// Once the settings are read: the values the game started with, against which the page marks the changes that wait
// for the next start
void gameOptionsRemember(void);
int gameOptionLaunchValue(const GameOption *option);

// A board of the cabinet on one of the game's serial ports, emulated by the loader or passed to a real board on a
// serial port of the PC (the page's "Boards" part, as windowsloader's): its EMULATE_ key and its port, the keys of
// [Emulation] and [Paths] that [Game.<short name>] also holds for this game
typedef struct
{
    const char *label;      // English, translated by the interface
    const char *gamePort;   // the game's side: "/dev/ttyS0", "base board" (translated)
    const char *emulateKey; // EMULATE_JVS, EMULATE_DRIVEBOARD
    const char *pathKey;    // JVS_PATH, SERIAL_1_PATH
    size_t emulateField;    // offsetof(EmulatorConfig, ...): an int
    size_t pathField;       // a char[MAX_PATH_LENGTH]
    bool (*applies)(void);
} GameBoard;

extern const GameBoard gameBoardTable[];
extern const int gameBoardCount;

bool gameBoardEmulated(const GameBoard *board);
const char *gameBoardPath(const GameBoard *board);
// Emulated (port NULL), or the real board on a port of the PC ("COM10")
void gameBoardSet(const GameBoard *board, const char *port);
// [Game.<short name>]'s values, NULL for a key it does not have
void gameBoardLoad(const GameBoard *board, const char *emulate, const char *path);
// Not as the game started
bool gameBoardChanged(const GameBoard *board);

#ifdef __cplusplus
}
#endif
