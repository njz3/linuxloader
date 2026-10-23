// The HASP HL key of Namco's N2 games whose HASP library is linked into the program (Maximum Tune 3): its functions
// are answered from a memory image kept in the game's folder (hasp.bin, through the write folder). The program finds
// the key on the USB bus (clHasp::isAvail reads /proc/bus/usb/devices), logs in, then reads its serial number (twelve
// ASCII digits at 0xD00 of the main memory, its 64 bytes ending with their sum and the sum's complement) and a 16-byte
// block of its own (the cards' ID counter first, the block at 0 or at 0x10, ending the same way), each decrypted after
// its read. The image holds them in the clear and encryption and decryption leave the data as it is; what the game
// writes back (the counter, as each new card is made) is kept. The key's clock is Windows'.

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "namcoHasp.h"
#include "../../log/log.h"
#include "../../patching/flowControl.h"
#include "../../redirections/writeDir.h"

#define HASP_STATUS_OK 0
#define HASP_MEM_RANGE 1
#define HASP_INV_FILEID 22
#define HASP_FILEID_MAIN 0xFFF0
#define HASP_SERIAL 0xD00
#define HASP_IMAGE "hasp.bin"

static uint8_t memory[4096];
static int loaded;

// A block ending with the sum of its other bytes and that sum's complement
static void seal(uint8_t *block, int length)
{
    uint8_t sum = 0;
    for (int i = 0; i < length - 2; i++)
        sum += block[i];
    block[length - 2] = sum;
    block[length - 1] = (uint8_t)~sum;
}

static void load(void)
{
    if (loaded)
        return;
    loaded = 1;
    char path[260];
    FILE *file = fopen(writeDirForRead(HASP_IMAGE, path, sizeof(path)), "rb");
    if (file)
    {
        size_t count = fread(memory, 1, sizeof(memory), file);
        fclose(file);
        if (count == sizeof(memory))
            return;
    }
    // A new key: its serial number, and the game's block at 0 with a counter of 0
    memset(memory, 0, sizeof(memory));
    memcpy(memory + HASP_SERIAL, "270620010001", 12);
    seal(memory + HASP_SERIAL, 64);
    seal(memory, 16);
    log_info("HASP: a new key image, serial 270620010001");
}

static void save(void)
{
    FILE *file = writeDirFopen(HASP_IMAGE, "wb");
    if (!file)
    {
        log_warn("HASP: %s not written", HASP_IMAGE);
        return;
    }
    fwrite(memory, 1, sizeof(memory), file);
    fclose(file);
}

static int haspIsAvail(void *hasp)
{
    (void)hasp;
    return 1;
}

static int haspLogin(uint32_t feature, const void *vendorCode, uint32_t *handle)
{
    (void)vendorCode;
    load();
    log_info("HASP: login to feature 0x%X", feature);
    if (handle)
        *handle = 1;
    return HASP_STATUS_OK;
}

static int haspLogout(uint32_t handle)
{
    (void)handle;
    return HASP_STATUS_OK;
}

static int haspRead(uint32_t handle, uint32_t file, uint32_t offset, uint32_t length, void *buffer)
{
    (void)handle;
    load();
    if (file != HASP_FILEID_MAIN)
        return HASP_INV_FILEID;
    if (offset > sizeof(memory) || length > sizeof(memory) - offset)
        return HASP_MEM_RANGE;
    memcpy(buffer, memory + offset, length);
    return HASP_STATUS_OK;
}

static int haspWrite(uint32_t handle, uint32_t file, uint32_t offset, uint32_t length, const void *buffer)
{
    (void)handle;
    load();
    if (file != HASP_FILEID_MAIN)
        return HASP_INV_FILEID;
    if (offset > sizeof(memory) || length > sizeof(memory) - offset)
        return HASP_MEM_RANGE;
    memcpy(memory + offset, buffer, length);
    save();
    return HASP_STATUS_OK;
}

// Encryption and decryption: the image is in the clear
static int haspCrypt(uint32_t handle, void *buffer, uint32_t length)
{
    (void)handle;
    (void)buffer;
    (void)length;
    return HASP_STATUS_OK;
}

// hasp_time_t: seconds since 1970, UTC
static int haspGetRtc(uint32_t handle, uint64_t *now)
{
    (void)handle;
    if (now)
        *now = (uint64_t)time(NULL);
    return HASP_STATUS_OK;
}

static int haspSetRtc(uint32_t handle, uint32_t timeLow, uint32_t timeHigh)
{
    (void)handle;
    (void)timeLow;
    (void)timeHigh;
    return HASP_STATUS_OK;
}

static void replace(const char *name, void *function)
{
    void *address = findStaticFnAddr(name);
    if (address)
        detourFunction((size_t)address, function);
}

void namcoHaspInstall(void)
{
    replace("_ZNK6clHasp7isAvailEv", haspIsAvail); // clHasp::isAvail
    replace("hasp_login", haspLogin);
    replace("hasp_logout", haspLogout);
    replace("hasp_read", haspRead);
    replace("hasp_write", haspWrite);
    replace("hasp_encrypt", haspCrypt);
    replace("hasp_decrypt", haspCrypt);
    replace("hasp_get_rtc", haspGetRtc);
    replace("hasp_legacy_set_rtc", haspSetRtc);
    log_info("HASP key emulated (%s)", HASP_IMAGE);
}
