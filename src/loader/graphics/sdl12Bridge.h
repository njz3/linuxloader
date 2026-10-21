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

#ifdef __cplusplus
}
#endif

#endif
