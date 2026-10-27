
#include <SDL3/SDL.h>
#include <stdint.h>
#include <string.h>

#include "../../config/config.h"
#include "../../redirections/filesystemShared.h"
#include "driveBoard.h"
#include "forceFeedback.h"
#include "jvs.h"
#include "netOutputs.h"
#include "../../agent/agent.h"

// The Sega servo board's replies, as DRIVEBOARD.md describes them ("Sega servoboard", checked on the hardware): one
// reply to every command. In the short reply mode, the one a reset sets, a status byte below 0x80. In a long mode,
// chosen by FA 00 address, a 4-byte frame: 0x80 + the address once the encoder index is found, 0xA0 + the address
// before, two 7-bit values, then the checksum (b0 ^ b1 ^ b2) & 0x7F. The mode holds for every reply after the FA, its
// own included.
#define DRIVEBOARD_READY 0x00
#define DRIVEBOARD_NOT_INIT 0x11
#define DRIVEBOARD_BUSY 0x44

#define REPLY_SHORT 0x1F   // FA 00 1F: one-byte statuses
#define REPLY_STATE 0x10   // FA 00 10: the board's state, 00 01 reset, 00 04 searching the index, 00 00 ready (Initial D)
#define REPLY_ENCODER 0x14 // FA 00 14: the encoder position, 0x2000 at the centre (Harley-Davidson, Race TV)
#define ENCODER_CENTRE 0x2000
#define INDEX_SEARCH_MS 4050 // FC to the index found: 4.03 to 4.06 s in the captures of a real cabinet

#define LINUX_TCFLSH 0x540B
#define LINUX_FIONREAD 0x541B

extern uint32_t gId;
extern int gGrp;
extern DeviceType hooks[5];

// What FA 00 address reads, read on a cold board: 00 to 0F hold the last 80 to 8F, 1D the last 9D; 10 (the state)
// and 14 (the encoder) are worked out when read
static const uint8_t coldRegisters[0x20][2] = {
    {0x01, 0x00}, {0x40, 0x7F}, {0x04, 0x54}, {0x40, 0x04}, {0x01, 0x00}, {0x00, 0x00}, {0x01, 0x00}, {0x01, 0x00},
    {0x00, 0x03}, {0x00, 0x00}, {0x20, 0x40}, {0x00, 0x00}, {0x00, 0x00}, {0x01, 0x01}, {0x00, 0x01}, {0x00, 0x01},
    {0x00, 0x01}, {0x00, 0x01}, {0x01, 0x00}, {0x01, 0x00}, {0x40, 0x00}, {0x01, 0x00}, {0x01, 0x00}, {0x01, 0x00},
    {0x01, 0x00}, {0x01, 0x00}, {0x01, 0x00}, {0x01, 0x00}, {0x01, 0x00}, {0x00, 0x00}, {0x00, 0x20}, {0x01, 0x00}};
static uint8_t registers[0x20][2];
static int replyMode;
static bool indexFound;
static uint64_t searchEndMs; // the index search running until then, 0 when none

// The replies the game has not read yet, a frame each, the first one read up to replyOffset
#define REPLIES_KEPT 16
static uint8_t replies[REPLIES_KEPT][4];
static int replyLengths[REPLIES_KEPT];
static int replyFrames;
static int replyOffset;
static uint8_t lastReply[4];
static int lastReplyLength;

int wheelInitialized;

uint8_t buffer[64];
uint8_t bufferIdx = 0;

// The board as it powers up or after FF 00 00: short statuses, its registers back to their defaults, the index lost
static void boardReset(void)
{
    memcpy(registers, coldRegisters, sizeof(registers));
    replyMode = REPLY_SHORT;
    indexFound = false;
    searchEndMs = 0;
}

static int repliesWaiting(void)
{
    int bytes = -replyOffset;
    for (int i = 0; i < replyFrames; i++)
        bytes += replyLengths[i];
    return bytes;
}

static void dropReply(int index)
{
    memmove(replies[index], replies[index + 1], (size_t)(replyFrames - index - 1) * sizeof(replies[0]));
    memmove(&replyLengths[index], &replyLengths[index + 1], (size_t)(replyFrames - index - 1) * sizeof(replyLengths[0]));
    replyFrames--;
    if (index == 0)
        replyOffset = 0;
}

// Replies left unread: the oldest goes first
static void pushReply(const uint8_t *reply, int length)
{
    if (replyFrames == REPLIES_KEPT)
        dropReply(0);
    memcpy(replies[replyFrames], reply, length);
    replyLengths[replyFrames++] = length;
}

int initDriveboard()
{
    boardReset();
    replyFrames = 0;
    replyOffset = 0;
    // A read before any command gets the status of a board not initialised, as it always did
    lastReply[0] = DRIVEBOARD_NOT_INIT;
    lastReplyLength = 1;
    wheelInitialized = 0;
    return 0;
}

static void queueReply(void)
{
    if (searchEndMs && SDL_GetTicks() >= searchEndMs)
    {
        searchEndMs = 0;
        indexFound = true;
    }

    uint8_t reply[4];
    int length = 1;
    if (replyMode == REPLY_SHORT)
    {
        reply[0] = searchEndMs ? DRIVEBOARD_BUSY : indexFound ? DRIVEBOARD_READY : DRIVEBOARD_NOT_INIT;
    }
    else
    {
        reply[0] = (uint8_t)((indexFound ? 0x80 : 0xA0) | replyMode);
        if (replyMode == REPLY_STATE)
        {
            reply[1] = 0x00;
            reply[2] = searchEndMs ? 0x04 : indexFound ? 0x00 : 0x01;
        }
        else if (replyMode == REPLY_ENCODER)
        {
            reply[1] = ENCODER_CENTRE >> 7;
            reply[2] = ENCODER_CENTRE & 0x7F;
        }
        else
        {
            reply[1] = registers[replyMode][0];
            reply[2] = registers[replyMode][1];
        }
        reply[3] = (reply[0] ^ reply[1] ^ reply[2]) & 0x7F;
        length = 4;
    }

    pushReply(reply, length);
    memcpy(lastReply, reply, length);
    lastReplyLength = length;
}

// A command of player 1's board: its effect on the board's state, then its reply
static void boardCommand(const uint8_t *buf)
{
    switch (buf[0])
    {
        case 0xFF:
        {
            printf("Driveboard: Drive board reset\n");
            queueReply(); // the reply is the board's before the reset
            boardReset();
            return;
        }

        case 0xFA: // the reply mode, this command's reply included
        {
            if (buf[2] < 0x20)
            {
                if (replyMode == REPLY_SHORT && buf[2] != REPLY_SHORT)
                {
                    // Short statuses left unread would put every 4-byte reply out of step for a game that reads them
                    // 4 at a time: dropped, as windowsloader does
                    for (int i = replyFrames - 1; i >= 0; i--)
                        if (replyLengths[i] == 1)
                            dropReply(i);
                }
                replyMode = buf[2];
            }
        }
        break;

        case 0xFC:
        {
            printf("Driveboard: Start wheel bounds testing\n");
            if (!searchEndMs)
            {
                indexFound = false;
                searchEndMs = SDL_GetTicks() + INDEX_SEARCH_MS;
            }
        }
        break;

        case 0x9D:
        {
            registers[0x1D][0] = buf[1];
            registers[0x1D][1] = buf[2];
        }
        break;

        default:
        {
            // 80 to 8F: their parameters, which FA 00 00 to 0F read back; a damper's first parameter of 0 is kept as 1
            if (buf[0] >= 0x80 && buf[0] <= 0x8F)
            {
                registers[buf[0] & 0x0F][0] = (buf[0] == 0x86 && buf[1] == 0) ? 0x01 : buf[1];
                registers[buf[0] & 0x0F][1] = buf[2];
            }
        }
        break;
    }
    queueReply();
}

ssize_t driveboardRead(int fd, void *buf, size_t count)
{
    memset(buffer, '\0', 64);
    if (count == 0)
        return 0;
    // select() reports the port readable at all times (sharedSelect), so a game reads it with no new reply too: it
    // gets the last one again, whole. The byte was left unwritten before, and OutRun read what its stack held: 0x11,
    // 0x22 or 0x33 were taken for a board's encoder, overcurrent or overheat alarm ("DRIVE BOARD FAULT DETECTED").
    if (replyFrames == 0)
        pushReply(lastReply, lastReplyLength);
    size_t length = 0;
    while (length < count && replyFrames > 0)
    {
        size_t part = (size_t)(replyLengths[0] - replyOffset);
        if (part > count - length)
            part = count - length;
        memcpy((uint8_t *)buf + length, replies[0] + replyOffset, part);
        length += part;
        replyOffset += (int)part;
        if (replyOffset == replyLengths[0])
            dropReply(0);
    }
    return (ssize_t)length;
}

ssize_t driveboardWrite(int fd, const void *buf, size_t count)
{
    const uint8_t *bytes = (const uint8_t *)buf;
    netOutputsDrive(bytes, count);
    agentDriveboard(bytes, count);
    for (size_t i = 0; i < count; ++i)
    {
        buffer[bufferIdx++] = bytes[i];
        if (bufferIdx == 4)
        {
            // if (buffer[0] != 0xfd)// && buffer[0] != 0x00 && buffer[0] != 0xff && buffer[0] != 0x01 && buffer[0] != 0xfa)
            // {
            //     printf("Write: fd: %d,count: %zu, bufferIdx: %d\n", fd, count, bufferIdx);
            //     for (int x = 0; x < 4; x++)
            //     {
            //         printf("0x%02x ", buffer[x]);
            //     }
            //     printf("\n");
            // }
            processDrivePacket(buffer, 1);
            // The replies are read on the first port only
            if (fd == (int)hooks[SERIAL0])
                boardCommand(buffer);
            sdlFfbDriveboard(buffer, 4);
            if (count != 7)
                bufferIdx = 0;
        }
        else if (bufferIdx == 7)
        {
            uint8_t player2Buffer[4];
            player2Buffer[0] = buffer[0];
            player2Buffer[1] = buffer[4];
            player2Buffer[2] = buffer[5];
            player2Buffer[3] = buffer[6];
            processDrivePacket(player2Buffer, 2);
            // if (player2Buffer[0] != 0xfd && player2Buffer[0] != 0x00 && player2Buffer[0] != 0xff && player2Buffer[0] != 0x01)
            // {
                // printf("Write P2: fd: %d,count: %zu, bufferIdx: %d\n", fd, count, bufferIdx);
                // for (int x = 0; x < 4; x++)
                // {
                //     printf("0x%02x ", player2Buffer[x]);
                // }
                // printf("\n");
            // }
            bufferIdx = 0;
        }
    }
    return count;
}

// The wheel of a game that centres it with torques before its 80: the torque moves the steering, as the wheel would
void processDrivePacket(uint8_t *buf, int player)
{
    switch (buf[0])
    {
        case 0x80:
        {
            if ((buf[1] == 0x00 && buf[2] == 0x00 && buf[3] == 0x00) || (buf[1] == 0x01 && buf[2] == 0x00 && buf[3] == 0x01) ||
                (buf[1] == 0x01 && buf[2] == 0x00 && buf[3] == 0x00))
                wheelInitialized = 1;
        }
        break;

        case 0x84:
        {
            if ((buf[1] == 0x01 && buf[2] == 0x00 && buf[3] == 0x05) && !wheelInitialized)
            {
                if (player == 1)
                    setAnalogue(ANALOGUE_1, (int)(0.5f * 1024.0f));
                else
                    setAnalogue(ANALOGUE_5, (int)(0.5f * 1024.0f));
                break;
            }

            uint8_t cmd1 = buf[1];
            if (gGrp == GROUP_OUTRUN_TEST)
            {
                if (buf[4] != 0)
                    cmd1 = buf[4];
            }

            float scaled = 0.0f;
            if (cmd1 == 1)
                scaled = (128.0f - (buf[2] - 1)) / 256.0f;

            if (cmd1 == 0)
                scaled = (256.0f - (buf[2] + 1)) / 256.0f;

            if (!wheelInitialized)
            {
                if (player == 1)
                    setAnalogue(ANALOGUE_1, (int)(scaled * 1024.0f));
                else
                    setAnalogue(ANALOGUE_5, (int)(scaled * 1024.0f));
            }
        }
        break;

        default:
            break;
    }
}

// tcflush on the board's port: 0 the input, 1 the output, 2 both. Only the replies wait here.
void driveboardFlush(int fd, int queue)
{
    if (getConfig()->emulateDriveboard && fd == (int)hooks[SERIAL0] && (queue == 0 || queue == 2))
    {
        replyFrames = 0;
        replyOffset = 0;
    }
}

int driveBoardioctl(int fd, unsigned int request, void *data)
{
    if (request == LINUX_TCFLSH) // its argument is the queue, not a pointer
    {
        driveboardFlush(fd, (int)(intptr_t)data);
        return 0;
    }
    int waiting = repliesWaiting();
    if (waiting == 0)
        return -1;
    if (request == LINUX_FIONREAD)
    {
        memcpy(data, &waiting, sizeof(waiting));
    }
    else
    {
        uint8_t d = 1;
        memcpy(data, &d, sizeof(uint8_t));
    }
    return 0;
}
