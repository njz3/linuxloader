#ifndef FFB_H
#define FFB_H

#include <stddef.h>

void sdlFfbInit(void);
void sdlFfbShutdown(void);
void sdlFfbRumble(float left, float right, int duration_ms);
void sdlFfbDriveboard(const unsigned char *buffer, size_t count);
void sdlFfbPause(int paused);
void sdlFfbOutput(const unsigned char *buffer, size_t count);
// The agent mode: the board model's effects now (constant, spring, damper, sine strengths, the device's -1..1)
int sdlFfbAgentEffects(float effects[4]);

// [ForceFeedback] FFB_DEVICE: AUTO (or empty) for the first controller's device, else a force feedback device's name
// as SDL gives it (the DirectInput product name on Windows), matched without case and the spaces around it
#define FFB_DEVICE_NAME 128
int sdlFfbAutomatic(const char *name);
int sdlFfbNameMatches(const char *device, const char *name);
// The force feedback devices connected, by name: their count, up to max
int sdlFfbDevices(char names[][FFB_DEVICE_NAME], int max);
// The effects move to FFB_DEVICE's device now; before the game's start, nothing (the start reads FFB_DEVICE)
void sdlFfbSelect(const char *name);
// The device the effects go to, NULL for none; the controller slot whose device it is, -1 for none
const char *sdlFfbCurrentDevice(void);
int sdlFfbJoystick(void);

#endif