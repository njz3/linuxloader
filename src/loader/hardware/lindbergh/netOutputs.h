#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// MAME-style network outputs, same protocol as the njz3 sm2-emu and Supermodel forks:
// a TCP server broadcasting "name = value\r" lines and a UDP start announcement (BackForceFeeder).
void netOutputsInit(void);
void netOutputsShutdown(void);
// The server started (the ports read again from the settings) or stopped while the game runs; false when it could not
// start (netOutputsGetStatus says why)
bool netOutputsEnable(bool on);

typedef struct
{
    bool running;   // the server listens
    int tcpPort;    // the port it listens on, or failed to open
    int udpPort;    // where its start was announced
    int clients;    // connected now
    int error;      // why the last start failed (WSAGetLastError, errno), 0 when it did not
    bool portInUse; // the error is that another program has the port
} NetOutputsStatus;
void netOutputsGetStatus(NetOutputsStatus *status);

// JVS general purpose output byte (lamps), byteIndex 0 holds outputs 1-8 in bits 7..0
void netOutputsSetGpo(int byteIndex, uint8_t value);
// One output, 0 the first (lamp0, bit 7 of byte 0), cleared, set or inverted as JVS's CMD_WRITE_GPO_BIT does
#define NET_GPO_CLEAR 0
#define NET_GPO_SET 1
#define NET_GPO_INVERT 2
void netOutputsSetGpoBit(int output, int action);
// Bytes the game writes to the drive board, in order
void netOutputsDrive(const uint8_t *bytes, size_t count);

// For the settings' list: whether the server runs, an output byte's last value and the last drive board byte, -1 if
// the game gave none yet (kept while the server does not run)
bool netOutputsRunning(void);
int netOutputsGpo(int byteIndex);
int netOutputsLastDrive(void);

// A lamp also sent under a name other tools read (LampStart...), for the games whose lamps are known: its JVS outputs,
// 0 the first as lampN (-1 for none), on when either is
typedef struct
{
    const char *name;
    int outputs[2];
} NetNamedLamp;
// The game's: their count, at most max copied, each one's value now in values (-1 while the game gave none)
int netOutputsNamedLamps(NetNamedLamp *lamps, int *values, int max);

#ifdef __cplusplus
}
#endif
