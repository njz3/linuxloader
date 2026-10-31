// Gaelco's PC-based games (Ring Riders, Tokyo Cop, Championship Tuning Race) on their interface board.
//
// The games reach the board through system calls Gaelco added to its kernel (int 0x80, numbers 229 to 236), each
// wrapped in a function of the game's iogaelco_* family: port reads and writes (the VGA status port for the vertical
// sync), interface commands (the controls' registers, the motor and vibrator, the sound chip's channels, the RS485
// link), the sound chip's command buffer and sample memory. Windows has no such calls (an int 0x80 there stops the
// program): the functions are replaced, found by their symbols (Tokyo Cop's in GCC 2.95's mangling).
//
// Interface command 8 reads a register of the controls, its index in params[0]: 0 and 1 the switches, active low,
// 4 to 8 the analogue channels.

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "gaelcoIo.h"
#include "gaelcoSound.h"
#include "../../config/config.h"
#include "../lindbergh/forceFeedback.h"
#include "../lindbergh/jvs.h"
#include "../lindbergh/netOutputs.h"
#include "../../log/log.h"
#include "../../agent/agent.h"
#include "../../log/log.h"
#include "../../patching/flowControl.h"
#include "../../graphics/x11Bridge.h"

extern uint32_t gId;

#define COMMAND_SOUND_MASTER_VOLUME 0 // the sound board's: params[0] a float
#define COMMAND_SOUND_BALANCE 1        // the four outputs' levels, floats
#define COMMAND_SOUND_PLAYING 2        // the playing channels' mask
#define COMMAND_SOUND_START 3          // sound_start(channel, sample, flags, four levels)
#define COMMAND_SOUND_FLUSH 4          // the commands sent (they run at once here)
#define COMMAND_SOUND_READY 13         // room for commands (Tokyo Cop's 11: until then it plays nothing)
#define COMMAND_SOUND_READY_OLD 11
#define COMMAND_COIN_COUNTER_1 6 // the coin mechanisms' counters, which the game follows for its credits
#define COMMAND_COIN_COUNTER_2 7
#define COMMAND_READ_REGISTER 8
#define COMMAND_WRITE_OUTPUT 9 // params[0] the output, params[1] its value (0 to 255)

// The commands seen, logged once each to follow what the game asks for
static bool commandSeen[256];

static void logCommand(int command, const int *params)
{
    if (command < 0 || command > 255 || commandSeen[command])
        return;
    commandSeen[command] = true;
    log_info("Gaelco interface command %d (%d, %d, %d, %d)", command, params ? params[0] : 0, params ? params[1] : 0,
             params ? params[2] : 0, params ? params[3] : 0);
}

// The registers from the loader's JVS state (controls.ini, the agent). Switches active low, but the emergency button.
// Seen in Ring Riders: register 0 bit 1 Service (its test menu), bit 2 the volume, bit 5 Start (down in the test menu),
// register 1 bit 0 up in the test menu. The test switch is on port 0x41, the coins are counters (commands 6 and 7)
typedef struct
{
    int player; // SYSTEM or PLAYER_1
    int button;
} SwitchSource;

static const SwitchSource registerSwitches[2][8] = {
    {{PLAYER_1, BUTTON_1}, {PLAYER_1, BUTTON_SERVICE}, {PLAYER_1, BUTTON_2}, {PLAYER_1, BUTTON_UP}, {PLAYER_1, BUTTON_5},
     {PLAYER_1, BUTTON_START}, {-1, 0}, {PLAYER_1, BUTTON_DOWN}},
    {{PLAYER_1, BUTTON_LEFT}, {PLAYER_1, BUTTON_RIGHT}, {PLAYER_1, BUTTON_3}, {PLAYER_1, BUTTON_4}, {PLAYER_1, BUTTON_6},
     {PLAYER_1, BUTTON_7}, {PLAYER_1, BUTTON_8}, {PLAYER_1, BUTTON_9}},
};
#define EMERGENCY_BIT 6 // register 0, active high (Ring Riders' "BOTON DE EMERGENCIA APRETADO" when set)

// Championship Tuning Race reads registers 0 and 2 (Mandos::readControls): register 0 bit 1 a button of its controls
// test, bit 2 Start, bit 3 the view (peticionCambioCamara), bit 4 gear up and bit 5 gear down (controlCar)
static const SwitchSource tuningSwitches[8] = {{-1, 0},          {PLAYER_1, BUTTON_1}, {PLAYER_1, BUTTON_START},
                                               {PLAYER_1, BUTTON_2}, {PLAYER_1, BUTTON_3}, {PLAYER_1, BUTTON_4},
                                               {-1, 0},          {-1, 0}};

// The code of one key, 0x10 + its number, taken after a few equal reads, on Championship Tuning Race's register 2 and
// Tokyo Cop's port 0x42: 0x10 the service credit, 0x14 the test menu (both games' Ramas), the others the operator's
// numeric keypad
#define KEY_SERVICE 0x10
#define KEY_TEST 0x14

static int keyCode(void)
{
    JVSIO *io = getJVSIO();
    if (io->state.inputSwitch[SYSTEM] & BUTTON_TEST)
        return KEY_TEST;
    if (io->state.inputSwitch[PLAYER_1] & BUTTON_SERVICE)
        return KEY_SERVICE;
    return 0; // no key
}

// Tokyo Cop reads ports 0x40 and 0x41 (G3Dintf_mandos::UpdateControllers, test_io): port 0x40 bit 4 the brake pedal, a
// switch, bit 5 the horn (it confirms the menus: Start); port 0x41 bit 0 the view and bit 1 the siren, the test menu's
// up and down (its test is a key's code, port 0x42), bits 3 and 4 the coin chutes' switches (the coins count by
// commands 6 and 7)
static const SwitchSource tokyoCopSwitches[2][8] = {
    {{-1, 0}, {-1, 0}, {-1, 0}, {-1, 0}, {-1, 0}, {PLAYER_1, BUTTON_START}, {-1, 0}, {-1, 0}},
    {{PLAYER_1, BUTTON_2}, {PLAYER_1, BUTTON_1}, {-1, 0}, {-1, 0}, {-1, 0}, {-1, 0}, {-1, 0}, {-1, 0}},
};
#define TOKYO_COP_BRAKE_BIT 4 // port 0x40, from the brake's analogue channel past a quarter

static int readSwitchesValue(int index)
{
    JVSIO *io = getJVSIO();
    int value = 0xFFFF;
    for (int bit = 0; bit < 8; bit++)
    {
        const SwitchSource *source = gId == GAELCO_TUNING_RACE ? &tuningSwitches[bit]
                                     : gId == TOKYO_COP        ? &tokyoCopSwitches[index][bit]
                                                               : &registerSwitches[index][bit];
        if (source->player >= 0 && (io->state.inputSwitch[source->player] & source->button))
            value &= ~(1 << bit);
    }
    if (index == 0)
        value &= ~(1 << EMERGENCY_BIT); // the emergency button, active high: released
    if (gId == TOKYO_COP && index == 0)
    {
        int bits = io->capabilities.analogueInBits ? io->capabilities.analogueInBits : 10;
        if (io->state.analogueChannel[ANALOGUE_3] > (1 << bits) / 4)
            value &= ~(1 << TOKYO_COP_BRAKE_BIT);
    }
    return value;
}

// The analogue channels 4 to 8, on 8 bits, as Ring Riders reads them: 4 the handlebar, left high on Gaelco's board
// (the three games turn that way, their menus too, and their motor's torque then pulls back to the centre), 5
// the throttle grip (255 at rest: the game takes 255 - it; its edges confirm the menus' choices), 6 the brake (a fixed
// range, 0xAA released to 0xF8, ControlMoto), 7 unused, 8 a sensor of the ride (ControlMoto, DinamicaMoto), centred.
// The game calibrates 4, 5 and 8 itself: their ends widen by one a read towards the values seen, from its saved
// ones, and a channel that never moves reads as one of its ends (the brake held, a burnout). So, as a technician
// moving the controls at power on, their first reads sweep each end in turn (300 reads at 0, then 300 at 255).
// Championship Tuning Race calibrates its wheel and both pedals (4, 5 and 6) the same way, by two a read
#define SWEEP_READS 300

static int readAnalogueValue(int index)
{
    static int sweep[9];
    JVSIO *io = getJVSIO();
    int bits = io->capabilities.analogueInBits ? io->capabilities.analogueInBits : 10;
    int shift = bits > 8 ? bits - 8 : 0;
    bool calibrated = index == 4 || index == 5 || index == (gId == GAELCO_TUNING_RACE ? 6 : 8);
    if (calibrated && sweep[index] < 2 * SWEEP_READS)
        return sweep[index]++ < SWEEP_READS ? 0 : 255;
    int steer = 255 - ((io->state.analogueChannel[ANALOGUE_1] >> shift) & 0xFF); // left high
    int gas = (io->state.analogueChannel[ANALOGUE_2] >> shift) & 0xFF;
    int brake = (io->state.analogueChannel[ANALOGUE_3] >> shift) & 0xFF;
    if (gId == TOKYO_COP || gId == GAELCO_TUNING_RACE) // a car: its wheel, accelerator and brake pedals on 4, 5 and 6
    {
        switch (index)
        {
            case 4:
                return steer;
            case 5:
                return gas;
            case 6:
                return brake;
            default:
                return 0x80;
        }
    }
    switch (index)
    {
        case 4:
            return steer;
        case 5:
            return 255 - gas;
        case 6:
            return 0xAA + brake * (0xF8 - 0xAA) / 255;
        case 8:
            return 0x80;
        default:
            return 0x80;
    }
}

// The journal of what the game reads (LL_LOG=debug and DEBUG_MSGS = true): each change of a switch register or an
// analogue channel, and every 600th read, with the loader's JVS channels 1 to 3 (wheel, accelerator, brake), so that
// a control the game sees frozen shows whether the loader's value is
static void journalRead(const char *kind, int index, int value, int *last, unsigned *count)
{
    JVSIO *io = getJVSIO();
    (*count)++;
    if (value != *last || *count % 600 == 1)
        log_debug("Gaelco %s %d = %d (0x%02X), read %u times; JVS 1/2/3 = %d/%d/%d", kind, index, value, value & 0xFF,
                  *count, io->state.analogueChannel[ANALOGUE_1], io->state.analogueChannel[ANALOGUE_2],
                  io->state.analogueChannel[ANALOGUE_3]);
    *last = value;
}

static int readSwitches(int index)
{
    static int last[2] = {-1, -1};
    static unsigned count[2];
    int value = readSwitchesValue(index);
    if (index >= 0 && index < 2)
        journalRead("switches", index, value, &last[index], &count[index]);
    return value;
}

static int readAnalogue(int index)
{
    static int last[9] = {-1, -1, -1, -1, -1, -1, -1, -1, -1};
    static unsigned count[9];
    int value = readAnalogueValue(index);
    if (index >= 0 && index < 9)
        journalRead("analogue", index, value, &last[index], &count[index]);
    return value;
}

static int readRegister(int index)
{
    if (gId == GAELCO_TUNING_RACE && index == 2)
        return keyCode();
    if (index == 0 || index == 1)
        return readSwitches(index);
    return readAnalogue(index);
}

// --- The outputs: 4 and 5 the motor's torque one way and the other (G3Dintf_ports::setMotor), 6 and 7 the vibrators
// (setVibrador). They become the Sega servo board's frames of the loader's FFB model (forceFeedback.c), as Dead Heat's
// Kickback board's (hardware/namco/deadHeatIo.c). Outputs 0 to 3 are bytes of bits (G3Dintf_ports::setOutBits80000,
// 80001 and 80002): the network outputs' RawLamps to RawLamps3. Seen in Ring Riders: on output 0 the cabinet's lamps
// on bits 0 to 3, the platform's security on bit 5; on output 1 the cabinet's lamps on bits 7, 2 and 3, the coin
// meter's pulses on bit 5, the sound's mute on bit 0. In Championship Tuning Race (IOFrontal): on output 0 the coin
// meter on bit 0, the lamps (procesaBombillas) on bits 1 and 2, the sound's mute on bit 3
#define OUTPUT_LAMP_BYTES 4
#define OUTPUT_MOTOR_A 4
#define OUTPUT_MOTOR_B 5
#define OUTPUT_VIBRATOR 6

static int outputs[16];

static void servoFrame(uint8_t command, int first, int second)
{
    uint8_t frame[4] = {command, (uint8_t)(first & 0x7F), (uint8_t)(second & 0x7F), 0};
    frame[3] = (uint8_t)((frame[0] + frame[1] + frame[2]) & 0x7F);
    netOutputsDrive(frame, 4);
    agentDriveboard(frame, 4);
    sdlFfbDriveboard(frame, 4);
}

static void feedbackUpdate(void)
{
    static bool started = false;
    static int sentTorque = -1000, sentVibration = -1;
    if (!started)
    {
        servoFrame(0x80, 0x00, 0x01); // effects on, permanent
        servoFrame(0x81, 0x7F, 0x7F); // no cap
        servoFrame(0x83, 0x7F, 0x00); // full gain: FFB_STRENGTH scales the whole
        started = true;
    }
    int torque = (outputs[OUTPUT_MOTOR_B] - outputs[OUTPUT_MOTOR_A]) / 2; // -127..127
    if (torque != sentTorque)
    {
        int raw = torque + 0x80; // the servo board's centred value (84), on 14 bits
        servoFrame(0x84, raw >> 7, raw);
        sentTorque = torque;
    }
    int vibration = outputs[OUTPUT_VIBRATOR] / 4; // 0..63
    if (vibration != sentVibration)
    {
        servoFrame(0x85, 0x10, vibration);
        sentVibration = vibration;
    }
}

static int soundCommand(int command, int *params)
{
    float levels[4];
    switch (command)
    {
        case COMMAND_SOUND_MASTER_VOLUME:
            memcpy(levels, params, sizeof(float));
            gaelcoSoundMasterVolume(levels[0]);
            return 0;
        case COMMAND_SOUND_BALANCE:
            memcpy(levels, params, sizeof levels);
            gaelcoSoundBalance(levels);
            return 0;
        case COMMAND_SOUND_PLAYING:
            return gaelcoSoundChannelsPlaying();
        case COMMAND_SOUND_START:
            gaelcoSoundStart(params);
            return 0;
        default:
            return 0;
    }
}

static int gaelcoInport(int port, char *value);

static int gaelcoIntfCommand(int command, int *params)
{
    logCommand(command, params);
    if (command <= COMMAND_SOUND_FLUSH && params)
        return soundCommand(command, params);
    if (command == COMMAND_SOUND_READY || (command == COMMAND_SOUND_READY_OLD && gId == TOKYO_COP))
        return 1;
    // Tokyo Cop's older interface reads and writes ports with commands 8 and 9 (params[0] the port): its link's slave
    // waits for the vertical retrace on 0x3DA (wait_vsync_raw) and moves its own on the CRTC's 0x3D4 (setScanLines),
    // so that its frames follow the master's. Writes ignored
    if ((command == COMMAND_READ_REGISTER || command == COMMAND_WRITE_OUTPUT) && gId == TOKYO_COP && params &&
        params[0] >= 0x100)
    {
        char value = 0;
        if (command == COMMAND_READ_REGISTER)
            gaelcoInport(params[0], &value);
        return (unsigned char)value;
    }
    if (command == COMMAND_READ_REGISTER && params)
        return readRegister(params[0]);
    if (command == COMMAND_WRITE_OUTPUT && params && params[0] >= 0 && params[0] < 16)
    {
        outputs[params[0]] = params[1];
        if (params[0] >= OUTPUT_MOTOR_A && params[0] <= OUTPUT_VIBRATOR)
            feedbackUpdate();
        if (params[0] < OUTPUT_LAMP_BYTES)
            netOutputsSetGpo(params[0], (uint8_t)params[1]);
        return 0;
    }
    if (command == COMMAND_COIN_COUNTER_1 || command == COMMAND_COIN_COUNTER_2)
        return getJVSIO()->state.coinCount[command - COMMAND_COIN_COUNTER_1] & 0xFF; // cumulative, on 8 bits
    return 0;
}

// The ports read: the VGA status register (0x3DA), its vertical retrace bit (3) toggling at each read so that the
// game's waits for the retrace end (the frame rate comes from the loader's swap); port 0x41, the test switch on its
// bit 1, active low (G3Dintf_mandos::readTest). Tokyo Cop's older interface reads its controls on ports too: 0x40 and
// 0x41 the switches (registers 0 and 1), 0x42 a key's code, 0x44 to 0x47 the analogue channels 4 to 7
#define PORT_SWITCHES_0 0x40
#define PORT_TEST 0x41
#define PORT_TEST_BIT 0x02
#define PORT_KEY 0x42
#define PORT_ANALOGUE_4 0x44

static int gaelcoInport(int port, char *value)
{
    static uint8_t status = 0;
    if (value)
    {
        if (port == 0x3DA)
        {
            status ^= 0x08;
            *value = (char)status;
        }
        else if (port == PORT_SWITCHES_0)
            *value = (char)readSwitches(0);
        else if (port == PORT_TEST && gId == TOKYO_COP) // its siren there
            *value = (char)readSwitches(1);
        else if (port == PORT_TEST)
        {
            int switches = readSwitches(1) | PORT_TEST_BIT;
            if (getJVSIO()->state.inputSwitch[SYSTEM] & BUTTON_TEST)
                switches &= ~PORT_TEST_BIT;
            *value = (char)switches;
        }
        else if (port >= PORT_ANALOGUE_4 && port <= PORT_ANALOGUE_4 + 3)
            *value = (char)readAnalogue(4 + port - PORT_ANALOGUE_4);
        else if (port == PORT_KEY)
            *value = (char)(gId == TOKYO_COP ? keyCode() : 0xFF); // Ring Riders reprograms its settings on 0
        else
            *value = 0;
    }
    return 0;
}

// Tokyo Cop's older interface writes its motor's torque on ports 0x44 and 0x45, the outputs 4 and 5 of the newer, and
// bits on ports 0x40 and 0x41 (G3Dintf_ports::set_out_bits_40 and _41), the newer's outputs 0 and 1 and as them the
// network outputs' RawLamps and RawLamps1: on 0x41 the cabinet's lamps (enableCabinetLight) on bits 7 (the siren
// button's, luzBocina), 2 and 3, and the coin meter's pulses on bit 5; on 0x40 the platform (platform_enable and
// platform_move) on bits 4 to 7
#define PORT_OUT_BITS_40 0x40
#define PORT_OUT_BITS_41 0x41

static int gaelcoOutport(int port, char *value)
{
    if (value && (port == 0x44 || port == 0x45))
    {
        outputs[port == 0x44 ? OUTPUT_MOTOR_A : OUTPUT_MOTOR_B] = (uint8_t)*value;
        feedbackUpdate();
    }
    if (value && (port == PORT_OUT_BITS_40 || port == PORT_OUT_BITS_41))
        netOutputsSetGpo(port - PORT_OUT_BITS_40, (uint8_t)*value);
    return 0;
}

// The sound board's commands (gaelcoSound.c): the command, the channel, two words
static int gaelcoAddComm(int command, int channel, int first, int second)
{
    gaelcoSoundCommand(command, channel, first, second);
    return 0;
}

// The sample bank (roms.new), sent once at start
static int gaelcoDumpSoundBuffer(int *buffer, int length)
{
    gaelcoSoundLoad(buffer, length);
    return 0;
}

static int gaelcoStopSound(int value)
{
    (void)value;
    gaelcoSoundStopAll();
    return 0;
}

static int gaelcoSoundNothing(int channel)
{
    (void)channel;
    return 0;
}

// Tokyo Cop's iolinux methods make the system calls themselves: the same, with the object first
static int methodInport(void *self, int port, char *value)
{
    (void)self;
    return gaelcoInport(port, value);
}

static int methodOutport(void *self, int port, char *value)
{
    (void)self;
    return gaelcoOutport(port, value);
}

static int methodIntfCommand(void *self, int command, int *params)
{
    (void)self;
    return gaelcoIntfCommand(command, params);
}

static int methodAddComm(void *self, int a, int b, int c, int d)
{
    (void)self;
    return gaelcoAddComm(a, b, c, d);
}

static int methodDumpSoundBuffer(void *self, int *buffer, int length)
{
    (void)self;
    return gaelcoDumpSoundBuffer(buffer, length);
}

static int methodStopSound(void *self)
{
    (void)self;
    return gaelcoStopSound(0);
}

static int methodSoundNothing(void *self, int value)
{
    (void)self;
    (void)value;
    return 0;
}

static void replaceIfThere(const char *name, void *replacement)
{
    void *address = findStaticFnAddr(name);
    if (address)
        detourFunction((size_t)address, replacement);
}

// --- Championship Tuning Race links the XF86VidMode client library statically: its functions speak the X protocol
// (_XSend, _XReply...), which there is no server for. They are replaced by the loader's X11 bridge (x11Bridge.c) for
// the modes and the viewport, and answered here for the gamma: a ramp of 256, the identity
static int vidModeGammaRampSize(void *dpy, int screen, int *size)
{
    (void)dpy;
    (void)screen;
    if (size)
        *size = 256;
    return 1;
}

static int vidModeGetGammaRamp(void *dpy, int screen, int size, unsigned short *red, unsigned short *green,
                               unsigned short *blue)
{
    (void)dpy;
    (void)screen;
    for (int i = 0; i < size; i++)
    {
        unsigned short level = (unsigned short)(size > 1 ? i * 65535 / (size - 1) : 0);
        if (red)
            red[i] = level;
        if (green)
            green[i] = level;
        if (blue)
            blue[i] = level;
    }
    return 1;
}

static int vidModeGetGamma(void *dpy, int screen, float *gamma)
{
    (void)dpy;
    (void)screen;
    if (gamma)
        gamma[0] = gamma[1] = gamma[2] = 1.0f;
    return 1;
}

static int vidModeTrue(void)
{
    return 1;
}

static void replaceVidMode(void)
{
    replaceIfThere("XF86VidModeQueryExtension", bridgeXF86VidModeQueryExtension);
    replaceIfThere("XF86VidModeQueryVersion", bridgeXF86VidModeQueryVersion);
    replaceIfThere("XF86VidModeGetAllModeLines", bridgeXF86VidModeGetAllModeLines);
    replaceIfThere("XF86VidModeGetModeLine", bridgeXF86VidModeGetModeLine);
    replaceIfThere("XF86VidModeSwitchToMode", bridgeXF86VidModeSwitchToMode);
    replaceIfThere("XF86VidModeSetViewPort", bridgeXF86VidModeSetViewPort);
    replaceIfThere("XF86VidModeGetViewPort", bridgeXF86VidModeGetViewPort);
    replaceIfThere("XF86VidModeLockModeSwitch", bridgeXF86VidModeLockModeSwitch);
    replaceIfThere("XF86VidModeGetGammaRampSize", vidModeGammaRampSize);
    replaceIfThere("XF86VidModeGetGammaRamp", vidModeGetGammaRamp);
    replaceIfThere("XF86VidModeGetGamma", vidModeGetGamma);
    replaceIfThere("XF86VidModeSetGammaRamp", vidModeTrue);
    replaceIfThere("XF86VidModeSetGamma", vidModeTrue);
    replaceIfThere("XF86VidModeSetClientVersion", vidModeTrue);
    replaceIfThere("XF86VidModeSwitchMode", vidModeTrue);
}


// A function by either of its names (GCC 3's mangling, then GCC 2.95's)
static void replace(const char *modern, const char *old, void *replacement)
{
    void *address = findStaticFnAddr(modern);
    if (!address)
        address = findStaticFnAddr(old);
    if (address)
        detourFunction((size_t)address, replacement);
    else
        log_warn("Gaelco: %s not found", modern);
}

void gaelcoIoInstall(void)
{
    replace("_Z15iogaelco_inportiPc", "iogaelco_inport__FiPc", gaelcoInport);
    replace("_Z16iogaelco_outportiPc", "iogaelco_outport__FiPc", gaelcoOutport);
    replace("_Z21iogaelco_intf_commandiPi", "iogaelco_intf_command__FiPi", gaelcoIntfCommand);
    replace("_Z17iogaelco_add_commiiii", "iogaelco_add_comm__Fiiii", gaelcoAddComm);
    replace("_Z26iogaelco_dump_sound_bufferPii", "iogaelco_dump_sound_buffer__FPii", gaelcoDumpSoundBuffer);
    replace("_Z20iogaelco_start_soundi", "iogaelco_start_sound__Fi", gaelcoSoundNothing);
    replace("_Z19iogaelco_stop_soundi", "iogaelco_stop_sound__Fi", gaelcoStopSound);
    replace("_Z17iogaelco_set_mutei", "iogaelco_set_mute__Fi", gaelcoSoundNothing);
    // GCC 2.95's iolinux methods (Tokyo Cop), and its four dummy calls
    replaceIfThere("inport__7iolinuxiPc", methodInport);
    replaceIfThere("outport__7iolinuxiPc", methodOutport);
    replaceIfThere("intf_command__7iolinuxiPi", methodIntfCommand);
    replaceIfThere("add_comm__7iolinuxiiii", methodAddComm);
    replaceIfThere("dump_sound_buffer__7iolinuxPii", methodDumpSoundBuffer);
    replaceIfThere("start_sound__7iolinux", methodSoundNothing);
    replaceIfThere("stop_sound__7iolinux", methodStopSound);
    replaceIfThere("set_mute__7iolinuxi", methodSoundNothing);
    replaceIfThere("gaelco_dummy0__Fi", gaelcoSoundNothing);
    replaceIfThere("gaelco_dummy1__Fi", gaelcoSoundNothing);
    replaceIfThere("gaelco_dummy2__Fi", gaelcoSoundNothing);
    replaceIfThere("gaelco_dummy3__Fi", gaelcoSoundNothing);
    replaceVidMode();
    log_info("Gaelco: interface board emulated");
}
