#include <SDL3/SDL.h>
#include <SDL3/SDL_haptic.h>
#include <stdio.h>
#include <string.h>

#include "forceFeedback.h"
#include "../../config/config.h"
#include "../../input/sdlInput.h"
#include "../../log/log.h"

static int effect_id = -1;
extern SDLControllers sdlJoysticks;

// Wheel mode: devices without LEFTRIGHT rumble but with real force effects (FFB wheels, vJoy). The drive board is
// emulated as the Sega servo board (RS422) of the cabinet: its commands set the board's parameters, and the wheel's
// constant, spring, damper and sine effects follow them. Protocol: BackForceFeeder's DRIVEBOARD.md, "Sega
// servoboard".
// The game's thread only updates the board and wakes the FFB thread, which alone talks to the wheel: a slow wheel (a
// DirectInput call can block for milliseconds) never holds the game back, and the effects it could not send in time
// give way to the latest ones.
static bool wheelMode = false;
static int wheelConstantId = -1;
static int wheelSpringId = -1;
static int wheelDamperId = -1;
static Uint16 wheelDamperType = SDL_HAPTIC_DAMPER;
static int wheelSineId = -1;
static SDL_Mutex *boardLock = NULL;
static SDL_Condition *boardChanged = NULL;
static SDL_Thread *wheelThread = NULL;
static bool wheelQuit = false;
static bool wheelPaused = false; // F10 settings open: the game waits, the wheel goes limp

static int SDLCALL wheelThreadMain(void *data);

// The servo board's parameters, on its 7-bit scales
typedef struct
{
    bool enabled;      // 80 xx 01: effects on
    bool shortMode;    // 80 01 xx: the torque drops about 240 ms after the last frame
    int maxForce;      // 81: cap of every output
    int gain;          // 83: an output is its value times gain/127
    int torque;        // 84, centred: + pushes the wheel to the left
    int sineFrequency; // 85, in half Hertz
    int sineAmplitude; // 85
    int damperSpeed;   // 86
    int damperParam;   // 86
    int springOffset;  // 87, centred encoder counts: + to the left, 128 is about 90 degrees
    bool springOn;     // 88 00 xx
    int springForce;   // 8B
    Uint64 lastFrameMs;
    bool torqueDropped;
    // The parameter table, 16 pages of 16 values of 14 bits, written by 9D index page then 9E value. A page holds
    // a duration at index 0 and torque samples around 0x80 after it, which FB plays (DRIVEBOARD.md, "Reading the
    // parameters back").
    Uint16 pages[16][16];
    int selectedIndex; // 9D, -1 for none
    int selectedPage;
    bool dirty; // changed since the FFB thread last sent the effects
} ServoBoard;

static ServoBoard board;

// The page FB is playing
typedef struct
{
    bool playing;
    int page;
    int sign; // FB's bit 0x10 plays the page the other way
    Uint64 startMs;
    int index; // the sample being played
    int level; // the sample, centred, + to the left like 84
} PagePlayer;

static PagePlayer player;

// The board as it powers up or after FF 00 00: its registers read cold on a bench, effects off until 80.
static void boardReset(void)
{
    memset(&board, 0, sizeof(board));
    board.maxForce = 0x40;
    board.gain = 0x40;
    board.damperSpeed = 1;
    board.springOn = true;
    board.lastFrameMs = SDL_GetTicks();
    // Silent pages until the game writes its own
    for (int page = 0; page < 16; page++)
    {
        board.pages[page][0] = 0x10;
        for (int index = 1; index < 16; index++)
            board.pages[page][index] = 0x80;
    }
    board.selectedIndex = -1;
    board.dirty = true;
    memset(&player, 0, sizeof(player));
}

static bool hapticUsable(SDL_Haptic *haptic)
{
    return (SDL_GetHapticFeatures(haptic) & (SDL_HAPTIC_LEFTRIGHT | SDL_HAPTIC_CONSTANT)) != 0;
}

// Created stopped: the FFB thread starts each effect with its first parameters
static int wheelCreateEffect(SDL_Haptic *haptic, SDL_HapticEffect *effect)
{
    int id = SDL_CreateHapticEffect(haptic, effect);
    if (id < 0)
        log_warn("FFB: Cannot create effect 0x%x: %s", effect->type, SDL_GetError());
    return id < 0 ? -1 : id;
}

static void wheelInit(SDL_Haptic *haptic)
{
    unsigned int features = SDL_GetHapticFeatures(haptic);
    SDL_HapticEffect effect;

    wheelMode = true;
    boardLock = SDL_CreateMutex();
    boardReset();

    if (features & SDL_HAPTIC_GAIN)
        SDL_SetHapticGain(haptic, 100);
    if (features & SDL_HAPTIC_AUTOCENTER)
        SDL_SetHapticAutocenter(haptic, 0);

    memset(&effect, 0, sizeof(effect));
    effect.type = SDL_HAPTIC_CONSTANT;
    effect.constant.direction.type = SDL_HAPTIC_STEERING_AXIS;
    effect.constant.length = SDL_HAPTIC_INFINITY;
    wheelConstantId = wheelCreateEffect(haptic, &effect);

    if (features & SDL_HAPTIC_SPRING)
    {
        memset(&effect, 0, sizeof(effect));
        effect.type = SDL_HAPTIC_SPRING;
        effect.condition.direction.type = SDL_HAPTIC_STEERING_AXIS;
        effect.condition.length = SDL_HAPTIC_INFINITY;
        wheelSpringId = wheelCreateEffect(haptic, &effect);
    }

    // The board's 86 is a damper; a wheel without one gets a friction instead
    wheelDamperType = (features & SDL_HAPTIC_DAMPER) ? SDL_HAPTIC_DAMPER : SDL_HAPTIC_FRICTION;
    if (features & wheelDamperType)
    {
        memset(&effect, 0, sizeof(effect));
        effect.type = wheelDamperType;
        effect.condition.direction.type = SDL_HAPTIC_STEERING_AXIS;
        effect.condition.length = SDL_HAPTIC_INFINITY;
        wheelDamperId = wheelCreateEffect(haptic, &effect);
    }

    if (features & SDL_HAPTIC_SINE)
    {
        memset(&effect, 0, sizeof(effect));
        effect.type = SDL_HAPTIC_SINE;
        effect.periodic.direction.type = SDL_HAPTIC_STEERING_AXIS;
        effect.periodic.length = SDL_HAPTIC_INFINITY;
        effect.periodic.period = 50;
        wheelSineId = wheelCreateEffect(haptic, &effect);
    }

    log_warn("FFB: Wheel mode (constant=%d spring=%d %s=%d sine=%d)", wheelConstantId, wheelSpringId,
             wheelDamperType == SDL_HAPTIC_DAMPER ? "damper" : "friction", wheelDamperId, wheelSineId);

    boardChanged = SDL_CreateCondition();
    wheelThread = SDL_CreateThread(wheelThreadMain, "FFB", NULL);
    if (!wheelThread)
        log_error("FFB: Cannot start the FFB thread: %s", SDL_GetError());
}

// The games repeat their frames (OutRun sends 80 01 01 every frame at startup): an effect is sent to the wheel only
// when it changes, and started only when it is not playing, to spare the USB link. Each is a synchronous HID report
// in DirectInput. An update that fails stops the effect, which is sent and started again at the next change.
#define WHEEL_EFFECTS 8
static SDL_HapticEffect wheelSent[WHEEL_EFFECTS];
static bool wheelSentValid[WHEEL_EFFECTS];
static bool wheelPlaying[WHEEL_EFFECTS];

static void wheelUpdate(int id, const SDL_HapticEffect *effect)
{
    if (id < 0 || id >= WHEEL_EFFECTS)
        return;
    if (wheelSentValid[id] && wheelPlaying[id] && memcmp(&wheelSent[id], effect, sizeof(*effect)) == 0)
        return;
    if (!SDL_UpdateHapticEffect(sdlJoysticks.haptics[0], id, effect))
    {
        log_warn("FFB: Cannot update effect %d: %s", id, SDL_GetError());
        SDL_StopHapticEffect(sdlJoysticks.haptics[0], id);
        wheelSentValid[id] = false;
        wheelPlaying[id] = false;
        return;
    }
    wheelSent[id] = *effect;
    wheelSentValid[id] = true;
    if (!wheelPlaying[id])
        wheelPlaying[id] = SDL_RunHapticEffect(sdlJoysticks.haptics[0], id, 1);
}

// force: -1.0 (full right) .. 1.0 (full left)
static void wheelConstant(float force)
{
    if (wheelConstantId < 0 || !sdlJoysticks.haptics[0])
        return;
    SDL_HapticEffect effect;
    memset(&effect, 0, sizeof(effect));
    effect.type = SDL_HAPTIC_CONSTANT;
    effect.constant.direction.type = SDL_HAPTIC_STEERING_AXIS;
    effect.constant.length = SDL_HAPTIC_INFINITY;
    effect.constant.level = (Sint16)(SDL_clamp(force, -1.0f, 1.0f) * 32767.0f);
    wheelUpdate(wheelConstantId, &effect);
}

// strength, saturation: 0.0 .. 1.0; centre: -1.0 (left end of the axis) .. 1.0 (right end)
static void wheelCondition(int id, Uint16 type, float strength, float saturation, float centre)
{
    if (id < 0 || !sdlJoysticks.haptics[0])
        return;
    Sint16 coeff = (Sint16)(SDL_clamp(strength, 0.0f, 1.0f) * 32767.0f);
    Uint16 sat = (Uint16)(SDL_clamp(saturation, 0.0f, 1.0f) * 65535.0f);
    SDL_HapticEffect effect;
    memset(&effect, 0, sizeof(effect));
    effect.type = type;
    effect.condition.direction.type = SDL_HAPTIC_STEERING_AXIS;
    effect.condition.length = SDL_HAPTIC_INFINITY;
    effect.condition.right_sat[0] = sat;
    effect.condition.left_sat[0] = sat;
    effect.condition.right_coeff[0] = coeff;
    effect.condition.left_coeff[0] = coeff;
    effect.condition.center[0] = (Sint16)(SDL_clamp(centre, -1.0f, 1.0f) * 32767.0f);
    wheelUpdate(id, &effect);
}

static void wheelSine(float strength, int periodMs)
{
    if (wheelSineId < 0 || wheelSineId >= WHEEL_EFFECTS || !sdlJoysticks.haptics[0])
        return;
    if (strength <= 0.0f)
    {
        if (wheelPlaying[wheelSineId])
            SDL_StopHapticEffect(sdlJoysticks.haptics[0], wheelSineId);
        wheelPlaying[wheelSineId] = false;
        return;
    }
    SDL_HapticEffect effect;
    memset(&effect, 0, sizeof(effect));
    effect.type = SDL_HAPTIC_SINE;
    effect.periodic.direction.type = SDL_HAPTIC_STEERING_AXIS;
    effect.periodic.length = SDL_HAPTIC_INFINITY;
    effect.periodic.period = (Uint16)SDL_clamp(periodMs, 1, 0xFFFF);
    effect.periodic.magnitude = (Sint16)(SDL_clamp(strength, 0.0f, 1.0f) * 32767.0f);
    wheelUpdate(wheelSineId, &effect);
}

// An output of the board as a fraction of the wheel's strength: the value, on the board's 0..127 scale, times the
// gain 83/127, held within the cap 81 (measured on a board: the cap applies after the gain), then FFB_STRENGTH.
static float boardOutput(float value)
{
    float out = value * (float)board.gain / 127.0f;
    out = SDL_clamp(out, -(float)board.maxForce, (float)board.maxForce);
    return out / 127.0f * (float)getConfig()->ffbStrength / 100.0f;
}

// Index 0 of a page taken as the duration of each sample, in milliseconds. OutRun writes 0x10 in all its pages,
// whose alternating samples then vibrate at about 16 Hz. Not measured on a board.
static Uint64 pagePeriodMs(void)
{
    int period = board.pages[player.page][0];
    return (Uint64)(period > 0 ? SDL_min(period, 1000) : 16);
}

// FB 00 pp: play page pp & 0F, the other way with bit 0x10, on top of the torque of 84
static void pagePlay(uint8_t code, Uint64 now)
{
    player.playing = true;
    player.page = code & 0x0F;
    player.sign = (code & 0x10) ? -1 : 1;
    player.startMs = now;
    player.index = 1;
    player.level = player.sign * ((int)board.pages[player.page][1] - 0x80);
    board.dirty = true;
}

// The board's own timing at now: the sample of the page being played, and the torque the short mode drops. Returns
// the milliseconds until the next change, -1 for none. The sample follows the time, so a wheel that held the FFB
// thread back skips samples rather than playing the page late.
static Sint32 boardTimers(Uint64 now)
{
    Sint32 wait = -1;
    if (player.playing)
    {
        Uint64 period = pagePeriodMs();
        Uint64 step = (now - player.startMs) / period;
        int index = step < 15 ? 1 + (int)step : 16;
        if (index != player.index)
        {
            player.index = index;
            player.level = index < 16 ? player.sign * ((int)board.pages[player.page][index] - 0x80) : 0;
            board.dirty = true;
        }
        if (index < 16)
            wait = (Sint32)(player.startMs + (step + 1) * period - now);
        else
            player.playing = false;
    }
    if (board.shortMode && !board.torqueDropped && board.torque != 0)
    {
        Uint64 idle = now - board.lastFrameMs;
        if (idle > 240)
        {
            board.torqueDropped = true;
            board.dirty = true;
        }
        else if (wait < 0 || (Sint32)(241 - idle) < wait)
            wait = (Sint32)(241 - idle);
    }
    return wait;
}

// The wheel's effects for the board's state, computed under the lock and sent after it
typedef struct
{
    float constant;
    float springStrength;
    float springSaturation;
    float springCentre;
    float damperStrength;
    float sineStrength;
    int sinePeriodMs;
} WheelEffects;

static WheelEffects boardEffects(void)
{
    WheelEffects effects;
    if (wheelPaused)
    {
        memset(&effects, 0, sizeof(effects));
        return effects;
    }
    int torque = board.torqueDropped ? 0 : board.torque;
    float level = board.enabled ? (float)SDL_clamp(torque + player.level, -127, 127) : 0.0f;
    // The board's + is to the left, like SDL's: the steering axis is the cartesian (1, 0), a force coming from the
    // right (0x4000 on Linux, 90 degrees in DirectInput as vJoy reports it), so a positive level pushes to the left
    float sign = getConfig()->ffbInvert ? -1.0f : 1.0f;
    effects.constant = sign * boardOutput(level);

    effects.springStrength = (board.enabled && board.springOn) ? boardOutput((float)board.springForce) : 0.0f;
    effects.springSaturation = boardOutput(127.0f);
    // 128 counts, about 90 degrees, taken as two thirds of the half travel of a 270 degree wheel
    effects.springCentre = -(float)board.springOffset / 128.0f * (2.0f / 3.0f);

    // Behaves like the ratio of its two parameters: 86 10 7F strong, 86 40 40 light, 86 xx 00 none
    float ratio = board.damperSpeed > 0x10 ? 16.0f / (float)board.damperSpeed : 1.0f;
    effects.damperStrength = board.enabled ? boardOutput((float)board.damperParam * ratio) : 0.0f;

    if (!board.enabled || board.sineFrequency == 0 || board.sineAmplitude == 0)
    {
        effects.sineStrength = 0.0f;
        effects.sinePeriodMs = 0;
    }
    else
    {
        effects.sineStrength = boardOutput((float)board.sineAmplitude);
        effects.sinePeriodMs = 2000 / board.sineFrequency;
    }
    return effects;
}

static void wheelSend(const WheelEffects *effects)
{
    wheelConstant(effects->constant);
    wheelCondition(wheelSpringId, SDL_HAPTIC_SPRING, effects->springStrength, effects->springSaturation,
                   effects->springCentre);
    wheelCondition(wheelDamperId, wheelDamperType, effects->damperStrength, 1.0f, 0.0f);
    wheelSine(effects->sineStrength, effects->sinePeriodMs);
}

// The FFB thread: sleeps until the board changes or its next timed change, then sends the latest effects. Only the
// copy of the board's state is made under the lock, never a call to the wheel.
static int SDLCALL wheelThreadMain(void *data)
{
    (void)data;
    SDL_LockMutex(boardLock);
    while (!wheelQuit)
    {
        Sint32 wait = boardTimers(SDL_GetTicks());
        if (!board.dirty)
        {
            SDL_WaitConditionTimeout(boardChanged, boardLock, wait);
            continue;
        }
        board.dirty = false;
        WheelEffects effects = boardEffects();
        SDL_UnlockMutex(boardLock);
        wheelSend(&effects);
        SDL_LockMutex(boardLock);
    }
    SDL_UnlockMutex(boardLock);
    return 0;
}

// One 4-byte frame from the game: opcode, two 7-bit values, checksum. Runs on the game's thread: it only updates the
// board and wakes the FFB thread.
static void sdlFfbWheelDriveboard(const unsigned char *buffer)
{
    int centred = ((buffer[1] & 0x7F) << 7 | (buffer[2] & 0x7F)) - 0x80;
    Uint64 now = SDL_GetTicks();

    SDL_LockMutex(boardLock);
    board.lastFrameMs = now;
    if (board.torqueDropped) // any frame keeps the torque of the short mode, and an 84 after a drop is taken again
    {
        board.torqueDropped = false;
        board.dirty = true;
    }

    switch (buffer[0])
    {
        case 0x80: // effect mode: 00 permanent / 01 short, then 01 enable / 00 disable
            board.shortMode = buffer[1] != 0;
            board.enabled = buffer[2] != 0;
            board.dirty = true;
            break;
        case 0x81: // maximum force, forward / backward of the wheel's motion: the first byte caps both ways
            board.maxForce = buffer[1] & 0x7F;
            board.dirty = true;
            break;
        case 0x83: // global gain
            board.gain = buffer[1] & 0x7F;
            board.dirty = true;
            break;
        case 0x84: // constant torque
            board.torque = centred;
            board.dirty = true;
            break;
        case 0x85: // sine: frequency in half Hertz, amplitude; either at 0 stops it
            board.sineFrequency = buffer[1] & 0x7F;
            board.sineAmplitude = buffer[2] & 0x7F;
            board.dirty = true;
            break;
        case 0x86: // damper: a first parameter of 0 is stored as 1
            board.damperSpeed = (buffer[1] & 0x7F) ? (buffer[1] & 0x7F) : 1;
            board.damperParam = buffer[2] & 0x7F;
            board.dirty = true;
            break;
        case 0x87: // spring centre
            board.springOffset = centred;
            board.dirty = true;
            break;
        case 0x88: // spring: 00 enable, 01 disable
            board.springOn = buffer[1] == 0;
            board.dirty = true;
            break;
        case 0x8B: // spring force
            board.springForce = buffer[1] & 0x7F;
            board.dirty = true;
            break;
        case 0x9D: // select a parameter: index, page
            board.selectedIndex = (buffer[1] < 16 && buffer[2] < 16) ? buffer[1] : -1;
            board.selectedPage = buffer[2];
            break;
        case 0x9E: // write the selected parameter, 14 bits
            if (board.selectedIndex >= 0)
                board.pages[board.selectedPage][board.selectedIndex] = (Uint16)((buffer[1] & 0x7F) << 7 | (buffer[2] & 0x7F));
            break;
        case 0xFB: // play a page of the parameter table (OutRun: collisions)
            pagePlay(buffer[2], now);
            break;
        case 0xFF: // reset: the board back to its defaults, effects off
            boardReset();
            break;
        default: // 82, 89, 8A, F0, FA, FC, FD: no force of their own
            break;
    }
    if (board.dirty)
        SDL_SignalCondition(boardChanged);
    SDL_UnlockMutex(boardLock);
}

void sdlFfbInit(void)
{
    // SDL input mode: open haptic from already-opened joysticks
    for (int i = 0; i < sdlJoysticks.joysticksCount && i < MAX_JOYSTICKS; ++i)
    {
        SDL_Joystick *joy = NULL;

        if (sdlJoysticks.controllers[i])
            joy = SDL_GetGamepadJoystick(sdlJoysticks.controllers[i]);
        else if (sdlJoysticks.joysticks[i])
            joy = sdlJoysticks.joysticks[i];

        if (!joy)
            continue;

        if (!SDL_IsJoystickHaptic(joy))
            continue;

        sdlJoysticks.haptics[i] = SDL_OpenHapticFromJoystick(joy);
        if (sdlJoysticks.haptics[i])
        {
            if (hapticUsable(sdlJoysticks.haptics[i]))
            {
                log_warn("FFB: Haptic opened from joystick %d (%s)", i, SDL_GetJoystickName(joy));
                if (i == 0 && !(SDL_GetHapticFeatures(sdlJoysticks.haptics[i]) & SDL_HAPTIC_LEFTRIGHT))
                    wheelInit(sdlJoysticks.haptics[i]);
                else
                    sdlFfbRumble(1.0, 1.0, 200);
            }
            else
            {
                log_warn("FFB: Joystick %d has no LEFTRIGHT or CONSTANT support, skipping", i);
                SDL_CloseHaptic(sdlJoysticks.haptics[i]);
                sdlJoysticks.haptics[i] = NULL;
            }
        }
    }

    // EVDEV input mode: fall back to standalone haptic devices
    if (!sdlJoysticks.haptics[0])
    {
        int num_haptics;
        SDL_HapticID *haptics = SDL_GetHaptics(&num_haptics);
        if (haptics)
        {
            for (int i = 0; i < num_haptics && i < MAX_JOYSTICKS; ++i)
            {
                if (sdlJoysticks.haptics[i])
                    continue;

                sdlJoysticks.haptics[i] = SDL_OpenHaptic(haptics[i]);
                if (sdlJoysticks.haptics[i])
                {
                    if (hapticUsable(sdlJoysticks.haptics[i]))
                    {
                        log_warn("FFB: Standalone haptic %d: %s", i, SDL_GetHapticNameForID(haptics[i]));
                        if (i == 0 && !(SDL_GetHapticFeatures(sdlJoysticks.haptics[i]) & SDL_HAPTIC_LEFTRIGHT))
                            wheelInit(sdlJoysticks.haptics[i]);
                        else
                            sdlFfbRumble(1.0, 1.0, 200);
                    }
                    else
                    {
                        log_warn("FFB: Standalone haptic %d has no LEFTRIGHT or CONSTANT support, skipping", i);
                        SDL_CloseHaptic(sdlJoysticks.haptics[i]);
                        sdlJoysticks.haptics[i] = NULL;
                    }
                }
            }
            SDL_free(haptics);
        }
    }

    if (!sdlJoysticks.haptics[0])
        log_warn("FFB: No haptic device found");
}

void sdlFfbRumble(float left, float right, int duration_ms)
{
    if (!sdlJoysticks.haptics[0])
        return;
    if (wheelMode) // the wheel plays the drive board's effects only, from the FFB thread
        return;
    SDL_HapticEffect effect;
    memset(&effect, 0, sizeof(effect));
    effect.type = SDL_HAPTIC_LEFTRIGHT;
    effect.leftright.length = duration_ms;
    effect.leftright.large_magnitude = (Uint16)(left * 0xFFFF);
    effect.leftright.small_magnitude = (Uint16)(right * 0xFFFF);
    if (effect_id >= 0)
        SDL_DestroyHapticEffect(sdlJoysticks.haptics[0], effect_id);
    effect_id = SDL_CreateHapticEffect(sdlJoysticks.haptics[0], &effect);
    SDL_RunHapticEffect(sdlJoysticks.haptics[0], effect_id, 1);
}

// The game paused (F10): no force on the wheel until it runs again, then the board's state as it was
void sdlFfbPause(int paused)
{
    if (!wheelMode)
        return;
    SDL_LockMutex(boardLock);
    wheelPaused = paused != 0;
    board.dirty = true;
    SDL_SignalCondition(boardChanged);
    SDL_UnlockMutex(boardLock);
}

void sdlFfbShutdown(void)
{
    if (wheelThread)
    {
        SDL_LockMutex(boardLock);
        wheelQuit = true;
        SDL_SignalCondition(boardChanged);
        SDL_UnlockMutex(boardLock);
        SDL_WaitThread(wheelThread, NULL);
        wheelThread = NULL;
    }
    if (sdlJoysticks.haptics[0])
    {
        SDL_CloseHaptic(sdlJoysticks.haptics[0]);
        sdlJoysticks.haptics[0] = NULL;
    }
}

void sdlFfbDriveboard(const unsigned char *buffer, size_t count)
{

    // printf("FFB driveboard: count=%zu, data:", count);
    // for (size_t i = 0; i < count; ++i) {
    //     printf(" 0x%02x", buffer[i]);
    // }
    // printf("\n");

    if (wheelMode && sdlJoysticks.haptics[0])
    {
        sdlFfbWheelDriveboard(buffer);
        return;
    }

    switch (buffer[0])
    {
        case 0x80:
        { // power
            if (buffer[2] == 0x01)
            {
                log_info("0x80 command: Triggering full-power rumble\n");
                // ffb_rumble(1.0f, 1.0f, 500);
            }
            break;
        }
        case 0x85:
        { // Rumble/vibrate
            // uint8_t speed = buffer[1];
            uint8_t power = buffer[2];

            float force = (float)power / 63.0f; // Map power 1-63 to 0.0-1.0

            int duration = 100;
            log_info("Triggering rumble: force=%.2f, duration=%dms\n", force, duration);
            sdlFfbRumble(force, force, duration);
            break;
        }
        case 0x84:
        { // Movement command
            uint8_t direction = buffer[1];
            uint8_t value = buffer[2];

            float left = 0.0f, right = 0.0f;
            int duration = 100; // ms

            if (direction == 0x00)
            { // right - value from 0x7F (min) to 0x01 (max)

                right = (127.0f - (float)value) / 127.0f;
            }
            else if (direction == 0x01)
            { // left  - value goes from 0x00 (min) to 0x7F (max)

                left = (float)value / 127.0f;
            }

            log_info("Triggering movement rumble: left=%.2f, right=%.2f, duration=%dms\n", left, right, duration);
            sdlFfbRumble(left, right, duration);
            break;
        }
        case 0x86: // Friction
            log_info("Friction:");
            printf(" Power: %d", buffer[1]);
            printf(" / Percentage: %d\n", buffer[2]);
            break;
        case 0x8:
            break;
        case 0x87: // move and set target point
            printf("Move and set target point\n");
            break;
        case 0x88: // move to current target point
            printf("Move to current target point\n");
            break;
        case 0x8B: // centering strength
            printf("Centering strength:");
            printf(" param1: %d", buffer[1]);
            printf(" / param2: %d\n", buffer[2]);
            break;
        case 0xFB:
        { // Playback sud package

            uint8_t effect = buffer[2];
            printf("Outrun FFB 0xFB effect: 0x%02x\n", effect);

            if (effect == 0x02)
            {
                sdlFfbRumble(1.0f, 1.0f, 120);
            }
            else if (effect == 0x10 || effect == 0x0B || effect == 0x04)
            {
                sdlFfbRumble(0.0f, 1.0f, 120);
            }
            else if (effect == 0x00 || effect == 0x1B || effect == 0x14)
            {
                sdlFfbRumble(1.0f, 0.0f, 120);
            }
            break;
        }
        default:
            break;
    }
}

void ffb_output(const unsigned char *buffer, size_t count)
{
    /*     printf("FFB output: count=%zu, data:", count);
        for (size_t i = 0; i < count; ++i)
            printf(" 0x%02x", buffer[i]);
        printf("\n"); */

    if (count > 0)
    {
        if (buffer[0] & 0x40)
        {
            // printf("GPO: ABC Vibration triggered (bit 6 set)\n");
            // sdlFfbRumble(1.0f, 1.0f, 120);
        }
    }
}