// The libraries Raw Thrills' Cruis'n Blast links that no Lindbergh game did:
// - udev, through which it finds its I/O boards (the RIO2 and the wheel's USB H-Bridge, FTDI serial adapters): for
//   now a system without any device, the calls logged;
// - Theora and Ogg, for its videos: streams that never yield a page, so the game finds no video to play;
// - PhysX's cooking library: none, as on a cabinet without PhysX installed.
#if defined(_WIN32) || defined(__MINGW32__)

#include <stdint.h>
#include <string.h>
#include <vector>

#include "rawThrillsBridge.hpp"
#include "symbolResolver.hpp"
#include "../log/log.h"

#define MAP(name, func) SymbolResolver::GetInstance().RegisterVTable(name, reinterpret_cast<void *>(func))

// --- PhysX

static void *bridgeNxGetCookingLib(uint32_t sdkVersion)
{
    log_warn("PhysX: no cooking library (SDK version 0x%08X asked)", sdkVersion);
    return nullptr;
}

// --- udev: one context, one enumeration and one monitor object, all empty

struct UdevObject
{
    int refs;
};
static UdevObject s_udev = {1}, s_enumerate = {1}, s_monitor = {1};

static void *bridgeUdevNew()
{
    return &s_udev;
}

static void *bridgeUdevUnref(void *)
{
    return nullptr;
}

static void *bridgeUdevEnumerateNew(void *)
{
    return &s_enumerate;
}

static int bridgeUdevEnumerateAddMatchSubsystem(void *, const char *subsystem)
{
    log_warn("udev: the game looks for %s devices: none", subsystem ? subsystem : "(null)");
    return 0;
}

static int bridgeUdevZero(void *)
{
    return 0;
}

static void *bridgeUdevNull(void *)
{
    return nullptr;
}

static void *bridgeUdevNull2(void *, const void *)
{
    return nullptr;
}

static void *bridgeUdevDeviceNewFromDevnum(void *, char type, uint64_t devnum)
{
    (void)type;
    (void)devnum;
    return nullptr;
}

static void *bridgeUdevDeviceGetParentWithSubsystemDevtype(void *, const char *, const char *)
{
    return nullptr;
}

static void *bridgeUdevMonitorNewFromNetlink(void *, const char *name)
{
    log_info("udev: monitor on %s", name ? name : "(null)");
    return &s_monitor;
}

static int bridgeUdevMonitorFilterAddMatchSubsystemDevtype(void *, const char *subsystem, const char *devtype)
{
    log_info("udev: monitor for %s %s", subsystem ? subsystem : "(null)", devtype ? devtype : "");
    return 0;
}

static int bridgeUdevMonitorGetFd(void *)
{
    return -1;
}

// --- Ogg and Theora: the sync layer takes the file's bytes and gives no page back

static std::vector<char> s_oggBuffer;

static int bridgeOggZero(void *)
{
    return 0;
}

static int bridgeOggZero2(void *, void *)
{
    return 0;
}

static int bridgeOggStreamInit(void *, int)
{
    return 0;
}

static char *bridgeOggSyncBuffer(void *, long size)
{
    if (size < 0)
        return nullptr;
    if ((size_t)size > s_oggBuffer.size())
        s_oggBuffer.resize((size_t)size);
    return s_oggBuffer.data();
}

static int bridgeOggSyncWrote(void *, long)
{
    return 0;
}

static int bridgeOggMinusOne()
{
    return -1;
}

static void bridgeTheoraNothing(void *)
{
}

static int bridgeTheoraBadHeader()
{
    log_info("Theora: no video decoder");
    return -20; // OC_BADHEADER
}

static int64_t bridgeTheoraGranuleFrame(void *, int64_t)
{
    return -1;
}

static double bridgeTheoraGranuleTime(void *, int64_t)
{
    return -1.0;
}

static char *bridgeTheoraCommentQuery(void *, char *, int)
{
    return nullptr;
}

namespace RawThrillsBridge
{
    void initBridges()
    {
        MAP("NxGetCookingLib", bridgeNxGetCookingLib);

        MAP("udev_new", bridgeUdevNew);
        MAP("udev_unref", bridgeUdevUnref);
        MAP("udev_enumerate_new", bridgeUdevEnumerateNew);
        MAP("udev_enumerate_add_match_subsystem", bridgeUdevEnumerateAddMatchSubsystem);
        MAP("udev_enumerate_scan_devices", bridgeUdevZero);
        MAP("udev_enumerate_get_list_entry", bridgeUdevNull);
        MAP("udev_enumerate_unref", bridgeUdevNull);
        MAP("udev_list_entry_get_name", bridgeUdevNull);
        MAP("udev_list_entry_get_next", bridgeUdevNull);
        MAP("udev_device_new_from_syspath", bridgeUdevNull2);
        MAP("udev_device_new_from_devnum", bridgeUdevDeviceNewFromDevnum);
        MAP("udev_device_get_parent_with_subsystem_devtype", bridgeUdevDeviceGetParentWithSubsystemDevtype);
        MAP("udev_device_get_sysattr_value", bridgeUdevNull2);
        MAP("udev_device_get_devnode", bridgeUdevNull);
        MAP("udev_device_get_sysname", bridgeUdevNull);
        MAP("udev_device_get_action", bridgeUdevNull);
        MAP("udev_device_unref", bridgeUdevNull);
        MAP("udev_monitor_new_from_netlink", bridgeUdevMonitorNewFromNetlink);
        MAP("udev_monitor_filter_add_match_subsystem_devtype", bridgeUdevMonitorFilterAddMatchSubsystemDevtype);
        MAP("udev_monitor_enable_receiving", bridgeUdevZero);
        MAP("udev_monitor_get_fd", bridgeUdevMonitorGetFd);
        MAP("udev_monitor_receive_device", bridgeUdevNull);
        MAP("udev_monitor_unref", bridgeUdevNull);

        MAP("ogg_sync_init", bridgeOggZero);
        MAP("ogg_sync_clear", bridgeOggZero);
        MAP("ogg_sync_buffer", bridgeOggSyncBuffer);
        MAP("ogg_sync_wrote", bridgeOggSyncWrote);
        MAP("ogg_sync_pageout", bridgeOggZero2); // no page: more data wanted, until the file ends
        MAP("ogg_page_bos", bridgeOggZero);
        MAP("ogg_page_serialno", bridgeOggZero);
        MAP("ogg_stream_init", bridgeOggStreamInit);
        MAP("ogg_stream_clear", bridgeOggZero);
        MAP("ogg_stream_pagein", bridgeOggMinusOne);
        MAP("ogg_stream_packetin", bridgeOggMinusOne);
        MAP("ogg_stream_packetout", bridgeOggZero2);
        MAP("ogg_stream_pageout", bridgeOggZero2);
        MAP("ogg_stream_flush", bridgeOggZero2);

        MAP("theora_info_init", bridgeTheoraNothing);
        MAP("theora_info_clear", bridgeTheoraNothing);
        MAP("theora_comment_init", bridgeTheoraNothing);
        MAP("theora_comment_clear", bridgeTheoraNothing);
        MAP("theora_comment_add", bridgeTheoraNothing);
        MAP("theora_comment_query", bridgeTheoraCommentQuery);
        MAP("theora_clear", bridgeTheoraNothing);
        MAP("theora_decode_header", bridgeTheoraBadHeader);
        MAP("theora_decode_init", bridgeTheoraBadHeader);
        MAP("theora_decode_packetin", bridgeTheoraBadHeader);
        MAP("theora_decode_YUVout", bridgeTheoraBadHeader);
        MAP("theora_encode_init", bridgeTheoraBadHeader);
        MAP("theora_encode_YUVin", bridgeTheoraBadHeader);
        MAP("theora_encode_packetout", bridgeTheoraBadHeader);
        MAP("theora_encode_header", bridgeTheoraBadHeader);
        MAP("theora_encode_comment", bridgeTheoraBadHeader);
        MAP("theora_encode_tables", bridgeTheoraBadHeader);
        MAP("theora_granule_frame", bridgeTheoraGranuleFrame);
        MAP("theora_granule_time", bridgeTheoraGranuleTime);
    }
}
#endif
