// Namco's Dead Heat (US DRIVE rev. 6273) on its cabinet's boards, emulated where the game reads them. Maximum Heat 3D
// (Dead Heat's engine), Dead Heat Riders (its successor), Nirin (an older n2jvio, no Kickback board) and Maximum Tune 3
// (Nirin's n2jvio, a six-speed H shifter) use them too.
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
#include "../../config/config.h"
#include "../lindbergh/forceFeedback.h"
#include "../lindbergh/jvs.h"
#include "../lindbergh/netOutputs.h"
#include "../../agent/agent.h"
#include "../../log/log.h"
#include "../../patching/flowControl.h"

// The game's addresses. Maximum Heat 3D (US DRIVE rev. 8807) is Dead Heat's engine: the same functions, blocks and
// fields, elsewhere (its symbols give them). Dead Heat Riders (2013) keeps them, its calibration's fields 16 bytes
// apart. Nirin (2009) has an older n2jvio: the same functions, its state block laid out otherwise, no Kickback board
// and no gear lever
typedef struct
{
    // clSystemN2's init, exec, execSystemN2, execSystemN2Step2, execFirstStep and final, n2JvioTxPl,
    // clSerialN2::open, and clKickback::setTRQPowerType's read of the type (0: none)
    uintptr_t init, exec, execSystemN2, execStep2, execFirstStep, final, jvioTxPl, serialOpen, torqueTypeRead;
    // clSystemN2's flags and error count, n2jvio (the board's state block), clInputDeviceJamma::sm_handle_center
    // (the calibration's first field), and the clConfig and clKickback instances (0: none)
    uintptr_t initFlag, jammaFlag, jvsError, errorCount, n2jvio, calibration, config, kickback;
    // In the state block: the system byte, the player's switches, the first coin counter and the distance to the
    // next one, the analogue channels (wheel, accelerator, brake)
    uint16_t systemSwitches, playerSwitches, coinSlot, coinStride, analogue;
    // The gearbox (GEARBOX_*), the brake also as a contact on button 3 (STR_BRAKE_DIGITAL of Dead Heat and Dead Heat
    // Riders), and the brake's calibration as rest and travel (Nirin, Dead Heat Riders: rest and floored)
    uint8_t gearbox;
    bool brakeContact, brakeTravel;
    // What replaces the read of the torque type (xor eax, eax and nops of its length), the distance between the
    // calibration's fields, and n2JvioAckTxVsync when the game calls it besides clSystemN2 (0: not)
    const char *torqueTypePatch;
    uint8_t calibrationStride;
    uintptr_t ackTxVsync;
    // The Kickback instance's spring, viscosity and reflect bytes and centre offset (clKickback's setters)
    uint8_t kickbackSpring, kickbackViscosity, kickbackReflect, kickbackCentre;
    // Maximum Tune 3's calibration (clInputDeviceJamma's statics): each pedal's travel and rest, the brake's first,
    // then the wheel's half travel and centre
    bool centreWidth;
} DeadHeatAddresses;

#define GEARBOX_NONE 0
#define GEARBOX_LEVER 1 // Dead Heat's four-gear lever, one of buttons 6 to 9
#define GEARBOX_H 2     // Maximum Tune 3's six speeds: left, right, up and down on buttons 5, 6, 3 and 4

static const DeadHeatAddresses deadHeatUs = {
    0x0808BF50, 0x0808B5F0, 0x0808B8F0, 0x0808B7B0, 0x0808B790, 0x0808B5C0, 0x08092360, 0x0808C060, 0x0809FC64,
    0x0902BB04, 0x0902BB08, 0x0902BAFC, 0x0902BAE8, 0x09033340, 0x087E5ABC, 0x087E5588, 0x09035980,
    0x13C,      0x140,      0x160,      0x1C,       0x200,      GEARBOX_LEVER, true,    true,
    "31c09090", 4,          0,          0x48,       0x49,       0x4A,       0x2C,       false,
};

static const DeadHeatAddresses maximumHeat3d = {
    0x080AFC40, 0x080AEF50, 0x080AF4A0, 0x080AF240, 0x080AF220, 0x080AEF20, 0x080B4110, 0x080AFD50, 0x080BDEF7,
    0x0917083C, 0x09170840, 0x09170830, 0x0917081C, 0x09178080, 0x0891E120, 0x0891DBC8, 0x0917A6C0,
    0x13C,      0x140,      0x160,      0x1C,       0x200,      GEARBOX_LEVER, true,    true,
    "31c09090", 4,          0,          0x48,       0x49,       0x4A,       0x2C,       false,
};

// No gear lever; its main flow and boot also send the I/O board a frame of their own (n2JvioAckTxVsync, n2JvioTxPl)
static const DeadHeatAddresses deadHeatRiders = {
    0x080A5C50, 0x080A5F80, 0x080A5F60, 0x080A5D90, 0x080A5CC0, 0x080A5D40, 0x080ABCB0, 0x080A61D0, 0x080B1CE4,
    0x08F94AA0, 0x08F94AA1, 0x08F94AE8, 0x08F94AF0, 0x08F9C2A0, 0x086A9320, 0x086B2320, 0x08F9E920,
    0x13C,      0x140,      0x160,      0x1C,       0x200,      GEARBOX_NONE, true,     false,
    "31c090",   0x10,       0x080AA4B0, 0x48,       0x49,       0x4A,       0x2C,       false,
};

// Its second counter (0x178) is the service credits' (clCoin::addService)
static const DeadHeatAddresses nirin = {
    0x08093AF0, 0x08093200, 0x080931E0, 0x08093130, 0x08093250, 0x080930F0, 0x08099430, 0,          0,
    0x091A6700, 0x091A6701, 0x091A6748, 0x091A674C, 0x091ACF40, 0x083F1168, 0,          0,
    0x104,      0x108,      0x128,      0x50,       0x1A8,      GEARBOX_NONE, false,    false,
    NULL,       4,          0,          0,          0,          0,          0,          false,
};

// Maximum Tune 3 (2007): Nirin's state block, its clSystemN2 the same functions (init has no restart), its Kickback
// on its own port with its fields elsewhere, a six-speed H shifter, the calibration as centres and travels. Its
// clSerialN2 is n2CardPrinter.c's (the card reader-printer on port 2, the other ports left closed)
static const DeadHeatAddresses maximumTune3 = {
    0x0852EAB8, 0x0852EB78, 0x0852EBE4, 0x0852E08C, 0x0852DCFC, 0x0852E258, 0x08478734, 0,          0x0852F698,
    0x09B669E1, 0x09B669E0, 0x09B66A4C, 0x09B66A64, 0x09B5CEE0, 0x09AD8120, 0,          0x09B66A80,
    0x104,      0x108,      0x128,      0x50,       0x1A8,      GEARBOX_H,  false,      false,
    "31c090",   4,          0,          0x3D,       0x3E,       0x3F,       0x20,       true,
};

// Maximum Tune 3DX+ (Japan, 2008): Maximum Tune 3's boards and fields, its addresses elsewhere
static const DeadHeatAddresses maximumTune3dx = {
    0x085AE1C8, 0x085AE298, 0x085AE304, 0x085AD79C, 0x085AD408, 0x085AD964, 0x085558D0, 0,          0x085AAEFC,
    0x097FC081, 0x097FC080, 0x097FC0EC, 0x097FC104, 0x097EF580, 0x09768E40, 0,          0x097FC054,
    0x104,      0x108,      0x128,      0x50,       0x1A8,      GEARBOX_H,  false,      false,
    "31c090",   4,          0,          0x3D,       0x3E,       0x3F,       0x20,       true,
};

static const DeadHeatAddresses *game = &deadHeatUs;
extern uint32_t gId;

// clSystemN2 and its state block
#define N2_INIT_FLAG ((volatile uint8_t *)game->initFlag)
#define N2_JAMMA_FLAG ((volatile uint8_t *)game->jammaFlag)
#define N2_JVS_ERROR ((volatile uint8_t *)game->jvsError)
#define N2_ERROR_COUNT ((volatile int32_t *)game->errorCount)
#define N2_SYSTEM_SWITCHES ((volatile uint8_t *)(game->n2jvio + game->systemSwitches))
#define N2_PLAYER_SWITCHES ((volatile uint16_t *)(game->n2jvio + game->playerSwitches))
// count, two errors, condition
#define N2_COIN_SLOT(slot) ((volatile uint8_t *)(game->n2jvio + game->coinSlot + (slot) * game->coinStride))
#define N2_ANALOGUE ((volatile uint16_t *)(game->n2jvio + game->analogue)) // wheel, accelerator, brake

// clInputDeviceJamma's calibration, set by the test menu: the counts of the wheel's centre and ends, the accelerator
// at rest and floored, the brake at rest and its travel (Nirin, Dead Heat Riders: floored); fields 3 and 4 are the
// wheel's normalisation, derived from the others by the game
#define CALIBRATION(field) (*(volatile int32_t *)(game->calibration + (field) * game->calibrationStride))
#define CAL_WHEEL_CENTRE CALIBRATION(0)
#define CAL_WHEEL_LEFT CALIBRATION(1)
#define CAL_WHEEL_RIGHT CALIBRATION(2)
#define CAL_ACCEL_REST CALIBRATION(5)
#define CAL_ACCEL_FULL CALIBRATION(6)
#define CAL_BRAKE_REST CALIBRATION(7)
#define CAL_BRAKE_TRAVEL CALIBRATION(8)
#define CAL_BRAKE_FULL CALIBRATION(8)
// Maximum Tune 3's: counts = rest (or centre) + position x travel (the wheel: -1..1 of its half travel)
#define CW_BRAKE_TRAVEL CALIBRATION(0)
#define CW_BRAKE_REST CALIBRATION(1)
#define CW_ACCEL_TRAVEL CALIBRATION(2)
#define CW_ACCEL_REST CALIBRATION(3)
#define CW_WHEEL_TRAVEL CALIBRATION(4)
#define CW_WHEEL_CENTRE CALIBRATION(5)

#define CONFIG_INSTANCE (*(uint8_t *volatile *)game->config) // clConfig: byte 1 USE_JAMMA_DEVICE, 2 USE_CAMERA_DEVICE
#define KICKBACK_INSTANCE (*(uint8_t *volatile *)game->kickback)

// Gear up and gear down, the lever's four positions
#define SWITCH_GEAR_UP BUTTON_2
#define SWITCH_GEAR_DOWN BUTTON_10
#define SWITCH_GEARS (BUTTON_6 | BUTTON_7 | BUTTON_8 | BUTTON_9)
// Maximum Tune 3's: gear up and gear down on buttons 7 and 8, the H pattern's four contacts
#define SWITCH_H_GEAR_UP BUTTON_7
#define SWITCH_H_GEAR_DOWN BUTTON_8
#define SWITCH_H_LEFT BUTTON_5
#define SWITCH_H_RIGHT BUTTON_6
#define SWITCH_H_UP BUTTON_3
#define SWITCH_H_DOWN BUTTON_4

// Neutral, then first to sixth: the contacts the lever closes in each
static const int hPattern[7] = {
    0,
    SWITCH_H_LEFT | SWITCH_H_UP,
    SWITCH_H_LEFT | SWITCH_H_DOWN,
    SWITCH_H_UP,
    SWITCH_H_DOWN,
    SWITCH_H_RIGHT | SWITCH_H_UP,
    SWITCH_H_RIGHT | SWITCH_H_DOWN,
};

static int gear = 1;
static int lastSwitches = 0;

static uint16_t clampCount(float value)
{
    return (uint16_t)(value < 0.0f ? 0.0f : value > 65535.0f ? 65535.0f : value + 0.5f);
}

// A save never calibrated (zeros, from a machine without the board) gets the game's own defaults back; a calibration
// made in the test menu is kept, the wheel's left end below its centre or above it (Nirin's handlebar: 61056 at the
// left, 18112 at the right)
static void calibrationCheck(void)
{
    if (game->centreWidth)
    {
        if (CW_WHEEL_TRAVEL <= 0)
        {
            CW_WHEEL_CENTRE = 31100;
            CW_WHEEL_TRAVEL = 24576;
        }
        if (CW_ACCEL_TRAVEL <= 0)
        {
            CW_ACCEL_REST = 35000;
            CW_ACCEL_TRAVEL = 20480;
        }
        if (CW_BRAKE_TRAVEL <= 0)
        {
            CW_BRAKE_REST = 29000;
            CW_BRAKE_TRAVEL = 20480;
        }
        return;
    }
    bool leftLow = CAL_WHEEL_LEFT < CAL_WHEEL_CENTRE && CAL_WHEEL_CENTRE < CAL_WHEEL_RIGHT;
    bool leftHigh = CAL_WHEEL_LEFT > CAL_WHEEL_CENTRE && CAL_WHEEL_CENTRE > CAL_WHEEL_RIGHT;
    if (!leftLow && !leftHigh)
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
    if (game->brakeTravel && (CAL_BRAKE_TRAVEL <= 0 || CAL_BRAKE_REST <= 0))
    {
        CAL_BRAKE_REST = 29000;
        CAL_BRAKE_TRAVEL = 20480;
    }
    if (!game->brakeTravel && CAL_BRAKE_FULL <= CAL_BRAKE_REST)
    {
        CAL_BRAKE_REST = 27500;
        CAL_BRAKE_FULL = 40000;
    }
}

// The block as the board would report it now
static void n2Update(void)
{
    JVSIO *io = getJVSIO();
    int switches = io->state.inputSwitch[PLAYER_1];
    if (game->gearbox == GEARBOX_H)
    {
        int pressed = switches & ~lastSwitches;
        lastSwitches = switches;
        if ((pressed & SWITCH_H_GEAR_UP) && gear < 6)
            gear++;
        if ((pressed & SWITCH_H_GEAR_DOWN) && gear > 0)
            gear--;
        switches &= ~(SWITCH_H_GEAR_UP | SWITCH_H_GEAR_DOWN | hPattern[1] | hPattern[6]);
        switches |= hPattern[gear];
    }
    if (game->gearbox == GEARBOX_LEVER)
    {
        int pressed = switches & ~lastSwitches;
        lastSwitches = switches;
        if ((pressed & SWITCH_GEAR_UP) && gear < 4)
            gear++;
        if ((pressed & SWITCH_GEAR_DOWN) && gear > 1)
            gear--;
        switches &= ~(SWITCH_GEAR_UP | SWITCH_GEAR_DOWN | SWITCH_GEARS);
        switches |= BUTTON_6 >> (gear - 1);
    }

    float max = (float)((1 << io->capabilities.analogueInBits) - 1);
    float wheel = (float)io->state.analogueChannel[ANALOGUE_1] / max; // 0 at the left
    float accel = (float)io->state.analogueChannel[ANALOGUE_2] / max;
    float brake = (float)io->state.analogueChannel[ANALOGUE_3] / max;
    if (game->brakeContact && brake > 0.5f)
        switches |= BUTTON_3;

    calibrationCheck();
    if (game->centreWidth)
    {
        N2_ANALOGUE[0] = clampCount((float)CW_WHEEL_CENTRE + (wheel * 2.0f - 1.0f) * (float)CW_WHEEL_TRAVEL);
        N2_ANALOGUE[1] = clampCount((float)CW_ACCEL_REST + accel * (float)CW_ACCEL_TRAVEL);
        N2_ANALOGUE[2] = clampCount((float)CW_BRAKE_REST + brake * (float)CW_BRAKE_TRAVEL);
    }
    else
    {
        float centre = (float)CAL_WHEEL_CENTRE;
        float wheelCount = wheel < 0.5f ? centre - (0.5f - wheel) * 2.0f * (centre - (float)CAL_WHEEL_LEFT)
                                        : centre + (wheel - 0.5f) * 2.0f * ((float)CAL_WHEEL_RIGHT - centre);
        N2_ANALOGUE[0] = clampCount(wheelCount);
        N2_ANALOGUE[1] = clampCount((float)CAL_ACCEL_REST + accel * (float)(CAL_ACCEL_FULL - CAL_ACCEL_REST));
        float brakeTravel = game->brakeTravel ? (float)CAL_BRAKE_TRAVEL : (float)(CAL_BRAKE_FULL - CAL_BRAKE_REST);
        N2_ANALOGUE[2] = clampCount((float)CAL_BRAKE_REST + brake * brakeTravel);
    }

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
    const uint8_t *kickback = game->kickback ? KICKBACK_INSTANCE : NULL;
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
    int spring = kickback[game->kickbackSpring] & 0x7F;
    int viscosity = kickback[game->kickbackViscosity] & 0x7F;
    int reflect = (int8_t)kickback[game->kickbackReflect];
    int centre = *(const int32_t *)(kickback + game->kickbackCentre) / 4;
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
    if (game->config && CONFIG_INSTANCE)
    {
        CONFIG_INSTANCE[1] = 1;
        CONFIG_INSTANCE[2] = 0; // USE_CAMERA_DEVICE: no /dev/video0 here (Maximum Heat 3D's config.csv has it on)
    }
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

// n2JvioAckTxVsync: the board's acknowledgement of a frame, there at once
static int n2AckNow(void)
{
    return 1;
}

// clSerialN2::open: the Kickback board's port stays closed
static int serialOpenNone(void *serial)
{
    (void)serial;
    return 0;
}

void deadHeatIoInstall(void)
{
    game = gId == MAXIMUM_HEAT_3D ? &maximumHeat3d
           : gId == DEAD_HEAT_RIDERS ? &deadHeatRiders
           : gId == NIRIN            ? &nirin
           : gId == WANGAN_MIDNIGHT_MT3 ? &maximumTune3
           : gId == WANGAN_MIDNIGHT_MT3DX ? &maximumTune3dx
                                     : &deadHeatUs;
    if (game->gearbox == GEARBOX_H)
        gear = 0; // neutral, as the lever rests: in the menus, up and down would move the cursor
    detourFunction(game->init, n2Init);
    detourFunction(game->exec, n2Exec);
    detourFunction(game->execSystemN2, n2Exec);
    detourFunction(game->execStep2, n2Exec);
    detourFunction(game->execFirstStep, n2Nothing);
    detourFunction(game->final, n2Nothing);
    detourFunction(game->jvioTxPl, n2TxNothing);
    if (game->serialOpen)
        detourFunction(game->serialOpen, serialOpenNone);
    if (game->torqueTypeRead) // clKickback::setTRQPowerType: the type read as 0
        patchMemoryFromString(game->torqueTypeRead, (char *)game->torqueTypePatch);
    if (game->ackTxVsync)
        detourFunction(game->ackTxVsync, n2AckNow);
    log_info("Namco N2 I/O board emulated%s", game->kickback ? ", and the Kickback wheel board" : "");
}
