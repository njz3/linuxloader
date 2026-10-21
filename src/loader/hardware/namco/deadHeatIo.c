// Namco's Dead Heat (US DRIVE rev. 6273) on its cabinet's boards, emulated where the game reads them.
//
// The N2 I/O board: the game talks to it over a serial port (Namco's n2Jvio library) and keeps what it reports in a
// state block, clSystemN2's, that its input device and coin counter read: the switches in the JVS layout (the loader's
// own: Start 0x8000, Service 0x4000, Up, Down, buttons 1 to 10, Test in the system byte), the analogue channels of
// the wheel and the pedals, and cumulative coin counters. The serial link is skipped: clSystemN2's start and per
// frame service are replaced, and the block is written from the loader's JVS state (controls.ini, the agent). This
// is the cabinet's way in, with analogue pedals and coins; the game's other way, an SDL joystick, has digital pedals
// and no coin, and is left without a joystick (sdl12Bridge.c).
//
// The board's switches as the game uses them: Service 0x4000 (test menu: back), Up 0x2000 and Down 0x1000 (test
// menu), button 1 0x0200 (test menu: enter), button 3 0x0080 (the brake when STR_BRAKE_DIGITAL is TRUE), button 4
// 0x0040 (nitro), button 5 0x0020 (view), buttons 6 to 9 0x0010..0x0002 (the four gears of the lever). In its menus
// the accelerator decides and the wheel moves. Gear up and gear down (buttons 2 and 10 here) move the lever one gear.
//
// The Kickback wheel board: on its own serial port, left closed, and set to torque type 0 (no motor), for which its
// functions skip their exchanges (the steering check at boot passes, as with a board that answers). The game sets
// its spring, viscosity, reflect (a torque) and centre offset each frame of a race (clGameInput::update); they become
// the Sega servo board's frames of the loader's FFB model (forceFeedback.c), so the wheel, the agent's journal and the
// network outputs follow them as for the Lindbergh games.

#include <stdbool.h>
#include <stdint.h>

#include "deadHeatIo.h"
#include "../lindbergh/forceFeedback.h"
#include "../lindbergh/jvs.h"
#include "../lindbergh/netOutputs.h"
#include "../../agent/agent.h"
#include "../../log/log.h"
#include "../../patching/flowControl.h"

// clSystemN2 and its state block
#define N2_INIT_FLAG ((volatile uint8_t *)0x0902BB04)
#define N2_JAMMA_FLAG ((volatile uint8_t *)0x0902BB08)
#define N2_JVS_ERROR ((volatile uint8_t *)0x0902BAFC)
#define N2_ERROR_COUNT ((volatile int32_t *)0x0902BAE8)
#define N2_SYSTEM_SWITCHES ((volatile uint8_t *)0x0903347C)
#define N2_PLAYER_SWITCHES ((volatile uint16_t *)0x09033480)
#define N2_COIN_SLOT(slot) ((volatile uint8_t *)(0x090334A0 + (slot) * 0x1C)) // count, two errors, condition
#define N2_ANALOGUE ((volatile uint16_t *)0x09033540)                      // wheel, accelerator, brake

// clInputDeviceJamma's calibration, set by the test menu: the counts of the wheel's centre and ends, the accelerator
// at rest and floored, the brake at rest and its travel
#define CAL_WHEEL_CENTRE (*(volatile int32_t *)0x087E5ABC)
#define CAL_WHEEL_LEFT (*(volatile int32_t *)0x087E5AC0)
#define CAL_WHEEL_RIGHT (*(volatile int32_t *)0x087E5AC4)
#define CAL_ACCEL_REST (*(volatile int32_t *)0x087E5AD0)
#define CAL_ACCEL_FULL (*(volatile int32_t *)0x087E5AD4)
#define CAL_BRAKE_REST (*(volatile int32_t *)0x087E5AD8)
#define CAL_BRAKE_TRAVEL (*(volatile int32_t *)0x087E5ADC)

#define CONFIG_INSTANCE (*(uint8_t *volatile *)0x087E5588) // clConfig: byte 1 is USE_JAMMA_DEVICE
#define KICKBACK_INSTANCE (*(uint8_t *volatile *)0x09035980)

// Gear up and gear down, the lever's four positions
#define SWITCH_GEAR_UP BUTTON_2
#define SWITCH_GEAR_DOWN BUTTON_10
#define SWITCH_GEARS (BUTTON_6 | BUTTON_7 | BUTTON_8 | BUTTON_9)

static int gear = 1;
static int lastSwitches = 0;

static uint16_t clampCount(float value)
{
    return (uint16_t)(value < 0.0f ? 0.0f : value > 65535.0f ? 65535.0f : value + 0.5f);
}

// A save never calibrated (zeros, from a machine without the board) gets the game's own defaults back; a calibration
// made in the test menu is kept
static void calibrationCheck(void)
{
    if (CAL_WHEEL_LEFT >= CAL_WHEEL_CENTRE || CAL_WHEEL_RIGHT <= CAL_WHEEL_CENTRE)
    {
        CAL_WHEEL_CENTRE = 28600;
        CAL_WHEEL_LEFT = 4000;
        CAL_WHEEL_RIGHT = 55000;
    }
    if (CAL_ACCEL_FULL <= CAL_ACCEL_REST)
    {
        CAL_ACCEL_REST = 5900;
        CAL_ACCEL_FULL = 60000;
    }
    if (CAL_BRAKE_TRAVEL <= 0 || CAL_BRAKE_REST <= 0)
    {
        CAL_BRAKE_REST = 29000;
        CAL_BRAKE_TRAVEL = 20480;
    }
}

// The block as the board would report it now
static void n2Update(void)
{
    JVSIO *io = getJVSIO();
    int switches = io->state.inputSwitch[PLAYER_1];
    int pressed = switches & ~lastSwitches;
    lastSwitches = switches;
    if ((pressed & SWITCH_GEAR_UP) && gear < 4)
        gear++;
    if ((pressed & SWITCH_GEAR_DOWN) && gear > 1)
        gear--;
    switches &= ~(SWITCH_GEAR_UP | SWITCH_GEAR_DOWN | SWITCH_GEARS);
    switches |= BUTTON_6 >> (gear - 1);

    float max = (float)((1 << io->capabilities.analogueInBits) - 1);
    float wheel = (float)io->state.analogueChannel[ANALOGUE_1] / max; // 0 at the left
    float accel = (float)io->state.analogueChannel[ANALOGUE_2] / max;
    float brake = (float)io->state.analogueChannel[ANALOGUE_3] / max;
    if (brake > 0.5f)
        switches |= BUTTON_3;

    calibrationCheck();
    float centre = (float)CAL_WHEEL_CENTRE;
    float wheelCount = wheel < 0.5f ? centre - (0.5f - wheel) * 2.0f * (centre - (float)CAL_WHEEL_LEFT)
                                    : centre + (wheel - 0.5f) * 2.0f * ((float)CAL_WHEEL_RIGHT - centre);
    N2_ANALOGUE[0] = clampCount(wheelCount);
    N2_ANALOGUE[1] = clampCount((float)CAL_ACCEL_REST + accel * (float)(CAL_ACCEL_FULL - CAL_ACCEL_REST));
    N2_ANALOGUE[2] = clampCount((float)CAL_BRAKE_REST + brake * (float)CAL_BRAKE_TRAVEL);

    *N2_PLAYER_SWITCHES = (uint16_t)switches;
    *N2_SYSTEM_SWITCHES = (uint8_t)(io->state.inputSwitch[SYSTEM] & 0xFF);
    for (int slot = 0; slot < 2; slot++)
    {
        volatile uint8_t *coins = N2_COIN_SLOT(slot);
        *(volatile uint16_t *)coins = (uint16_t)(io->state.coinCount[slot] & 0x3FFF);
        *(volatile uint16_t *)(coins + 2) = 0; // no jam
        *(volatile uint16_t *)(coins + 4) = 0;
        coins[6] = 0; // the mechanism fine
    }
}

// --- the Kickback board as the servo board's frames

typedef struct
{
    bool started;
    int spring, viscosity, reflect, centre;
} Kickback;

static Kickback sent;

// A frame of the Sega servo board: command, two 7-bit bytes, checksum
static void servoFrame(uint8_t command, int first, int second)
{
    uint8_t frame[4] = {command, (uint8_t)(first & 0x7F), (uint8_t)(second & 0x7F), 0};
    frame[3] = (uint8_t)((frame[0] + frame[1] + frame[2]) & 0x7F);
    netOutputsDrive(frame, 4);
    agentDriveboard(frame, 4);
    sdlFfbDriveboard(frame, 4);
}

// A centred value of the servo board (84 torque, 87 spring centre): 0x80 is 0, on 14 bits
static void servoCentred(uint8_t command, int value)
{
    int raw = value + 0x80;
    servoFrame(command, raw >> 7, raw);
}

static void kickbackUpdate(void)
{
    const uint8_t *kickback = KICKBACK_INSTANCE;
    if (!kickback)
        return;
    if (!sent.started)
    {
        servoFrame(0x80, 0x00, 0x01); // effects on, permanent
        servoFrame(0x81, 0x7F, 0x7F); // no cap
        servoFrame(0x83, 0x7F, 0x00); // full gain: FFB_STRENGTH scales the whole
        servoFrame(0x88, 0x00, 0x00); // spring on
        sent.started = true;
        sent.spring = sent.viscosity = sent.reflect = sent.centre = -1000;
    }
    // Scales of clGameInput::update: spring, viscosity 0..127, reflect -127..127, centre offset -511..511
    int spring = kickback[0x48] & 0x7F;
    int viscosity = kickback[0x49] & 0x7F;
    int reflect = (int8_t)kickback[0x4A];
    int centre = *(const int32_t *)(kickback + 0x2C) / 4;
    centre = centre < -127 ? -127 : centre > 127 ? 127 : centre;
    if (spring != sent.spring)
        servoFrame(0x8B, spring, 0x00);
    if (viscosity != sent.viscosity)
        servoFrame(0x86, 0x10, viscosity);
    if (reflect != sent.reflect)
        servoCentred(0x84, reflect);
    if (centre != sent.centre)
        servoCentred(0x87, centre);
    sent.spring = spring;
    sent.viscosity = viscosity;
    sent.reflect = reflect;
    sent.centre = centre;
}

// --- clSystemN2, replaced

// init(bool jamma, bool restart): the board found, whatever config.csv's USE_JAMMA_DEVICE says (the steering check
// at boot and the Kickback board follow that flag)
static int n2Init(int jamma, int restart)
{
    (void)jamma;
    (void)restart;
    *N2_JAMMA_FLAG = 1;
    *N2_JVS_ERROR = 0;
    *N2_ERROR_COUNT = 0;
    if (CONFIG_INSTANCE)
        CONFIG_INSTANCE[1] = 1;
    n2Update();
    *N2_INIT_FLAG = 1;
    return 1;
}

// exec, execSystemN2, execSystemN2Step2: the board's service each frame
static int n2Exec(void)
{
    n2Update();
    kickbackUpdate();
    return 1;
}

// execFirstStep, final: nothing to send or close
static int n2Nothing(void)
{
    return 1;
}

// n2JvioTxPl: a command to the board's outputs (the Kickback's setupGOUT), not sent
static int n2TxNothing(void)
{
    return 0;
}

// clSerialN2::open: the Kickback board's port stays closed
static int serialOpenNone(void *serial)
{
    (void)serial;
    return 0;
}

void deadHeatIoInstall(void)
{
    detourFunction(0x0808BF50, n2Init);
    detourFunction(0x0808B5F0, n2Exec);      // clSystemN2::exec
    detourFunction(0x0808B8F0, n2Exec);      // clSystemN2::execSystemN2
    detourFunction(0x0808B7B0, n2Exec);      // clSystemN2::execSystemN2Step2
    detourFunction(0x0808B790, n2Nothing);   // clSystemN2::execFirstStep
    detourFunction(0x0808B5C0, n2Nothing);   // clSystemN2::final
    detourFunction(0x08092360, n2TxNothing); // n2JvioTxPl
    detourFunction(0x0808C060, serialOpenNone);
    patchMemoryFromString(0x0809FC64, "31c09090"); // clKickback::setTRQPowerType: the type read as 0
    log_info("Dead Heat: N2 I/O and Kickback boards emulated");
}
