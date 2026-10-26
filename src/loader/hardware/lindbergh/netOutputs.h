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
// none was sent yet (or the server does not run)
bool netOutputsRunning(void);
int netOutputsGpo(int byteIndex);
int netOutputsLastDrive(void);

#ifdef __cplusplus
}
#endif
