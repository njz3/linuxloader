// The agent mode: the game driven over stdin/stdout by another program, frame by frame. One command a line on
// stdin, one answer on stdout; the game's own output goes to stderr. Once the game draws its first frame:
// "ready GAME W H" (the picture's size). The commands are those of sm2-emu's and Supermodel's agent modes, so one
// client drives the three; what native code cannot do is answered "err".
//
//   step [N]
//       Run N frames (1 without N) with the holds as they are. Answer: "ok FRAME", then every byte written to the
//       drive board meanwhile, in hex, on the same line.
//   step N STEER ACCEL BRAKE [IN0 IN1]
//       The same, holding the steering (-1 left to 1 right, as sm2-emu's positional step), the accelerator and the
//       brake (0 to 1) first. IN0 and IN1, Model 2's button ports, are ignored.
//   set INPUT VALUE / setraw INPUT RAW / release INPUT|all
//       Hold an input by its name in `inputs` (controls.ini's names, P2_ for player 2; Steering and Accelerator as in
//       the other agent modes): an axis -1..1 the way a real wheel turns, +1 at its left end (Steering 1 fully
//       left), a pedal or another analogue control 0..1, a switch 0 or 1. RAW is what the game reads: an analogue
//       channel's count or a switch's state. Held from the next frame on; the controllers are not read in agent mode.
//       Answer: "ok", or "err ...".
//   get INPUT
//       What the game reads now: "value RAW" (a switch: 1 pressed; Coin: the player's coins).
//   inputs
//       "inputs N", then N lines "NAME TYPE", TYPE axis, analog or switch.
//   ffb
//       The force feedback since the last `ffb`, from the drive board model: "ffb N", then N lines
//       "FRAME Steering EFFECT FORCE", EFFECT constant, selfcenter (the spring), damper or vibrate (the sine), FORCE
//       -1..1 of the device's range, a constant > 0 pushing left. A line each time an effect changes.
//   servo on|off / servo
//       Record the frames the game writes to the drive board, or read them since the last `servo`: "servo N", then N
//       lines "FRAME B0 B1 B2 B3" in hex.
//   frame
//       "frame N", the frames drawn.
//   obs W H
//       The picture scaled to W x H: "obs W H", a new line, then W * H * 3 bytes, RGB, top row first.
//   shot PATH
//       The picture as a BMP, full size. Answer: "ok" or "err ...".
//   screen PATH
//       What the window shows, as a BMP at the window's size: the picture placed by the aspect ratio and the display
//       mode (black bars, stretching). Answer: "ok" or "err ...".
//   mem ADDRESS LENGTH / ram ADDRESS LENGTH
//       The game's memory at ADDRESS (hex): its program runs at its own addresses, 08048000 and up. Answer:
//       "mem LENGTH" (0 when unreadable), a new line, then the bytes.
//   tel [KEY,KEY...]
//       The telemetry, one JSON line: frame, rate, the game, its inputs (switches, analogue channels, coins), the
//       force feedback; with KEYs, only those keys.
//   perf [N]
//       Run N frames (600): "perf frames N wall MS ms/frame".
//   settle [MS] / bands N
//       Accepted for the other agent modes' clients: "settle 0", "ok".
//   save PATH / load PATH
//       "err": a native program's state cannot be saved.
//   quit
//       "bye", and the loader leaves.

#include <glad/gl.h>
#include <SDL3/SDL.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#include <fcntl.h>
#include <io.h>
#else
#include <unistd.h>
#endif

#include "agent.h"
#include "../config/config.h"
#include "../graphics/sdlCalls.h"
#include "../graphics/blitStretching.h"
#include "../hardware/lindbergh/forceFeedback.h"
#include "../hardware/lindbergh/jvs.h"
#include "../input/sdlInput.h"

static int active = 0;
static int shown = 0;
static FILE *answers = NULL; // the process's stdout, kept for the protocol; the game's goes to stderr
static Uint64 frames = 0;
static int stepsLeft = 0;    // frames to run before the step's answer
static int perfFrames = 0;   // with stepsLeft: `perf` rather than `step`
static Uint64 perfStart = 0;
static bool ready = false;

// --- stdin, read by a thread: a shown window keeps its events while the agent thinks

#define QUEUE_SIZE 64
#define LINE_SIZE 4096
static char queue[QUEUE_SIZE][LINE_SIZE];
static int queueHead = 0, queueCount = 0;
static bool stdinClosed = false;
static SDL_Mutex *queueLock = NULL;
static SDL_Condition *queueChanged = NULL;

// The bytes of stdin, read from the system rather than through the C library's stream: a thread waiting in fgets
// holds stdin's lock, and a game's fflush(NULL) waits for it forever (Dead Heat)
static int readSome(char *buffer, int size)
{
#ifdef _WIN32
    DWORD got = 0;
    if (!ReadFile(GetStdHandle(STD_INPUT_HANDLE), buffer, (DWORD)size, &got, NULL))
        return 0;
    return (int)got;
#else
    ssize_t got = read(0, buffer, (size_t)size);
    return got > 0 ? (int)got : 0;
#endif
}

static void queueLine(const char *line)
{
    SDL_LockMutex(queueLock);
    while (queueCount == QUEUE_SIZE)
        SDL_WaitCondition(queueChanged, queueLock);
    snprintf(queue[(queueHead + queueCount) % QUEUE_SIZE], LINE_SIZE, "%s", line);
    queueCount++;
    SDL_SignalCondition(queueChanged);
    SDL_UnlockMutex(queueLock);
}

static int SDLCALL readStdin(void *data)
{
    (void)data;
    char line[LINE_SIZE], chunk[512];
    int length = 0, got;
    while ((got = readSome(chunk, sizeof(chunk))) > 0)
    {
        for (int i = 0; i < got; i++)
        {
            if (chunk[i] == '\n')
            {
                line[length] = '\0';
                if (length && line[length - 1] == '\r')
                    line[length - 1] = '\0';
                queueLine(line);
                length = 0;
            }
            else if (length < LINE_SIZE - 1)
                line[length++] = chunk[i];
        }
    }
    SDL_LockMutex(queueLock);
    stdinClosed = true;
    SDL_SignalCondition(queueChanged);
    SDL_UnlockMutex(queueLock);
    return 0;
}

// The next command, waiting for it; false once stdin is closed
static bool nextCommand(char *line)
{
    SDL_LockMutex(queueLock);
    while (queueCount == 0 && !stdinClosed)
    {
        SDL_WaitConditionTimeout(queueChanged, queueLock, 20);
        if (queueCount == 0 && shown)
        {
            SDL_UnlockMutex(queueLock);
            SDL_PumpEvents();
            SDL_LockMutex(queueLock);
        }
    }
    bool got = queueCount > 0;
    if (got)
    {
        memcpy(line, queue[queueHead], LINE_SIZE);
        queueHead = (queueHead + 1) % QUEUE_SIZE;
        queueCount--;
        SDL_SignalCondition(queueChanged);
    }
    SDL_UnlockMutex(queueLock);
    return got;
}

static void answer(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    vfprintf(answers, format, args);
    va_end(args);
    fputc('\n', answers);
    fflush(answers);
}

// --- journals, filled from the game's threads

static SDL_Mutex *journalLock = NULL;

typedef struct
{
    char *text;
    size_t length, capacity;
    int lines;
} Journal;

static Journal driveBytes;  // the bytes since the last step, hex
static Journal servoLines;  // "FRAME B0 B1 B2 B3" since the last `servo`
static Journal ffbLines;    // "FRAME Steering EFFECT FORCE" since the last `ffb`
static bool servoOn = false;

static void journalAdd(Journal *journal, const char *format, ...)
{
    char text[128];
    va_list args;
    va_start(args, format);
    int length = vsnprintf(text, sizeof(text), format, args);
    va_end(args);
    if (length <= 0)
        return;
    if (journal->length + (size_t)length + 1 > journal->capacity)
    {
        size_t capacity = journal->capacity ? journal->capacity * 2 : 4096;
        while (capacity < journal->length + (size_t)length + 1)
            capacity *= 2;
        if (capacity > (64u << 20)) // an agent that never reads: the oldest is dropped
        {
            journal->length = 0;
            journal->lines = 0;
            capacity = journal->capacity;
        }
        else
        {
            char *grown = (char *)realloc(journal->text, capacity);
            if (!grown)
                return;
            journal->text = grown;
            journal->capacity = capacity;
        }
    }
    memcpy(journal->text + journal->length, text, (size_t)length + 1);
    journal->length += (size_t)length;
    journal->lines++;
}

static void journalClear(Journal *journal)
{
    journal->length = 0;
    journal->lines = 0;
    if (journal->text)
        journal->text[0] = '\0';
}

void agentDriveboard(const unsigned char *bytes, size_t count)
{
    if (!active)
        return;
    SDL_LockMutex(journalLock);
    for (size_t i = 0; i < count; i++)
        journalAdd(&driveBytes, " %02X", bytes[i]);
    if (servoOn)
        for (size_t i = 0; i + 4 <= count; i += 4)
            journalAdd(&servoLines, "%llu %02X %02X %02X %02X\n", (unsigned long long)frames, bytes[i], bytes[i + 1],
                       bytes[i + 2], bytes[i + 3]);
    SDL_UnlockMutex(journalLock);
}

// The board model's effects, a line for each one that changed since the last frame
static void sampleForceFeedback(void)
{
    static const char *names[4] = {"constant", "selfcenter", "damper", "vibrate"};
    static float last[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    float now[4];
    if (!sdlFfbAgentEffects(now))
        return;
    SDL_LockMutex(journalLock);
    for (int i = 0; i < 4; i++)
    {
        if (SDL_fabsf(now[i] - last[i]) < 0.0005f)
            continue;
        journalAdd(&ffbLines, "%llu Steering %s %.4f\n", (unsigned long long)frames, names[i], now[i]);
        last[i] = now[i];
    }
    SDL_UnlockMutex(journalLock);
}

// --- the picture

// The frame as the game drew it, RGB, bottom row first (OpenGL's order); to be freed
static unsigned char *readPicture(unsigned int fbo, int width, int height)
{
    unsigned char *pixels = (unsigned char *)malloc((size_t)width * height * 3);
    if (!pixels)
        return NULL;
    GLint packBuffer = 0, readFbo = 0;
    glad_glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING, &packBuffer);
    glad_glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &readFbo);
    glad_glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
    glad_glPushClientAttrib(GL_CLIENT_PIXEL_STORE_BIT);
    glad_glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glad_glPixelStorei(GL_PACK_ROW_LENGTH, 0);
    glad_glPixelStorei(GL_PACK_SKIP_PIXELS, 0);
    glad_glPixelStorei(GL_PACK_SKIP_ROWS, 0);
    glad_glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
    glad_glReadBuffer(fbo ? GL_COLOR_ATTACHMENT0 : GL_BACK);
    glad_glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels);
    glad_glBindFramebuffer(GL_READ_FRAMEBUFFER, readFbo);
    glad_glPopClientAttrib();
    glad_glBindBuffer(GL_PIXEL_PACK_BUFFER, packBuffer);
    return pixels;
}

static void commandObs(const char *arguments, unsigned int fbo, int width, int height)
{
    int w = 0, h = 0;
    if (sscanf(arguments, "%d %d", &w, &h) != 2 || w <= 0 || h <= 0 || w > 8192 || h > 8192)
    {
        answer("err usage: obs W H");
        return;
    }
    unsigned char *pixels = readPicture(fbo, width, height);
    unsigned char *scaled = (unsigned char *)malloc((size_t)w * h * 3);
    if (!pixels || !scaled)
    {
        free(pixels);
        free(scaled);
        answer("err out of memory");
        return;
    }
    // Each output pixel the average of the source pixels it covers, top row first
    for (int y = 0; y < h; y++)
    {
        int y0 = (h - 1 - y) * height / h, y1 = (h - y) * height / h;
        if (y1 <= y0)
            y1 = y0 + 1;
        for (int x = 0; x < w; x++)
        {
            int x0 = x * width / w, x1 = (x + 1) * width / w;
            if (x1 <= x0)
                x1 = x0 + 1;
            unsigned sum[3] = {0, 0, 0}, count = 0;
            for (int sy = y0; sy < y1 && sy < height; sy++)
                for (int sx = x0; sx < x1 && sx < width; sx++)
                {
                    const unsigned char *p = pixels + ((size_t)sy * width + sx) * 3;
                    sum[0] += p[0];
                    sum[1] += p[1];
                    sum[2] += p[2];
                    count++;
                }
            unsigned char *q = scaled + ((size_t)y * w + x) * 3;
            for (int c = 0; c < 3; c++)
                q[c] = (unsigned char)(count ? sum[c] / count : 0);
        }
    }
    fprintf(answers, "obs %d %d\n", w, h);
    fwrite(scaled, 1, (size_t)w * h * 3, answers);
    fflush(answers);
    free(pixels);
    free(scaled);
}

static void writeBmp(const char *path, unsigned char *pixels, int width, int height);

static void commandShot(const char *path, unsigned int fbo, int width, int height)
{
    if (!path[0])
    {
        answer("err usage: shot PATH");
        return;
    }
    writeBmp(path, readPicture(fbo, width, height), width, height);
}

extern int drawableW, drawableH; // blitStretching.c: the window's size in pixels

static void commandScreen(const char *path)
{
    if (!path[0])
    {
        answer("err usage: screen PATH");
        return;
    }
    blitAgentPresent(); // the picture placed in the window's back buffer, as at each swap
    writeBmp(path, readPicture(0, drawableW, drawableH), drawableW, drawableH);
}

// pixels: RGB, bottom row first, freed here
static void writeBmp(const char *path, unsigned char *pixels, int width, int height)
{
    if (!pixels)
    {
        answer("err out of memory");
        return;
    }
    FILE *file = fopen(path, "wb");
    if (!file)
    {
        free(pixels);
        answer("err cannot write %s", path);
        return;
    }
    // A BMP's rows go bottom up, like OpenGL's, in BGR, each padded to 4 bytes
    int rowSize = (width * 3 + 3) & ~3;
    uint32_t dataSize = (uint32_t)rowSize * height, fileSize = 54 + dataSize;
    unsigned char header[54] = {'B', 'M'};
    memcpy(header + 2, &fileSize, 4);
    header[10] = 54;
    header[14] = 40;
    int32_t w = width, h = height;
    memcpy(header + 18, &w, 4);
    memcpy(header + 22, &h, 4);
    header[26] = 1;
    header[28] = 24;
    memcpy(header + 34, &dataSize, 4);
    fwrite(header, 1, sizeof(header), file);
    unsigned char *row = (unsigned char *)calloc(1, rowSize);
    for (int y = 0; y < height && row; y++)
    {
        const unsigned char *p = pixels + (size_t)y * width * 3;
        for (int x = 0; x < width; x++)
        {
            row[x * 3] = p[x * 3 + 2];
            row[x * 3 + 1] = p[x * 3 + 1];
            row[x * 3 + 2] = p[x * 3];
        }
        fwrite(row, 1, rowSize, file);
    }
    free(row);
    fclose(file);
    free(pixels);
    answer("ok");
}

// --- memory

static bool readable(uintptr_t address, size_t length)
{
#ifdef _WIN32
    uintptr_t end = address + length;
    while (address < end)
    {
        MEMORY_BASIC_INFORMATION info;
        if (!VirtualQuery((LPCVOID)address, &info, sizeof(info)) || info.State != MEM_COMMIT ||
            (info.Protect & (PAGE_NOACCESS | PAGE_GUARD)) || !(info.Protect & 0xEE)) // any of the readable kinds
            return false;
        address = (uintptr_t)info.BaseAddress + info.RegionSize;
    }
    return true;
#else
    (void)address;
    (void)length;
    return false;
#endif
}

static void commandMem(const char *arguments)
{
    unsigned long address = 0, length = 0;
    if (sscanf(arguments, "%lx %lx", &address, &length) != 2 || length > (16u << 20))
    {
        answer("err usage: mem ADDRESS LENGTH (hex)");
        return;
    }
    if (!length || !readable(address, length))
    {
        answer("mem 0");
        return;
    }
    fprintf(answers, "mem %lu\n", length);
    fwrite((const void *)address, 1, length, answers);
    fflush(answers);
}

// --- inputs

typedef struct
{
    int player, action;
    AgentInputKind kind;
} Hold;

#define MAX_HOLDS 64
static Hold holds[MAX_HOLDS];
static int holdCount = 0;

static void rememberHold(int player, int action, AgentInputKind kind)
{
    for (int i = 0; i < holdCount; i++)
        if (holds[i].player == player && holds[i].action == action)
            return;
    if (holdCount < MAX_HOLDS)
        holds[holdCount++] = (Hold){player, action, kind};
}

// An axis's or a pedal's place at rest
static float restValue(AgentInputKind kind)
{
    return kind == AGENT_INPUT_AXIS ? 0.5f : 0.0f;
}

static void releaseHold(const Hold *hold)
{
    sdlInputAgentSet(hold->player, hold->action, hold->kind == AGENT_INPUT_SWITCH ? 0.0f : restValue(hold->kind));
}

// Hold by name; the steering turned from the wheel's -1..1 (+1 at the left) into the game's 0..1 (0 at the left)
static bool holdInput(const char *name, float value)
{
    int player, action;
    AgentInputKind kind;
    if (!sdlInputAgentFind(name, &player, &action, &kind))
        return false;
    if (kind == AGENT_INPUT_AXIS)
        value = (1.0f - SDL_clamp(value, -1.0f, 1.0f)) * 0.5f;
    sdlInputAgentSet(player, action, value);
    rememberHold(player, action, kind);
    return true;
}

static void emitInput(const char *name, AgentInputKind kind, void *context)
{
    static const char *types[3] = {"switch", "analog", "axis"};
    char *list = (char *)context;
    size_t length = strlen(list);
    snprintf(list + length, 16384 - length, "%s %s\n", name, types[kind]);
}

static void countLines(const char *text, int *lines)
{
    *lines = 0;
    for (const char *p = text; *p; p++)
        *lines += *p == '\n';
}

static void commandInputs(void)
{
    char *list = (char *)calloc(1, 16384);
    if (!list)
    {
        answer("err out of memory");
        return;
    }
    // The other agent modes' names for a wheel first, when the game has one
    int player, action;
    AgentInputKind kind;
    if (sdlInputAgentFind("Steer", &player, &action, &kind))
        strcat(list, "Steering axis\n");
    if (sdlInputAgentFind("Gas", &player, &action, &kind))
        strcat(list, "Accelerator analog\n");
    sdlInputAgentList(emitInput, list);
    int lines;
    countLines(list, &lines);
    fprintf(answers, "inputs %d\n%s", lines, list);
    fflush(answers);
    free(list);
}

// --- telemetry

static double rate(void)
{
    static Uint64 times[61];
    static int count = 0, next = 0;
    times[next] = SDL_GetTicksNS();
    next = (next + 1) % 61;
    if (count < 61)
        count++;
    if (count < 2)
        return 0.0;
    Uint64 newest = times[(next + 60) % 61], oldest = times[count < 61 ? 0 : next];
    return newest > oldest ? (count - 1) * 1e9 / (double)(newest - oldest) : 0.0;
}

static double currentRate = 0.0;

// A key is kept when no list is given or it is in the list
static bool wanted(const char *keys, const char *key)
{
    if (!keys || !keys[0])
        return true;
    size_t length = strlen(key);
    for (const char *p = keys; (p = strstr(p, key)) != NULL; p += length)
        if ((p == keys || p[-1] == ',') && (p[length] == ',' || p[length] == '\0'))
            return true;
    return false;
}

static void commandTel(const char *keys)
{
    char json[4096];
    size_t n = 0;
    JVSIO *io = getJVSIO();
    EmulatorConfig *config = getConfig();
#define ADD(...) n += (size_t)snprintf(json + n, sizeof(json) - n, __VA_ARGS__)
    ADD("{");
    bool first = true;
#define KEY(name) (wanted(keys, name) ? (first ? (first = false, ADD("\"%s\":", name), true) : (ADD(",\"%s\":", name), true)) : false)
    if (KEY("frame"))
        ADD("%llu", (unsigned long long)frames);
    if (KEY("fps"))
        ADD("%.2f", currentRate);
    if (KEY("game"))
        ADD("\"%s\"", config->gameShortTitle ? config->gameShortTitle : "");
    if (KEY("title"))
        ADD("\"%s\"", getGameName() ? getGameName() : "");
    if (KEY("switches"))
        ADD("[%d,%d,%d]", io->state.inputSwitch[0], io->state.inputSwitch[1], io->state.inputSwitch[2]);
    if (KEY("analog"))
    {
        ADD("[");
        for (int i = 0; i < 8; i++)
            ADD(i ? ",%d" : "%d", io->state.analogueChannel[i]);
        ADD("]");
    }
    if (KEY("coins"))
        ADD("[%d,%d]", io->state.coinCount[0], io->state.coinCount[1]);
    float effects[4];
    if (KEY("ffb"))
    {
        if (sdlFfbAgentEffects(effects))
            ADD("{\"constant\":%.4f,\"selfcenter\":%.4f,\"damper\":%.4f,\"vibrate\":%.4f}", effects[0], effects[1],
                effects[2], effects[3]);
        else
            ADD("null");
    }
    ADD("}");
#undef KEY
#undef ADD
    answer("%s", json);
}

// --- commands

static void answerStep(void)
{
    SDL_LockMutex(journalLock);
    fprintf(answers, "ok %llu%s\n", (unsigned long long)frames, driveBytes.text ? driveBytes.text : "");
    journalClear(&driveBytes);
    SDL_UnlockMutex(journalLock);
    fflush(answers);
}

static void printJournal(const char *name, Journal *journal)
{
    SDL_LockMutex(journalLock);
    fprintf(answers, "%s %d\n%s", name, journal->lines, journal->text ? journal->text : "");
    journalClear(journal);
    SDL_UnlockMutex(journalLock);
    fflush(answers);
}

// One command; true when it lets the game run (a step)
static bool serve(char *line, unsigned int fbo, int width, int height)
{
    char command[32] = "";
    int consumed = 0;
    if (sscanf(line, "%31s%n", command, &consumed) != 1)
        return false;
    char *arguments = line + consumed;
    while (*arguments == ' ' || *arguments == '\t')
        arguments++;

    if (strcmp(command, "step") == 0 || strcmp(command, "perf") == 0)
    {
        int count = strcmp(command, "perf") == 0 ? 600 : 1;
        float steer, accel, brake;
        int fields = sscanf(arguments, "%d %f %f %f", &count, &steer, &accel, &brake);
        if (fields == 4)
        {
            // The positional step's steering: -1 left, 1 right
            int player, action;
            AgentInputKind kind;
            if (sdlInputAgentFind("Steer", &player, &action, &kind))
            {
                sdlInputAgentSet(player, action, (SDL_clamp(steer, -1.0f, 1.0f) + 1.0f) * 0.5f);
                rememberHold(player, action, kind);
            }
            holdInput("Gas", accel);
            holdInput("Brake", brake);
        }
        if (count <= 0)
        {
            answerStep();
            return false;
        }
        stepsLeft = count;
        perfFrames = strcmp(command, "perf") == 0 ? count : 0;
        perfStart = SDL_GetTicksNS();
        return true;
    }
    if (strcmp(command, "set") == 0 || strcmp(command, "setraw") == 0)
    {
        char name[80];
        double value;
        if (sscanf(arguments, "%79s %lf", name, &value) != 2)
        {
            answer("err usage: %s INPUT VALUE", command);
            return false;
        }
        if (strcmp(command, "setraw") == 0)
        {
            int player, action;
            AgentInputKind kind;
            long raw = strtol(strchr(arguments, ' ') + 1, NULL, 0);
            if (!sdlInputAgentFind(name, &player, &action, &kind))
                answer("err unknown input %s", name);
            else
            {
                sdlInputAgentSetRaw(player, action, (int)raw);
                answer("ok");
            }
            return false;
        }
        if (holdInput(name, (float)value))
            answer("ok");
        else
            answer("err unknown input %s", name);
        return false;
    }
    if (strcmp(command, "release") == 0)
    {
        if (strcmp(arguments, "all") == 0)
        {
            for (int i = 0; i < holdCount; i++)
                releaseHold(&holds[i]);
            holdCount = 0;
            answer("ok");
            return false;
        }
        int player, action;
        AgentInputKind kind;
        if (!sdlInputAgentFind(arguments, &player, &action, &kind))
        {
            answer("err unknown input %s", arguments);
            return false;
        }
        Hold hold = {player, action, kind};
        releaseHold(&hold);
        for (int i = 0; i < holdCount; i++)
            if (holds[i].player == player && holds[i].action == action)
                holds[i] = holds[--holdCount];
        answer("ok");
        return false;
    }
    if (strcmp(command, "get") == 0)
    {
        int player, action;
        AgentInputKind kind;
        if (!sdlInputAgentFind(arguments, &player, &action, &kind))
            answer("err unknown input %s", arguments);
        else
            answer("value %d", sdlInputAgentGet(player, action));
        return false;
    }
    if (strcmp(command, "inputs") == 0)
        commandInputs();
    else if (strcmp(command, "ffb") == 0)
        printJournal("ffb", &ffbLines);
    else if (strcmp(command, "servo") == 0)
    {
        if (strcmp(arguments, "on") == 0 || strcmp(arguments, "off") == 0)
        {
            SDL_LockMutex(journalLock);
            servoOn = strcmp(arguments, "on") == 0;
            journalClear(&servoLines);
            SDL_UnlockMutex(journalLock);
            answer("ok");
        }
        else
            printJournal("servo", &servoLines);
    }
    else if (strcmp(command, "frame") == 0)
        answer("frame %llu", (unsigned long long)frames);
    else if (strcmp(command, "obs") == 0)
        commandObs(arguments, fbo, width, height);
    else if (strcmp(command, "shot") == 0)
        commandShot(arguments, fbo, width, height);
    else if (strcmp(command, "screen") == 0)
        commandScreen(arguments);
    else if (strcmp(command, "mem") == 0 || strcmp(command, "ram") == 0)
        commandMem(arguments);
    else if (strcmp(command, "tel") == 0)
        commandTel(arguments);
    else if (strcmp(command, "settle") == 0)
        answer("settle 0");
    else if (strcmp(command, "bands") == 0)
        answer("ok");
    else if (strcmp(command, "save") == 0 || strcmp(command, "load") == 0)
        answer("err a native program's state cannot be saved");
    else if (strcmp(command, "quit") == 0)
    {
        // exit() would flush stdin, which the reader thread holds while it waits: straight out
        answer("bye");
        fflush(stderr);
        _exit(0);
    }
    else
        answer("err unknown command %s", command);
    return false;
}

void agentFrame(unsigned int fbo, int width, int height)
{
    if (!active)
        return;
    frames++;
    currentRate = rate();
    sampleForceFeedback();

    if (!ready)
    {
        ready = true;
        if (!shown)
            SDL_GL_SetSwapInterval(0); // no pacing
        EmulatorConfig *config = getConfig();
        answer("ready %s %d %d", config->gameShortTitle ? config->gameShortTitle : "lindbergh", width, height);
    }
    else if (stepsLeft > 0)
    {
        if (--stepsLeft > 0)
            return;
        if (perfFrames)
        {
            double wall = (double)(SDL_GetTicksNS() - perfStart) / 1e6;
            answer("perf frames %d wall %.1f %.3f ms/frame", perfFrames, wall, wall / perfFrames);
            perfFrames = 0;
            SDL_LockMutex(journalLock);
            journalClear(&driveBytes);
            SDL_UnlockMutex(journalLock);
        }
        else
            answerStep();
    }

    char line[LINE_SIZE];
    for (;;)
    {
        if (!nextCommand(line))
        {
            fprintf(stderr, "agent: end of the commands, leaving\n");
            sdlQuit();
            return;
        }
        if (serve(line, fbo, width, height))
            break;
    }
    processChangedActions(); // the holds reach the game in the frame that starts
}

void agentInit(int show)
{
    active = 1;
    shown = show;
    // The answers on the process's stdout; the game and the loader print to stderr
#ifdef _WIN32
    int protocol = _dup(_fileno(stdout));
    _dup2(_fileno(stderr), _fileno(stdout));
    SetStdHandle(STD_OUTPUT_HANDLE, GetStdHandle(STD_ERROR_HANDLE));
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(protocol, _O_BINARY);
    answers = _fdopen(protocol, "wb");
#else
    int protocol = dup(1);
    dup2(2, 1);
    answers = fdopen(protocol, "w");
#endif
    if (!answers)
        answers = stderr;
    setvbuf(answers, NULL, _IOFBF, 1 << 16);
    queueLock = SDL_CreateMutex();
    queueChanged = SDL_CreateCondition();
    journalLock = SDL_CreateMutex();
    SDL_DetachThread(SDL_CreateThread(readStdin, "agent stdin", NULL));
}

int agentActive(void)
{
    return active;
}

int agentHidden(void)
{
    return active && !shown;
}
