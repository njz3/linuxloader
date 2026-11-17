// Raw Thrills' Cruis'n Blast (2017, g5 engine), version 1.25: the cabinet's program as the HASP envelope decrypted it in
// memory, dumped (`game`, file CRC 0xb919dbd6). Ported from the Linux support of aderumier's linuxloader fork
// (https://github.com/aderumier/linuxloader, branch rawthrills, by SPiRiT, CC BY-NC-SA 4.0: src/loader/rawthrills), its
// addresses and its way of answering the game; written again for this loader's Windows ELF loader.
//
// - The dongle (Aladdin HASP HL): its API answered in the game (hasp_login and the others), the dongle's memory a
//   128-byte image with the cabinet type, a flag and a serial number, kept in dongle_memory.dmp once the game writes
//   it (its factory setup flag). The data comes decrypted with the dump: DongleEncrypt and DongleDecrypt do nothing.
// - The cabinet's paths: /pm is the game's folder; the scripts it would decrypt (g5/race/data/programs_enc) are taken
//   from the decrypted copy beside it (programs_dec); its user data (g5/race/pmuser) is /pm/pmuser.
// - The controls: the cabinet board's inputs, which the game reads from its engine's I/O slots, fed each frame from
//   the loader's JVS state (controls.ini): ANALOGUE_1 steers, ANALOGUE_2 is the gas pedal, ANALOGUE_3 the brake (or
//   BUTTON_1), BUTTON_2 the view, BUTTON_3 the music, UP and DOWN the volume (and the menus), START, SERVICE, TEST and
//   the two coins.
// - The force feedback: the wheel's motor (Wheel_Set, -1..1, + turning left) as the Sega servo board's torque frames of
//   the loader's force feedback; in the menus the centring they ask for, a quarter of it; in a race the game's, with
//   its effects (bumps, crashes, off road) turned on.

#include <windows.h>

#include <direct.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cruisnBlast.h"
#include "../lindbergh/forceFeedback.h"
#include "../lindbergh/jvs.h"
#include "../lindbergh/netOutputs.h"
#include "../../agent/agent.h"
#include "../../log/log.h"
#include "../../patching/flowControl.h"
#include "../../redirections/filesystemShared.h"
#include "../../redirections/writeDir.h"

static char gameDir[1024];

// --- the controls: the engine's I/O slots (io_input_digital and analog) of the cabinet board

enum
{
    CB_IO_COIN0 = 0x14c,
    CB_IO_COIN1 = 0x14d,
    CB_IO_DIAG = 0x150,
    CB_IO_SERVICE = 0x151,
    CB_IO_VOL_UP = 0x152, // also the menus' up
    CB_IO_VOL_DOWN = 0x153,
    CB_IO_START0 = 0x154,
    CB_IO_BRAKE = 0x155,
    CB_IO_VIEW = 0x156,
    CB_IO_TUNES = 0x159,
    CB_IO_WHEEL = 0x173,
    CB_IO_GAS = 0x174,
};

typedef struct // the engine's digital slot
{
    uint32_t unused;
    uint32_t pressed;  // became held this frame
    uint32_t released; // stopped being held this frame
    uint32_t heldTime;
    uint32_t pressCount;
    uint32_t releaseCount;
    uint32_t raw;
    uint32_t rawCount;
} IoDigital;

static void *(*ioGetInputAnalog)(int id);
static void *(*ioGetInputDigital)(int id);
static void (*ioInputAnalogUpdate)(void *slot, float raw);
static int (*ioLoopOrig)(int dt);
static volatile uint32_t *ioNewData;

static void ioDigitalSet(int id, int dt, int held)
{
    IoDigital *slot = ioGetInputDigital(id);
    if (!slot)
        return;
    IoDigital old = *slot;
    slot->pressed = !old.pressed && !old.heldTime && held;
    slot->released = !old.released && old.heldTime && !held;
    slot->heldTime = held ? old.heldTime + dt + slot->pressed : 0;
    slot->raw = held;
    slot->pressCount += slot->pressed;
    slot->releaseCount += slot->released;
    slot->rawCount += held >= (int)old.raw ? held - old.raw : held + (0x100 - old.raw);
}

static void ioAnalogSet(int id, int raw)
{
    void *slot = ioGetInputAnalog(id);
    if (slot)
        ioInputAnalogUpdate(slot, (float)raw);
}

// A coin, a count in the JVS state: each one a press of the coin switch held 80 ms, then as long released
typedef struct
{
    int seen, held;
    ULONGLONG until;
} CoinPulse;

static int coinPulse(CoinPulse *p, int count)
{
    ULONGLONG now = GetTickCount64();
    if (now < p->until)
        return p->held;
    if (p->held)
    {
        p->held = 0;
        p->until = now + 80;
        return 0;
    }
    if (count > p->seen)
    {
        p->seen++;
        p->held = 1;
        p->until = now + 80;
        return 1;
    }
    p->seen = count < p->seen ? count : p->seen;
    return 0;
}

static int ioLoopHook(int dt)
{
    int ret = ioLoopOrig(dt);
    JVSIO *io = getJVSIO();
    if (!io)
        return ret;
    int max = io->analogueMax > 0 ? io->analogueMax : 1023;
    int *sw = io->state.inputSwitch;
    ioAnalogSet(CB_IO_WHEEL, io->state.analogueChannel[0]);
    ioAnalogSet(CB_IO_GAS, io->state.analogueChannel[1]);
    ioDigitalSet(CB_IO_BRAKE, dt, io->state.analogueChannel[2] > max / 2 || (sw[PLAYER_1] & BUTTON_1));
    ioDigitalSet(CB_IO_VIEW, dt, (sw[PLAYER_1] & BUTTON_2) != 0);
    ioDigitalSet(CB_IO_TUNES, dt, (sw[PLAYER_1] & BUTTON_3) != 0);
    ioDigitalSet(CB_IO_START0, dt, (sw[PLAYER_1] & BUTTON_START) != 0);
    ioDigitalSet(CB_IO_SERVICE, dt, (sw[PLAYER_1] & BUTTON_SERVICE) != 0);
    ioDigitalSet(CB_IO_DIAG, dt, (sw[SYSTEM] & BUTTON_TEST) != 0);
    ioDigitalSet(CB_IO_VOL_UP, dt, (sw[PLAYER_1] & BUTTON_UP) != 0);
    ioDigitalSet(CB_IO_VOL_DOWN, dt, (sw[PLAYER_1] & BUTTON_DOWN) != 0);
    static CoinPulse coins[2];
    ioDigitalSet(CB_IO_COIN0, dt, coinPulse(&coins[0], io->state.coinCount[0]));
    ioDigitalSet(CB_IO_COIN1, dt, coinPulse(&coins[1], io->state.coinCount[1]));
    // The engine reads its slots in the frames flagged as having new data only
    if (ioNewData)
        *ioNewData = 1;
    return ret;
}

// io_new_data_present returns a flag ("mov flag, %eax", after a frame setup in some builds): that flag
static volatile uint32_t *findNewDataFlag(void)
{
    const uint8_t *code = findStaticFnAddr("io_new_data_present");
    for (int i = 0; code && i < 16; i++)
        if (code[i] == 0xa1)
            return (volatile uint32_t *)(uintptr_t) * (const uint32_t *)(code + i + 1);
    return NULL;
}

static void installControls(void)
{
    *(void **)&ioGetInputAnalog = findStaticFnAddr("io_get_input_analog");
    *(void **)&ioGetInputDigital = findStaticFnAddr("io_get_input_digital");
    *(void **)&ioInputAnalogUpdate = findStaticFnAddr("io_input_analog_update");
    ioNewData = findNewDataFlag();
    void *loop = findStaticFnAddr("io_rio_loop"); // the cabinet board's backend: after the SDL one, which clears the edges
    if (!ioGetInputAnalog || !ioGetInputDigital || !ioInputAnalogUpdate || !ioNewData || !loop)
    {
        log_error("Cruis'n Blast: the engine's input functions not found, no controls");
        return;
    }
    *(void **)&ioLoopOrig = trampolineHook(loop, (void *)ioLoopHook, 7);
    if (!ioLoopOrig)
        log_error("Cruis'n Blast: io_rio_loop not hooked, no controls");
}

// --- the force feedback: Wheel_Set's force as the servo board's torque

#define CB_WHEEL_MENU_FORCE 0x08babbac // float: the centring the menus ask for
#define CB_WHEEL_IN_RACE 0x08babbc0    // int32: 1 in a race
#define CB_WHEEL_EFFECTS 0x09c28504    // byte: the wheel effects on, which the game only clears

static void servoFrame(uint8_t command, int first, int second)
{
    uint8_t frame[4] = {command, (uint8_t)(first & 0x7F), (uint8_t)(second & 0x7F), 0};
    frame[3] = (uint8_t)((frame[0] + frame[1] + frame[2]) & 0x7F);
    netOutputsDrive(frame, 4);
    agentDriveboard(frame, 4);
    sdlFfbDriveboard(frame, 4);
}

static int wheelSet(float force)
{
    static int started = 0, inRace = 0, sent = -1000;
    if (!started)
    {
        servoFrame(0x80, 0x00, 0x01); // effects on, permanent
        servoFrame(0x81, 0x7F, 0x7F); // no cap
        servoFrame(0x83, 0x7F, 0x00); // full gain: FFB_STRENGTH and the gains scale the whole
        started = 1;
    }
    int nowInRace = *(volatile int32_t *)(uintptr_t)CB_WHEEL_IN_RACE == 1;
    if (nowInRace != inRace)
    {
        *(volatile uint8_t *)(uintptr_t)CB_WHEEL_EFFECTS = (uint8_t)nowInRace;
        inRace = nowInRace;
        force = 0.0f;
    }
    else if (!inRace)
        force = *(volatile float *)(uintptr_t)CB_WHEEL_MENU_FORCE * 0.25f; // at full strength the wheel is hard to turn
    if (force > 1.0f)
        force = 1.0f;
    if (force < -1.0f)
        force = -1.0f;
    int torque = (int)(force * 127.0f); // + to the left, as the servo board's 84
    if (torque != sent)
    {
        int raw = torque + 0x80;
        servoFrame(0x84, raw >> 7, raw);
        sent = torque;
    }
    return 0;
}

// --- the cabinet's paths

static const char *cbPath(const char *path, char *buffer, size_t size)
{
    static const char encrypted[] = "g5/race/data/programs_enc";
    const char *enc = strstr(path, encrypted);
    size_t encLength = sizeof(encrypted) - 1;
    if (enc && (enc[encLength] == '/' || enc[encLength] == '\0'))
    {
        snprintf(buffer, size, "%s/programs_dec%s", gameDir, enc + encLength);
        return buffer;
    }
    if (strncmp(path, "/pm", 3) == 0 && (path[3] == '/' || path[3] == '\0'))
    {
        const char *rest = path + 3;
        if (strncmp(rest, "/g5/race/pmuser", 15) == 0 && (rest[15] == '/' || rest[15] == '\0'))
            snprintf(buffer, size, "%s/pmuser%s", gameDir, rest + 15);
        else
            snprintf(buffer, size, "%s%s", gameDir, rest);
        return buffer;
    }
    if (strncmp(path, "/mnt/usbflash", 13) == 0)
    {
        snprintf(buffer, size, "%s/rawthrills/usbflash%s", gameDir, path + 13);
        return buffer;
    }
    if (strncmp(path, "/etc/udev", 9) == 0)
    {
        snprintf(buffer, size, "%s/rawthrills/udev%s", gameDir, path + 9);
        return buffer;
    }
    if (strcmp(path, "/proc/cpuinfo") == 0) // read through open: the one cruisnBlastInstall wrote
    {
        snprintf(buffer, size, "%s/rawthrills/cpuinfo", gameDir);
        return buffer;
    }
    return path;
}

// --- the link: the game's own (UDP 46500 between the cabinets of a channel, TCP to the host of a race), its address
// given by eth0 (filesystemShared.c)

// Its Network adjustment as the game saved it: three copies written together (networked.0 to 2.txt), each a header
// (0x14, its text's SHA-1, its length) and the text, "val = 1" when on
int cruisnBlastLinked(void)
{
    for (int i = 0; i < 3; i++)
    {
        char path[1100];
        snprintf(path, sizeof(path), "%s\\pmuser\\adj\\save\\net\\networked.%d.txt", gameDir[0] ? gameDir : ".", i);
        FILE *file = writeDirFopen(path, "rb");
        if (!file)
            continue;
        char line[128];
        int value = -1;
        while (value < 0 && fgets(line, sizeof(line), file))
            if (sscanf(line, "val = %d", &value) != 1)
                value = -1;
        fclose(file);
        if (value >= 0)
            return value == 1;
    }
    return 0;
}

// The game reads its processor's speed from /proc/cpuinfo's "cpu MHz" (and stops without it): this PC's, from the
// registry, in a cpuinfo of one processor
static void writeCpuinfo(void)
{
    DWORD mhz = 3000, size = sizeof(mhz);
    HKEY key;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", 0, KEY_READ, &key) == ERROR_SUCCESS)
    {
        RegQueryValueExA(key, "~MHz", NULL, NULL, (LPBYTE)&mhz, &size);
        RegCloseKey(key);
    }
    char path[1100];
    snprintf(path, sizeof(path), "%s\\rawthrills\\cpuinfo", gameDir);
    FILE *f = writeDirFopen(path, "wb");
    if (!f)
    {
        log_warn("Cruis'n Blast: cannot write %s", path);
        return;
    }
    fprintf(f,
            "processor\t: 0\nvendor_id\t: GenuineIntel\ncpu family\t: 6\nmodel\t\t: 60\nmodel name\t: Intel(R) Core(TM) CPU\n"
            "stepping\t: 3\ncpu MHz\t\t: %lu.000\ncache size\t: 8192 KB\nflags\t\t: fpu vme de pse tsc msr pae mce cx8 apic sep "
            "mtrr pge mca cmov pat pse36 clflush mmx fxsr sse sse2 ht pni ssse3 sse4_1 sse4_2\nbogomips\t: %lu.00\n\n",
            (unsigned long)mhz, (unsigned long)mhz * 2);
    fclose(f);
}

// --- the dongle

#define HASP_STATUS_OK 0
#define HASP_FEATURE_NOT_FOUND 1
#define HASP_MEMORY_FILE 0xfff4 // the file the game reads its dongle memory from
#define MEMORY_SIZE 128
#define DONGLE_CAB_TYPE 0x1d
#define DONGLE_FLAG 0x1e
#define DONGLE_SERIAL 0x3c
#define CAB_TYPE 4
#define CAB_SERIAL 0xc0ffee
#define SESSION_INFO "<haspid>287339817</haspid>"

static uint8_t dongleMemory[MEMORY_SIZE];
static char dongleSaved[1024];

static int haspLogin(uint32_t feature, const void *vendorCode, uint32_t *handle)
{
    (void)vendorCode;
    if (handle)
        *handle = 1;
    return feature == 0 ? HASP_STATUS_OK : HASP_FEATURE_NOT_FOUND;
}

static int haspOk(void)
{
    return HASP_STATUS_OK;
}

static int haspRead(uint32_t handle, uint32_t file, uint32_t offset, uint32_t length, void *buffer)
{
    (void)handle;
    if (file != HASP_MEMORY_FILE)
        return HASP_STATUS_OK;
    memset(buffer, 0, length);
    if (offset < MEMORY_SIZE)
        memcpy(buffer, dongleMemory + offset, length < MEMORY_SIZE - offset ? length : MEMORY_SIZE - offset);
    return HASP_STATUS_OK;
}

// Kept for the next runs: the test menu's factory setup sets its flag there, which the attract mode checks
static int haspWrite(uint32_t handle, uint32_t file, uint32_t offset, uint32_t length, const void *buffer)
{
    (void)handle;
    if (file != HASP_MEMORY_FILE || offset >= MEMORY_SIZE)
        return HASP_STATUS_OK;
    uint32_t n = length > MEMORY_SIZE - offset ? MEMORY_SIZE - offset : length;
    if (memcmp(dongleMemory + offset, buffer, n) == 0)
        return HASP_STATUS_OK;
    memcpy(dongleMemory + offset, buffer, n);
    FILE *f = writeDirFopen(dongleSaved, "wb");
    if (!f)
    {
        log_warn("Cruis'n Blast: cannot save the dongle memory in %s", dongleSaved);
        return HASP_STATUS_OK;
    }
    fwrite(dongleMemory, 1, sizeof(dongleMemory), f);
    fclose(f);
    return HASP_STATUS_OK;
}

static int haspGetSessionInfo(uint32_t handle, const char *format, char **info)
{
    (void)handle;
    (void)format;
    static char text[] = SESSION_INFO; // never freed by hasp_free below
    *info = text;
    return HASP_STATUS_OK;
}

// The dongle object's status word (+0x10) cleared: the dump's data is decrypted already
static int dongleCrypt(uint8_t *dongle)
{
    *(uint32_t *)(dongle + 0x10) = 0;
    return 0;
}

static void loadDongleMemory(void)
{
    uint32_t serial = CAB_SERIAL;
    dongleMemory[DONGLE_CAB_TYPE] = CAB_TYPE;
    dongleMemory[DONGLE_FLAG] = 1;
    memcpy(dongleMemory + DONGLE_SERIAL, &serial, sizeof(serial));
    snprintf(dongleSaved, sizeof(dongleSaved), "%s\\dongle_memory.dmp", gameDir);
    FILE *f = writeDirFopen(dongleSaved, "rb");
    if (f)
    {
        uint8_t saved[MEMORY_SIZE];
        if (fread(saved, 1, sizeof(saved), f) == sizeof(saved))
        {
            memcpy(dongleMemory, saved, sizeof(saved));
            log_info("Cruis'n Blast: dongle memory from %s", dongleSaved);
        }
        fclose(f);
    }
}

static int returnZero(void)
{
    return 0;
}

static int returnOne(void)
{
    return 1;
}

static void hook(const char *name, void *replacement)
{
    void *address = findStaticFnAddr(name);
    if (address)
        detourFunction((size_t)address, replacement);
    else
        log_warn("Cruis'n Blast: %s not found in the game", name);
}

void cruisnBlastInstall(void)
{
    if (!writeDirGameFolder(gameDir, sizeof(gameDir)) && !_getcwd(gameDir, sizeof(gameDir)))
        strcpy(gameDir, ".");
    char folder[1100];
    snprintf(folder, sizeof(folder), "%s\\rawthrills", gameDir);
    writeDirMkdir(folder);
    snprintf(folder, sizeof(folder), "%s\\rawthrills\\usbflash", gameDir);
    writeDirMkdir(folder);
    snprintf(folder, sizeof(folder), "%s\\rawthrills\\udev", gameDir);
    writeDirMkdir(folder);
    snprintf(folder, sizeof(folder), "%s\\rawthrills\\udev\\rules.d", gameDir); // its USB stick's mount rule
    writeDirMkdir(folder);
    sharedSetPathRewrite(cbPath);
    writeCpuinfo();

    loadDongleMemory();
    hook("hasp_login", haspLogin);
    hook("hasp_logout", haspOk);
    hook("hasp_read", haspRead);
    hook("hasp_write", haspWrite);
    hook("hasp_get_sessioninfo", haspGetSessionInfo);
    hook("hasp_free", haspOk);
    hook("DongleEncrypt", dongleCrypt);
    hook("DongleDecrypt", dongleCrypt);
    hook("DongleWriteLoop", haspOk);
    hook("DongleNoise", haspOk);
    hook("DongleWriteHLFeature", haspOk);
    // The cabinet's boards: the wheel's driver board on the parallel port (its motor, Wheel_Set, the force feedback's),
    // and the USB I/O board (RIO), whose thread and reports are left out: their inputs come from the loader's
    hook("Wheel_Set", (void *)wheelSet);
    installControls();
    static const char *const wheel[] = {"Wheel_Init",      "Wheel_EnablePWM",  "Wheel_DisablePWM", "Wheel_Shutdown", "Wheel_EnableWDT",
                                        "Wheel_DisableWDT", "Wheel_TwiddleWDT", "Wheel_SetData",    "Wheel_SetControl"};
    for (size_t i = 0; i < sizeof(wheel) / sizeof(wheel[0]); i++)
        hook(wheel[i], returnZero);
    hook("pmrt_usb_open", returnOne);
    static const char *const usb[] = {"pmrt_usb_interrupt_read", "pmrt_usb_interrupt_write", "pmrt_usb_control_xfr", "io_usbdev_init",
                                      "read_thread_func",        "parse_in_report",          "RIO_FileLog"};
    for (size_t i = 0; i < sizeof(usb) / sizeof(usb[0]); i++)
        hook(usb[i], returnZero);
    // The IR guns' camera manager (other Raw Thrills games' guns, through FTDI and libusb): none on this cabinet, its
    // thread ends at once
    hook("cmgr_thread_main", returnZero);
    // The dump's file-backed stand-ins of hasp_read, hasp_write and hasp_get_sessioninfo, which it calls in their place
    detourFunction(0x0804829a, haspRead);
    detourFunction(0x0804833c, haspWrite);
    detourFunction(0x0804842a, haspGetSessionInfo);
    log_info("Cruis'n Blast: dongle and paths in %s", gameDir);
}
