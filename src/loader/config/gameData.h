#ifndef GAME_DATA_H
#define GAME_DATA_H

#include "config.h"

typedef struct GameData {
    uint32_t crc32;
    const char *gameTitle;
    const char *gameShortTitle;
    const char *gameDVP;
    const char *gameID;
    const char *gameReleaseYear;
    const char *gameNativeResolutions;
    GameStatus gameStatus;
    JVSIOType jvsIOType;
    GameType gameType;
    int width;
    int height;
    GameGroup gameGroup;
    int emulateRideboard;
    int emulateDriveboard;
    int emulateMotionboard;
    int emulateHW210CardReader;
    int emulateIDCardReader;
    int emulateTouchscreen;
    Colour gameLindberghColour;
} GameData;

const GameData *getGameData(uint32_t elf_crc);
// The table, entry by entry (--list-games)
size_t gameDataCount(void);
const GameData *gameDataAt(size_t index);
// What the loader does for a game (--list-games, the picker): its servo board, force feedback, link between cabinets
int gameDataHasServoBoard(const GameData *game);
int gameDataHasFfb(const GameData *game);
int gameDataHasLink(const GameData *game);

#endif // GAME_DATA_H
