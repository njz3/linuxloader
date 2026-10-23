// The card reader-printer of Namco's Maximum Tune 3 (clCardPrinter): a magnetic card reader and writer with a
// thermal printer and a dispenser of new cards, on the N2 system's serial port 2 (/dev/ttyM2, clSerialN2). The port
// is emulated where the game opens, writes and reads it, and its cards are files.
//
// The exchange: the game sends a command frame, the reader answers ACK, the game sends ENQ, the reader answers its
// response frame; a response whose P and S are '0' and '3' (busy) has the game send ENQ again 12 frames later.
//   command:  STX, length, command, 0, 0, 0, parameters, ETX, BCC
//   response: STX, length, command, R, P, S, data, ETX, BCC
// The length counts the bytes from the command to ETX, plus one; BCC is the XOR of the length and those bytes.
// R is where the card is ('0' none, '1' to '3' inside, '4' at the exit), P the result ('0' fine, '1' and 'Q' to 'V'
// read errors, '2' write error, '3' no card, '5' print error, '8' illegal, 'A' system, '@' battery), S the job ('0'
// done, '2' cannot be done, '3' busy, '5' the dispenser empty).
//
// The commands: 10 initialise, 20 status, 33 read (the card inside, or '2' first: the card the player inserts), 53
// write, 80 eject, B0 the dispenser ('1' a new card taken in, '2' checked), 40 cancel, A0 cleaning, F5 battery, and
// the printer's 7x. A card holds three tracks of 69 bytes; the game reads and writes the three at once (track 6).
// The read that waits for the player's card stays pending: until a card comes, it is answered "no card" without an
// error (R, P, S '0'), and the game polls it with ENQ alone, then cancels it (40) when the player plays without one;
// P '3' would be a jammed card (E53 once the game has tried three times).
//
// The cards: the files of the card emulators' "Card Data" folders (the game's own sample cards are such files): a
// [SETUP] section, then [DATA] with one line per track, its number (a byte 1 to 3) then its 69 bytes in hex. The card
// key (CardInsert, F7) offers the card of [Cards] CARDFILE_01 at the reader's slot, or takes it back; the game takes
// it in when it waits for a card, and ejects it at the end. A card from the dispenser is a new file: CARDFILE_01 when
// there is none, else the same name numbered. What the game writes is saved at once, through the write folder.

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "n2CardPrinter.h"
#include "../../config/config.h"
#include "../../graphics/overlayMsgs.h"
#include "../../log/log.h"
#include "../../patching/flowControl.h"
#include "../../redirections/writeDir.h"

#define STX 0x02
#define ETX 0x03
#define ENQ 0x05
#define ACK 0x06
#define NAK 0x15

#define TRACKS 3
#define TRACK_SIZE 69
#define CARD_PORT 2       // clSerialN2's port index: /dev/ttyM2
#define CARD_FD 0x4E32    // the emulated port's descriptor in clSerialN2
#define FRAME_MAX 600

// Maximum Tune 3's clSerialN2: open, close, send and receive
#define SERIAL_OPEN 0x0852D900
#define SERIAL_CLOSE 0x0852DAB0
#define SERIAL_SEND 0x0852DADC
#define SERIAL_RECEIVE 0x0852DB30

// clSerialN2's descriptor and port index
#define SERIAL_FD(serial) (*(int *)((uint8_t *)(serial) + 4))
#define SERIAL_PORT(serial) (*(int *)((uint8_t *)(serial) + 0xC))

typedef enum
{
    CARD_NONE,    // no card in the reader
    CARD_OFFERED, // the player's card at the slot, taken in when the game waits for one
    CARD_INSIDE,
} CardPlace;

static CardPlace place = CARD_NONE;
static uint8_t tracks[TRACKS][TRACK_SIZE];
static char cardPath[MAX_PATH_LENGTH]; // the card's file; empty: a new card, not saved yet

static uint8_t frame[FRAME_MAX]; // the command being received
static int frameLength;
static uint8_t command[FRAME_MAX]; // the last command acknowledged, answered at ENQ
static int commandPending; // answered at the next ENQ (the read waiting for a card at every ENQ)
static uint8_t response[FRAME_MAX]; // its response, sent again at another ENQ
static int responseLength;
static uint8_t lastLogged[4];
static uint8_t output[FRAME_MAX * 2]; // what the game reads next
static int outputLength;

// --- the card files

static const char *fileName(const char *path)
{
    const char *name = path;
    for (const char *c = path; *c; c++)
        if (*c == '/' || *c == '\\')
            name = c + 1;
    return name;
}

static bool fileExists(const char *path)
{
    FILE *file = writeDirFopen(path, "rb");
    if (file)
        fclose(file);
    return file != NULL;
}

static int hexDigit(char c)
{
    return c >= '0' && c <= '9' ? c - '0' : c >= 'A' && c <= 'F' ? c - 'A' + 10 : c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1;
}

// The card emulators' text, or the three tracks as they are (207 bytes)
static bool cardLoad(const char *path)
{
    FILE *file = writeDirFopen(path, "rb");
    if (!file)
        return false;
    char text[4096];
    size_t size = fread(text, 1, sizeof(text) - 1, file);
    fclose(file);
    text[size] = '\0';

    memset(tracks, 0, sizeof(tracks));
    if (size == sizeof(tracks))
    {
        memcpy(tracks, text, sizeof(tracks));
        return true;
    }
    int found = 0;
    for (char *line = text; line < text + size; line++)
    {
        int track = line == text || line[-1] == '\n' ? line[0] : 0;
        if (track < 1 || track > TRACKS)
            continue;
        const char *hex = line + 1;
        for (int i = 0; i < TRACK_SIZE && hexDigit(hex[0]) >= 0 && hexDigit(hex[1]) >= 0; i++, hex += 2)
            tracks[track - 1][i] = (uint8_t)(hexDigit(hex[0]) << 4 | hexDigit(hex[1]));
        found |= 1 << (track - 1);
    }
    return found != 0;
}

// A new card's file: CARDFILE_01, else the first of its name numbered that is free
static void newCardPath(void)
{
    const char *base = getConfig()->cardFile1;
    if (!fileExists(base))
    {
        snprintf(cardPath, sizeof(cardPath), "%s", base);
        return;
    }
    const char *dot = strrchr(fileName(base), '.');
    int stem = dot ? (int)(dot - base) : (int)strlen(base);
    for (int n = 2; n < 1000; n++)
    {
        snprintf(cardPath, sizeof(cardPath), "%.*s_%d%s", stem, base, n, dot ? dot : "");
        if (!fileExists(cardPath))
            return;
    }
}

static void cardSave(void)
{
    bool fresh = cardPath[0] == '\0';
    if (fresh)
        newCardPath();
    FILE *file = writeDirFopen(cardPath, "wb");
    if (!file)
    {
        log_error("Card reader-printer: cannot write the card %s", cardPath);
        return;
    }
    fputs("[SETUP]\r\nCARDTYPE =4\r\nPARITY1 =0\r\nPARITY2 =0\r\nPARITY3 =0\r\nBPC1 =8\r\nBPC2 =8\r\nBPC3 =8\r\n"
          "SS1 =5\r\nSS2 =11\r\nSS3 =11\r\nES1 =31\r\nES2 =15\r\nES3 =15\r\n\r\n[DATA]\r\n",
          file);
    for (int track = 0; track < TRACKS; track++)
    {
        fputc(track + 1, file);
        for (int i = 0; i < TRACK_SIZE; i++)
            fprintf(file, "%02X", tracks[track][i]);
        fputs("\r\n", file);
    }
    fputs("\r\n", file);
    fclose(file);
    log_info("Card reader-printer: card saved in %s", cardPath);
    if (fresh)
    {
        char message[96];
        snprintf(message, sizeof(message), "New card: %.64s", fileName(cardPath));
        overlayShowMessage(message, 4000, OVERLAY_BOTTOM_LEFT);
    }
}

// The card key: the card offered at the slot, or taken back from it
static void keyPressed(void)
{
    const char *path = getConfig()->cardFile1;
    char message[96];
    if (place == CARD_INSIDE)
        snprintf(message, sizeof(message), "A card is in the reader");
    else if (place == CARD_OFFERED)
    {
        place = CARD_NONE;
        snprintf(message, sizeof(message), "Card taken back");
    }
    else if (!cardLoad(path))
    {
        snprintf(message, sizeof(message), "No card %.64s", fileName(path));
        log_warn("Card reader-printer: no card in %s ([Cards] CARDFILE_01); the game gives a new one", path);
    }
    else
    {
        snprintf(cardPath, sizeof(cardPath), "%s", path);
        place = CARD_OFFERED;
        snprintf(message, sizeof(message), "Card inserted: %.64s", fileName(path));
    }
    log_info("Card reader-printer: %s", message);
    overlayShowMessage(message, 3000, OVERLAY_BOTTOM_LEFT);
}

// --- the reader

static void queue(const uint8_t *bytes, int length)
{
    if (outputLength + length > (int)sizeof(output))
        outputLength = 0; // the game stopped reading: what it left is dropped
    memcpy(output + outputLength, bytes, length);
    outputLength += length;
}

static char cardPosition(void)
{
    return place == CARD_INSIDE ? '1' : '0';
}

static void answer(char p, char s, const uint8_t *data, int size)
{
    response[0] = STX;
    response[1] = (uint8_t)(size + 6);
    response[2] = command[2];
    response[3] = (uint8_t)cardPosition();
    response[4] = (uint8_t)p;
    response[5] = (uint8_t)s;
    if (size)
        memcpy(response + 6, data, size);
    response[6 + size] = ETX;
    uint8_t bcc = response[1];
    for (int i = 2; i <= size + 6; i++)
        bcc ^= response[i];
    response[7 + size] = bcc;
    responseLength = size + 8;
}

// The tracks a command names: 0 to 2 one, 3 to 5 two, 6 the three
static int trackList(char id, int *list)
{
    static const int pairs[3][2] = {{0, 1}, {0, 2}, {1, 2}};
    int track = id - '0';
    if (track >= 0 && track <= 2)
    {
        list[0] = track;
        return 1;
    }
    if (track >= 3 && track <= 5)
    {
        list[0] = pairs[track - 3][0];
        list[1] = pairs[track - 3][1];
        return 2;
    }
    for (int i = 0; i < TRACKS; i++)
        list[i] = i;
    return TRACKS;
}

static void commandRead(void)
{
    bool waitInsert = command[6] == '2';
    if (waitInsert && place == CARD_OFFERED)
    {
        place = CARD_INSIDE;
        log_info("Card reader-printer: card %s taken in", cardPath);
    }
    if (place != CARD_INSIDE)
    {
        // Waiting for the player's card: no card yet, the read still pending; the card inside: none to read
        answer(waitInsert ? '0' : '3', '0', NULL, 0);
        commandPending = waitInsert;
        return;
    }
    uint8_t data[TRACKS * TRACK_SIZE];
    int list[TRACKS], count = trackList((char)command[8], list);
    for (int i = 0; i < count; i++)
        memcpy(data + i * TRACK_SIZE, tracks[list[i]], TRACK_SIZE);
    answer('0', '0', data, count * TRACK_SIZE);
}

// Write: '0', a mode, the tracks, their data
static void commandWrite(void)
{
    if (place != CARD_INSIDE)
    {
        answer('3', '0', NULL, 0);
        return;
    }
    int list[TRACKS], count = trackList((char)command[8], list);
    int size = command[1] - 9;
    for (int i = 0; i < count && (i + 1) * TRACK_SIZE <= size; i++)
        memcpy(tracks[list[i]], command + 9 + i * TRACK_SIZE, TRACK_SIZE);
    cardSave();
    answer('0', '0', NULL, 0);
}

static void commandEject(void)
{
    if (place == CARD_INSIDE)
    {
        place = CARD_NONE; // out, and taken by the player
        log_info("Card reader-printer: card %s ejected", cardPath);
        char message[96];
        snprintf(message, sizeof(message), "Card ejected: %.64s", fileName(cardPath));
        overlayShowMessage(message, 3000, OVERLAY_BOTTOM_LEFT);
    }
    answer('0', '0', NULL, 0);
}

// The dispenser: '1' a new card taken in (then '2', the dispenser checked: S '5' when empty)
static void commandDispenser(void)
{
    if (command[6] == '1')
    {
        if (place == CARD_INSIDE)
        {
            answer('0', '2', NULL, 0);
            return;
        }
        place = CARD_INSIDE;
        memset(tracks, 0, sizeof(tracks));
        cardPath[0] = '\0';
        log_info("Card reader-printer: a new card from the dispenser");
    }
    answer('0', '0', NULL, 0);
}

static void execute(void)
{
    commandPending = 0;
    switch (command[2])
    {
        case 0x33:
            commandRead();
            break;
        case 0x53:
            commandWrite();
            break;
        case 0x80:
            commandEject();
            break;
        case 0xB0:
            commandDispenser();
            break;
        default: // initialise, status, cancel, cleaning, battery, the printer's
            answer('0', '0', NULL, 0);
            break;
    }
    // Logged when the command or its answer changes (not each status poll, nor each poll of the read waiting for a card)
    uint8_t logged[4] = {command[2], response[3], response[4], response[5]};
    if (memcmp(logged, lastLogged, sizeof(logged)) != 0)
        log_info("Card reader-printer: command %02X %.*s answered R=%c P=%c S=%c", command[2],
                 command[1] - 6 < 3 ? command[1] - 6 : 3, command + 6, response[3], response[4], response[5]);
    memcpy(lastLogged, logged, sizeof(logged));
}

static void frameReceived(void)
{
    int length = frame[1];
    uint8_t bcc = frame[1];
    for (int i = 2; i <= length; i++)
        bcc ^= frame[i];
    if (bcc != frame[length + 1] || frame[length] != ETX)
    {
        static const uint8_t nak = NAK;
        log_warn("Card reader-printer: command %02X with a bad BCC", frame[2]);
        queue(&nak, 1);
        return;
    }
    static const uint8_t ack = ACK;
    memcpy(command, frame, length + 2);
    commandPending = 1;
    queue(&ack, 1);
}

// --- clSerialN2, replaced: the card port emulated, the other ports left closed (the Kickback board's: deadHeatIo.c)

static int serialOpen(void *serial)
{
    if (SERIAL_PORT(serial) != CARD_PORT)
        return 0;
    SERIAL_FD(serial) = CARD_FD;
    frameLength = outputLength = 0;
    commandPending = 0;
    log_info("Card reader-printer: port opened by the game");
    return 1;
}

static int serialClose(void *serial)
{
    SERIAL_FD(serial) = -1;
    return 1;
}

static int serialSend(void *serial, const uint8_t *bytes, int length)
{
    if (SERIAL_FD(serial) != CARD_FD)
        return -1;
    for (int i = 0; i < length; i++)
    {
        uint8_t byte = bytes[i];
        if (frameLength == 0)
        {
            if (byte == ENQ)
            {
                if (commandPending)
                    execute();
                if (responseLength)
                    queue(response, responseLength);
            }
            else if (byte == STX)
                frame[frameLength++] = byte;
            continue;
        }
        frame[frameLength++] = byte;
        if (frameLength >= 2 && (frame[1] < 6 || frameLength == FRAME_MAX))
            frameLength = 0; // not a frame
        else if (frameLength >= 2 && frameLength == frame[1] + 2)
        {
            frameReceived();
            frameLength = 0;
        }
    }
    return length;
}

static int serialReceive(void *serial, uint8_t *bytes, int length)
{
    if (SERIAL_FD(serial) != CARD_FD)
        return -1;
    int count = outputLength < length ? outputLength : length;
    if (count <= 0)
        return 0;
    memcpy(bytes, output, count);
    memmove(output, output + count, outputLength - count);
    outputLength -= count;
    return count;
}

// From the inputs' processing, on the game's thread as clCardPrinter::exec
void n2CardPrinterKey(void)
{
    keyPressed();
}

void n2CardPrinterInstall(void)
{
    detourFunction(SERIAL_OPEN, serialOpen);
    detourFunction(SERIAL_CLOSE, serialClose);
    detourFunction(SERIAL_SEND, serialSend);
    detourFunction(SERIAL_RECEIVE, serialReceive);
    log_info("Card reader-printer emulated, its card in %s ([Cards] CARDFILE_01, CardInsert key)",
             getConfig()->cardFile1);
}
