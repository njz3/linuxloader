// SDL 1.2 for the games built against libSDL-1.2.so.0 (Namco's Dead Heat): its window is the loader's (startSDL), its
// swap the GLX one (the picture scaled, the overlays, the agent), its controls the cabinet's I/O board's
// (hardware/namco). The structures are written by their i386 layout, keys numbered as SDL 1.2's SDLKey.

#include <SDL3/SDL.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "sdl12Bridge.h"
#include "blitStretching.h"
#include "fpsLimiter.h"
#include "overlayMsgs.h"
#include "sdlCalls.h"
#include "../agent/agent.h"
#include "../config/config.h"

extern SDL_Window *g_SdlWindow;
extern int gWidth;
extern int gHeight;

#define KEY_LAST 323 // SDLK_LAST

// SDL 1.2's SDL_PixelFormat and SDL_Surface, the fields a game reads from SDL_SetVideoMode's answer
typedef struct
{
    void *palette;
    uint8_t bitsPerPixel, bytesPerPixel;
    uint8_t loss[4], shift[4];
    uint32_t rmask, gmask, bmask, amask;
    uint32_t colorkey;
    uint8_t alpha;
} PixelFormat12;

typedef struct
{
    uint32_t flags;
    PixelFormat12 *format;
    int w, h;
    uint16_t pitch;
    void *pixels;
    int offset;
    void *hwdata;
    int16_t clipX, clipY;
    uint16_t clipW, clipH;
    uint32_t unused1, locked;
    void *map;
    unsigned int formatVersion;
    int refcount;
} Surface12;

static PixelFormat12 screenFormat = {NULL, 32, 4, {0, 0, 0, 0}, {16, 8, 0, 24}, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000, 0, 255};
static Surface12 screen;

int sdl12Init(uint32_t flags)
{
    (void)flags; // the loader has started SDL 3 (initSDL)
    return 0;
}

void sdl12Quit(void)
{
}

const char *sdl12GetError(void)
{
    return SDL_GetError();
}

// The game's size is its render resolution: the loader's window and picture follow it
void *sdl12SetVideoMode(int width, int height, int bpp, uint32_t flags)
{
    if (!g_SdlWindow)
    {
        if (width > 0 && height > 0)
        {
            gWidth = width;
            gHeight = height;
        }
        startSDL();
    }
    memset(&screen, 0, sizeof(screen));
    screen.flags = flags;
    screen.format = &screenFormat;
    screen.w = gWidth;
    screen.h = gHeight;
    screen.pitch = (uint16_t)(gWidth * 4);
    screen.clipW = (uint16_t)gWidth;
    screen.clipH = (uint16_t)gHeight;
    screen.refcount = 1;
    (void)bpp;
    return &screen;
}

int sdl12GLSetAttribute(int attribute, int value)
{
    (void)attribute; // the loader's context is made with its own attributes
    (void)value;
    return 0;
}

void sdl12GLSwapBuffers(void)
{
    blitStretch();
    overlayInit();
    overlayRender();
    SDL_GL_SwapWindow(g_SdlWindow);
    blitAfterSwap();
    SDL_PumpEvents(); // the window answers even if the game reads no event

    if (getConfig()->fpsLimiter)
        frameTiming();

    static char title[256];
    double fps = calculateFps();
    formatWindowTitle(title, sizeof(title), fps);
    SDL_SetWindowTitle(g_SdlWindow, title);
    fpsOverlayUpdateFps(fps);
}

int sdl12ShowCursor(int toggle)
{
    if (toggle == 1)
        SDL_ShowCursor();
    else if (toggle == 0)
        SDL_HideCursor();
    return SDL_CursorVisible() ? 1 : 0;
}

// --- keyboard

// The key of SDL 1.2 for SDL 3's: the ASCII ones are the same, the others go by scancode
static int key12(SDL_Scancode scancode, SDL_Keycode key)
{
    if (key > 0 && key < 128)
        return (int)key;
    if (scancode >= SDL_SCANCODE_F1 && scancode <= SDL_SCANCODE_F12)
        return 282 + (scancode - SDL_SCANCODE_F1);
    if (scancode >= SDL_SCANCODE_KP_1 && scancode <= SDL_SCANCODE_KP_9)
        return 257 + (scancode - SDL_SCANCODE_KP_1);
    switch (scancode)
    {
        case SDL_SCANCODE_KP_0:
            return 256;
        case SDL_SCANCODE_UP:
            return 273;
        case SDL_SCANCODE_DOWN:
            return 274;
        case SDL_SCANCODE_RIGHT:
            return 275;
        case SDL_SCANCODE_LEFT:
            return 276;
        case SDL_SCANCODE_INSERT:
            return 277;
        case SDL_SCANCODE_HOME:
            return 278;
        case SDL_SCANCODE_END:
            return 279;
        case SDL_SCANCODE_PAGEUP:
            return 280;
        case SDL_SCANCODE_PAGEDOWN:
            return 281;
        case SDL_SCANCODE_RSHIFT:
            return 303;
        case SDL_SCANCODE_LSHIFT:
            return 304;
        case SDL_SCANCODE_RCTRL:
            return 305;
        case SDL_SCANCODE_LCTRL:
            return 306;
        case SDL_SCANCODE_RALT:
            return 307;
        case SDL_SCANCODE_LALT:
            return 308;
        default:
            return 0;
    }
}

uint8_t *sdl12GetKeyState(int *numkeys)
{
    static uint8_t keys[KEY_LAST];
    memset(keys, 0, sizeof(keys));
    int count = 0;
    const bool *state = agentActive() ? NULL : SDL_GetKeyboardState(&count);
    for (int scancode = 0; state && scancode < count; scancode++)
    {
        if (!state[scancode])
            continue;
        int key = key12((SDL_Scancode)scancode, SDL_GetKeyFromScancode((SDL_Scancode)scancode, SDL_KMOD_NONE, false));
        if (key > 0 && key < KEY_LAST)
            keys[key] = 1;
    }
    if (numkeys)
        *numkeys = KEY_LAST;
    return keys;
}

uint8_t sdl12GetMouseState(int *x, int *y)
{
    float fx = 0.0f, fy = 0.0f;
    SDL_MouseButtonFlags buttons = SDL_GetMouseState(&fx, &fy);
    if (x)
        *x = (int)fx;
    if (y)
        *y = (int)fy;
    return (uint8_t)(buttons & 0x07); // left, middle, right: the same bits in SDL 1.2
}

// --- joysticks and events: the game's controls come through its I/O board (hardware/namco/deadHeatIo.c), fed by the
// loader's input layer (controls.ini, the agent). Its own joystick path (digital pedals, no coin) is given no joystick,
// so a controller is not read twice, and the events go to the loader (F10, F11, Esc, the bindings): the game gets
// none, and reads its keyboard and mouse by SDL_GetKeyState and SDL_GetMouseState.

int sdl12NumJoysticks(void)
{
    return 0;
}

void *sdl12JoystickOpen(int index)
{
    (void)index;
    return NULL;
}

int sdl12JoystickOpened(int index)
{
    (void)index;
    return 0;
}

void sdl12JoystickClose(void *joystick)
{
    (void)joystick;
}

int sdl12JoystickEventState(int state)
{
    (void)state;
    return 1; // SDL_ENABLE
}

int sdl12JoystickNumAxes(void *joystick)
{
    (void)joystick;
    return 0;
}

int sdl12JoystickNumButtons(void *joystick)
{
    (void)joystick;
    return 0;
}

int16_t sdl12JoystickGetAxis(void *joystick, int axis)
{
    (void)joystick;
    (void)axis;
    return 0;
}

int sdl12PollEvent(void *event)
{
    (void)event;
    pollEvents();
    return 0;
}

int sdl12PushEvent(void *event)
{
    (void)event; // the game reads no event (sdl12PollEvent)
    return 0;
}

// --- what Raw Thrills' Cruis'n Blast asks more: the video information, the window manager, time, threads

// SDL_VideoInfo: the capability bits, the video memory, the format, the size of the screen
typedef struct
{
    uint32_t bits;
    uint32_t videoMem;
    PixelFormat12 *format;
    int currentW, currentH;
} VideoInfo12;

const void *sdl12GetVideoInfo(void)
{
    static VideoInfo12 info;
    info.bits = 0x3; // hw_available, wm_available
    info.videoMem = 512 * 1024;
    info.format = &screenFormat;
    info.currentW = gWidth;
    info.currentH = gHeight;
    return &info;
}

void *sdl12GetVideoSurface(void)
{
    return screen.format ? &screen : NULL;
}

int sdl12Flip(void *surface)
{
    (void)surface;
    return 0; // a GL game swaps by SDL_GL_SwapBuffers
}

void sdl12FreeSurface(void *surface)
{
    (void)surface;
}

uint32_t sdl12WasInit(uint32_t flags)
{
    return flags;
}

int sdl12EnableKeyRepeat(int delay, int interval)
{
    (void)delay;
    (void)interval;
    return 0;
}

void sdl12SetError(const char *format, ...)
{
    char message[512];
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);
    SDL_SetError("%s", message);
}

void sdl12Error(int code)
{
    (void)code;
}

// SDL 1.2's attributes are numbered as SDL 3's up to SDL_GL_ACCUM_ALPHA_SIZE: the loader's context is made with its
// own, those are what it asks
int sdl12GLGetAttribute(int attribute, int *value)
{
    static const int values[] = {8, 8, 8, 8, 32, 1, 24, 8, 0, 0, 0, 0};
    if (value)
        *value = (attribute >= 0 && attribute < (int)(sizeof(values) / sizeof(values[0]))) ? values[attribute] : 0;
    return 0;
}

void sdl12WMSetCaption(const char *title, const char *icon)
{
    (void)title; // the loader titles its window
    (void)icon;
}

int sdl12WMGrabInput(int mode)
{
    (void)mode;
    return 0; // SDL_GRAB_OFF
}

void sdl12WarpMouse(uint16_t x, uint16_t y)
{
    (void)x;
    (void)y;
}

uint8_t sdl12GetRelativeMouseState(int *x, int *y)
{
    if (x)
        *x = 0;
    if (y)
        *y = 0;
    return 0;
}

uint32_t sdl12GetTicks(void)
{
    return (uint32_t)SDL_GetTicks();
}

void sdl12Delay(uint32_t ms)
{
    SDL_Delay(ms);
}

uint32_t sdl12ThreadID(void)
{
    return (uint32_t)SDL_GetCurrentThreadID();
}

// --- audio, files and shared objects: none. The game's sound goes through FMOD; without SDL audio it plays no
// sample of its own, without SDL_RWops it loads no WAV that way.

int sdl12OpenAudio(void *desired, void *obtained)
{
    (void)desired;
    (void)obtained;
    SDL_SetError("SDL 1.2 audio is not available: the sound goes through FMOD");
    return -1;
}

void sdl12AudioNothing(void)
{
}

void sdl12PauseAudio(int pause)
{
    (void)pause;
}

void sdl12MixAudio(uint8_t *dst, const uint8_t *src, uint32_t len, int volume)
{
    (void)dst;
    (void)src;
    (void)len;
    (void)volume;
}

int sdl12Unavailable(void)
{
    return -1;
}

void *sdl12Null(void)
{
    return NULL;
}

void sdl12Free(void *object)
{
    (void)object;
}

uint32_t sdl12ReadZero(void *rw)
{
    (void)rw;
    return 0;
}
