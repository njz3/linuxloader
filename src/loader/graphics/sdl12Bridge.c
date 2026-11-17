// SDL 1.2 for the games built against libSDL-1.2.so.0 (Namco's Dead Heat): its window is the loader's (startSDL), its
// swap the GLX one (the picture scaled, the overlays, the agent), its controls the cabinet's I/O board's
// (hardware/namco). The structures are written by their i386 layout, keys numbered as SDL 1.2's SDLKey.

#include <SDL3/SDL.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sdl12Bridge.h"
#include "../redirections/filesystemShared.h"
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
// so a controller is not read twice, and the events go to the loader (Tab, F11, Esc, the bindings): the game gets
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

uint8_t sdl12JoystickGetButton(void *joystick, int button)
{
    (void)joystick;
    (void)button;
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

// --- audio and files, SDL 1.2's (Raw Thrills' Cruis'n Blast: its SDL_mixer, linked in the game, plays its menus'
// samples; the rest of its sound goes through FMOD). The structures are written by their i386 layout: SDL_mixer reads
// an SDL_RWops's function pointers itself.

typedef struct Sdl12RWops Sdl12RWops;
struct Sdl12RWops
{
    int (*seek)(Sdl12RWops *ctx, int offset, int whence);
    int (*read)(Sdl12RWops *ctx, void *ptr, int size, int maxnum);
    int (*write)(Sdl12RWops *ctx, const void *ptr, int size, int num);
    int (*close)(Sdl12RWops *ctx);
    uint32_t type;
    int autoclose; // hidden.stdio
    FILE *fp;
};

static int rwSeek(Sdl12RWops *ctx, int offset, int whence)
{
    if (fseek(ctx->fp, offset, whence) != 0)
        return -1;
    return (int)ftell(ctx->fp);
}

static int rwRead(Sdl12RWops *ctx, void *ptr, int size, int maxnum)
{
    if (size <= 0 || maxnum <= 0)
        return 0;
    return (int)fread(ptr, (size_t)size, (size_t)maxnum, ctx->fp);
}

static int rwWrite(Sdl12RWops *ctx, const void *ptr, int size, int num)
{
    if (size <= 0 || num <= 0)
        return 0;
    return (int)fwrite(ptr, (size_t)size, (size_t)num, ctx->fp);
}

static int rwClose(Sdl12RWops *ctx)
{
    if (ctx)
    {
        if (ctx->fp && ctx->autoclose)
            fclose(ctx->fp);
        free(ctx);
    }
    return 0;
}

// The game's paths: through the loader's file calls (the cabinet's /pm, the write folder)
void *sdl12RWFromFile(const char *file, const char *mode)
{
    FILE *fp = file && mode ? sharedFopen(file, mode) : NULL;
    if (!fp)
    {
        SDL_SetError("Couldn't open %s", file ? file : "(null)");
        return NULL;
    }
    Sdl12RWops *rw = (Sdl12RWops *)calloc(1, sizeof(Sdl12RWops));
    if (!rw)
    {
        fclose(fp);
        return NULL;
    }
    rw->seek = rwSeek;
    rw->read = rwRead;
    rw->write = rwWrite;
    rw->close = rwClose;
    rw->type = 2; // SDL_RWOPS_STDFILE
    rw->autoclose = 1;
    rw->fp = fp;
    return rw;
}

void sdl12FreeRW(void *rw)
{
    free(rw);
}

static uint32_t rwRead32(Sdl12RWops *rw, int bigEndian)
{
    uint8_t b[4] = {0, 0, 0, 0};
    if (rw)
        rw->read(rw, b, 1, 4);
    return bigEndian ? (uint32_t)b[0] << 24 | b[1] << 16 | b[2] << 8 | b[3] : (uint32_t)b[3] << 24 | b[2] << 16 | b[1] << 8 | b[0];
}

uint32_t sdl12ReadLE32(void *rw)
{
    return rwRead32((Sdl12RWops *)rw, 0);
}

uint32_t sdl12ReadBE32(void *rw)
{
    return rwRead32((Sdl12RWops *)rw, 1);
}

uint16_t sdl12ReadBE16(void *rw)
{
    uint8_t b[2] = {0, 0};
    if (rw)
        ((Sdl12RWops *)rw)->read((Sdl12RWops *)rw, b, 1, 2);
    return (uint16_t)(b[0] << 8 | b[1]);
}

typedef struct // SDL 1.2's SDL_AudioSpec
{
    int freq;
    uint16_t format;
    uint8_t channels;
    uint8_t silence;
    uint16_t samples;
    uint16_t padding;
    uint32_t size;
    void (*callback)(void *userdata, uint8_t *stream, int len);
    void *userdata;
} Sdl12AudioSpec;

#define SDL12_AUDIO_U8 0x0008
#define SDL12_AUDIO_S8 0x8008
#define SDL12_AUDIO_S16LSB 0x8010

static int sdl12BytesPerSample(uint16_t format)
{
    return (format & 0xff) / 8;
}

// A RIFF WAVE of PCM samples (8 or 16 bits): all the game's are 16-bit
void *sdl12LoadWAV_RW(void *src, int freesrc, void *specOut, uint8_t **audioBuf, uint32_t *audioLen)
{
    Sdl12RWops *rw = (Sdl12RWops *)src;
    Sdl12AudioSpec *spec = (Sdl12AudioSpec *)specOut;
    void *result = NULL;
    if (!rw || !spec || !audioBuf || !audioLen)
    {
        SDL_SetError("Mix_LoadWAV_RW with NULL src");
        goto done;
    }
    uint8_t header[12];
    if (rw->read(rw, header, 1, 12) != 12 || memcmp(header, "RIFF", 4) != 0 || memcmp(header + 8, "WAVE", 4) != 0)
    {
        SDL_SetError("Unrecognized file type (not WAVE)");
        goto done;
    }
    uint16_t format = 0, channels = 0, bits = 0;
    uint32_t rate = 0;
    for (;;)
    {
        uint8_t chunk[8];
        if (rw->read(rw, chunk, 1, 8) != 8)
        {
            SDL_SetError("Complete WAVE file not found");
            goto done;
        }
        uint32_t size = (uint32_t)chunk[4] | chunk[5] << 8 | chunk[6] << 16 | (uint32_t)chunk[7] << 24;
        if (memcmp(chunk, "fmt ", 4) == 0)
        {
            uint8_t fmt[16];
            if (size < 16 || rw->read(rw, fmt, 1, 16) != 16)
                goto done;
            format = (uint16_t)(fmt[0] | fmt[1] << 8);
            channels = (uint16_t)(fmt[2] | fmt[3] << 8);
            rate = (uint32_t)fmt[4] | fmt[5] << 8 | fmt[6] << 16 | (uint32_t)fmt[7] << 24;
            bits = (uint16_t)(fmt[14] | fmt[15] << 8);
            if (size > 16)
                rw->seek(rw, (int)(size - 16 + (size & 1)), SEEK_CUR);
        }
        else if (memcmp(chunk, "data", 4) == 0)
        {
            if (format != 1 || (bits != 8 && bits != 16) || channels == 0)
            {
                SDL_SetError("Unknown WAVE data format: 0x%.4x", format);
                goto done;
            }
            uint8_t *data = (uint8_t *)malloc(size ? size : 1);
            if (!data || rw->read(rw, data, 1, (int)size) != (int)size)
            {
                free(data);
                SDL_SetError("Error reading the WAVE data");
                goto done;
            }
            memset(spec, 0, sizeof(*spec));
            spec->freq = (int)rate;
            spec->format = bits == 8 ? SDL12_AUDIO_U8 : SDL12_AUDIO_S16LSB;
            spec->channels = (uint8_t)channels;
            spec->silence = bits == 8 ? 0x80 : 0;
            spec->samples = 4096;
            *audioBuf = data;
            *audioLen = size;
            result = spec;
            goto done;
        }
        else
            rw->seek(rw, (int)(size + (size & 1)), SEEK_CUR);
    }
done:
    if (freesrc && rw)
        rw->close(rw);
    return result;
}

void sdl12FreeWAV(uint8_t *audioBuf)
{
    free(audioBuf);
}

typedef struct // SDL 1.2's SDL_AudioCVT, i386: the doubles at 4-byte alignment
{
    int needed;
    uint16_t src_format;
    uint16_t dst_format;
    double rate_incr;
    uint8_t *buf;
    int len;
    int len_cvt;
    int len_mult;
    double len_ratio;
    void *filters[10]; // here: the channels and the rates, for sdl12ConvertAudio
    int filter_index;
} __attribute__((packed, aligned(4))) Sdl12AudioCVT;

int sdl12BuildAudioCVT(void *cvtOut, uint16_t srcFormat, uint8_t srcChannels, int srcRate, uint16_t dstFormat, uint8_t dstChannels,
                       int dstRate)
{
    Sdl12AudioCVT *cvt = (Sdl12AudioCVT *)cvtOut;
    memset(cvt, 0, sizeof(*cvt));
    cvt->src_format = srcFormat;
    cvt->dst_format = dstFormat;
    cvt->filters[0] = (void *)(uintptr_t)srcChannels;
    cvt->filters[1] = (void *)(uintptr_t)dstChannels;
    cvt->filters[2] = (void *)(uintptr_t)srcRate;
    cvt->filters[3] = (void *)(uintptr_t)dstRate;
    double ratio = (double)dstRate / (double)(srcRate > 0 ? srcRate : 1) * (double)dstChannels / (double)(srcChannels ? srcChannels : 1) *
                   (double)sdl12BytesPerSample(dstFormat) / (double)(sdl12BytesPerSample(srcFormat) ? sdl12BytesPerSample(srcFormat) : 1);
    cvt->len_ratio = ratio;
    cvt->rate_incr = (double)dstRate / (double)(srcRate > 0 ? srcRate : 1);
    cvt->len_mult = (int)ratio + 2;
    cvt->needed = srcFormat != dstFormat || srcChannels != dstChannels || srcRate != dstRate;
    return cvt->needed;
}

static SDL_AudioFormat sdl3Format(uint16_t format)
{
    return format == SDL12_AUDIO_U8 ? SDL_AUDIO_U8 : format == SDL12_AUDIO_S8 ? SDL_AUDIO_S8 : SDL_AUDIO_S16LE;
}

// In place, cvt->buf holding len * len_mult bytes as SDL 1.2 asks
int sdl12ConvertAudio(void *cvtIn)
{
    Sdl12AudioCVT *cvt = (Sdl12AudioCVT *)cvtIn;
    if (!cvt || !cvt->buf)
        return -1;
    if (!cvt->needed)
    {
        cvt->len_cvt = cvt->len;
        return 0;
    }
    SDL_AudioSpec src = {sdl3Format(cvt->src_format), (int)(uintptr_t)cvt->filters[0], (int)(uintptr_t)cvt->filters[2]};
    SDL_AudioSpec dst = {sdl3Format(cvt->dst_format), (int)(uintptr_t)cvt->filters[1], (int)(uintptr_t)cvt->filters[3]};
    Uint8 *out = NULL;
    int outLength = 0;
    if (!SDL_ConvertAudioSamples(&src, cvt->buf, cvt->len, &dst, &out, &outLength))
        return -1;
    int room = cvt->len * cvt->len_mult;
    if (outLength > room)
        outLength = room;
    memcpy(cvt->buf, out, (size_t)outLength);
    SDL_free(out);
    cvt->len_cvt = outLength;
    return 0;
}

// The device: the game's callback fills what SDL3's stream asks for
static SDL_AudioStream *sdl12Stream = NULL;
static Sdl12AudioSpec sdl12Spec;
static SDL_AudioFormat sdl12Format = SDL_AUDIO_S16LE;

static void SDLCALL sdl12AudioCallback(void *userdata, SDL_AudioStream *stream, int additional, int total)
{
    (void)userdata;
    (void)total;
    static Uint8 *buffer = NULL;
    static int bufferSize = 0;
    if (additional <= 0 || !sdl12Spec.callback)
        return;
    if (additional > bufferSize)
    {
        Uint8 *bigger = (Uint8 *)realloc(buffer, (size_t)additional);
        if (!bigger)
            return;
        buffer = bigger;
        bufferSize = additional;
    }
    memset(buffer, sdl12Spec.silence, (size_t)additional);
    sdl12Spec.callback(sdl12Spec.userdata, buffer, additional);
    SDL_PutAudioStreamData(stream, buffer, additional);
}

int sdl12OpenAudio(void *desired, void *obtained)
{
    const Sdl12AudioSpec *want = (const Sdl12AudioSpec *)desired;
    if (!want || !want->callback)
    {
        SDL_SetError("SDL_OpenAudio() passed a NULL callback");
        return -1;
    }
    if (sdl12Stream)
    {
        SDL_SetError("Audio device is already opened");
        return -1;
    }
    if (!SDL_WasInit(SDL_INIT_AUDIO))
        SDL_InitSubSystem(SDL_INIT_AUDIO);
    sdl12Spec = *want;
    if (sdl12Spec.format != SDL12_AUDIO_U8 && sdl12Spec.format != SDL12_AUDIO_S8)
        sdl12Spec.format = SDL12_AUDIO_S16LSB;
    sdl12Format = sdl3Format(sdl12Spec.format);
    sdl12Spec.silence = sdl12Spec.format == SDL12_AUDIO_U8 ? 0x80 : 0;
    if (!sdl12Spec.samples)
        sdl12Spec.samples = 1024;
    sdl12Spec.size = (uint32_t)sdl12Spec.samples * sdl12Spec.channels * (uint32_t)sdl12BytesPerSample(sdl12Spec.format);
    SDL_AudioSpec spec = {sdl12Format, sdl12Spec.channels ? sdl12Spec.channels : 2, sdl12Spec.freq ? sdl12Spec.freq : 44100};
    sdl12Stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, sdl12AudioCallback, NULL);
    if (!sdl12Stream)
        return -1;
    if (agentHidden()) // --agent: the sound runs, silent
        SDL_SetAudioStreamGain(sdl12Stream, 0.0f);
    if (obtained)
        *(Sdl12AudioSpec *)obtained = sdl12Spec;
    return 0; // paused until SDL_PauseAudio(0), as SDL 1.2's
}

void sdl12CloseAudio(void)
{
    if (sdl12Stream)
    {
        SDL_DestroyAudioStream(sdl12Stream);
        sdl12Stream = NULL;
    }
}

void sdl12LockAudio(void)
{
    if (sdl12Stream)
        SDL_LockAudioStream(sdl12Stream);
}

void sdl12UnlockAudio(void)
{
    if (sdl12Stream)
        SDL_UnlockAudioStream(sdl12Stream);
}

void sdl12PauseAudio(int pause)
{
    if (!sdl12Stream)
        return;
    if (pause)
        SDL_PauseAudioStreamDevice(sdl12Stream);
    else
        SDL_ResumeAudioStreamDevice(sdl12Stream);
}

// volume 0..128 (SDL_MIX_MAXVOLUME), in the device's format
void sdl12MixAudio(uint8_t *dst, const uint8_t *src, uint32_t len, int volume)
{
    if (!dst || !src || !len || volume <= 0)
        return;
    SDL_MixAudio(dst, src, sdl12Format, len, (float)(volume > 128 ? 128 : volume) / 128.0f);
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
