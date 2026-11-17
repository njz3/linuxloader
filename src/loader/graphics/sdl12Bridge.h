#ifndef SDL12_BRIDGE_H
#define SDL12_BRIDGE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// SDL 1.2, the functions games built against libSDL-1.2.so.0 call (Namco's Dead Heat), over SDL 3 and the loader's
// window: sdl12Bridge.c
int sdl12Init(uint32_t flags);
void sdl12Quit(void);
const char *sdl12GetError(void);
void *sdl12SetVideoMode(int width, int height, int bpp, uint32_t flags);
int sdl12GLSetAttribute(int attribute, int value);
void sdl12GLSwapBuffers(void);
int sdl12ShowCursor(int toggle);
int sdl12PollEvent(void *event);
uint8_t *sdl12GetKeyState(int *numkeys);
uint8_t sdl12GetMouseState(int *x, int *y);
int sdl12NumJoysticks(void);
void *sdl12JoystickOpen(int index);
int sdl12JoystickOpened(int index);
void sdl12JoystickClose(void *joystick);
int sdl12JoystickEventState(int state);
int sdl12JoystickNumAxes(void *joystick);
int sdl12JoystickNumButtons(void *joystick);
int16_t sdl12JoystickGetAxis(void *joystick, int axis);
uint8_t sdl12JoystickGetButton(void *joystick, int button);
int sdl12PushEvent(void *event);

// What Raw Thrills' Cruis'n Blast calls more
const void *sdl12GetVideoInfo(void);
void *sdl12GetVideoSurface(void);
int sdl12Flip(void *surface);
void sdl12FreeSurface(void *surface);
uint32_t sdl12WasInit(uint32_t flags);
int sdl12EnableKeyRepeat(int delay, int interval);
void sdl12SetError(const char *format, ...);
void sdl12Error(int code);
int sdl12GLGetAttribute(int attribute, int *value);
void sdl12WMSetCaption(const char *title, const char *icon);
int sdl12WMGrabInput(int mode);
void sdl12WarpMouse(uint16_t x, uint16_t y);
uint8_t sdl12GetRelativeMouseState(int *x, int *y);
uint32_t sdl12GetTicks(void);
void sdl12Delay(uint32_t ms);
uint32_t sdl12ThreadID(void);
int sdl12OpenAudio(void *desired, void *obtained);
void sdl12AudioNothing(void);
void sdl12PauseAudio(int pause);
void sdl12CloseAudio(void);
void sdl12LockAudio(void);
void sdl12UnlockAudio(void);
void *sdl12RWFromFile(const char *file, const char *mode);
void sdl12FreeRW(void *rw);
uint32_t sdl12ReadLE32(void *rw);
uint32_t sdl12ReadBE32(void *rw);
uint16_t sdl12ReadBE16(void *rw);
void *sdl12LoadWAV_RW(void *src, int freesrc, void *spec, uint8_t **audioBuf, uint32_t *audioLen);
void sdl12FreeWAV(uint8_t *audioBuf);
int sdl12BuildAudioCVT(void *cvt, uint16_t srcFormat, uint8_t srcChannels, int srcRate, uint16_t dstFormat, uint8_t dstChannels,
                       int dstRate);
int sdl12ConvertAudio(void *cvt);
void sdl12MixAudio(uint8_t *dst, const uint8_t *src, uint32_t len, int volume);
int sdl12Unavailable(void);
void *sdl12Null(void);
void sdl12Free(void *object);
uint32_t sdl12ReadZero(void *rw);

#ifdef __cplusplus
}
#endif

#endif
