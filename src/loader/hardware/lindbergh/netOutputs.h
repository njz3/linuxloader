#pragma once

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
// Bytes the game writes to the drive board, in order
void netOutputsDrive(const uint8_t *bytes, size_t count);

#ifdef __cplusplus
}
#endif
