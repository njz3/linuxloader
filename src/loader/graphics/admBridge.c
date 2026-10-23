// NVIDIA's arcade driver, built into Namco's N2 programs (Maximum Tune 3): its "adm" layer drives /dev/nvidia itself
// (devices, display modes, screens, windows, contexts, swaps), and the program's OpenGL entry points are 32-byte slots
// of .writetext the driver fills in once a context is current. Here the adm functions the game calls are answered with
// the loader's window and context (startSDL), its swap is the loader's (the picture scaled, the overlays, the agent),
// and each OpenGL slot jumps to the loader's function of that name (glHooks), or to a stub that reports it.
// The functions are found by name in the program's symbols, the OpenGL table in admGetProcAddress's code.

#include <SDL3/SDL.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#include "admBridge.h"
#include "sdl12Bridge.h"
#include "sdlCalls.h"
#include "../elfLoader/glHooks.hpp"
#include "../log/log.h"
#include "../patching/flowControl.h"

extern SDL_Window *g_SdlWindow;
extern SDL_GLContext g_SdlContext;
extern int gWidth;
extern int gHeight;

#define ADM_WIDTH 0x500 // the attributes of the mode, screen and window lists
#define ADM_HEIGHT 0x501
#define ADM_REFRESH 0x502 // in thousandths of a hertz
#define ADM_DEPTH 0x503
#define ADM_WINDOW_WIDTH 0x802
#define ADM_WINDOW_HEIGHT 0x803
#define ADM_TEXTURE_MEMORY 0x200 // admGetDeviceAttribi, the frame's statistics
#define ADM_TEXTURE_MEMORY_FREE 0x207

// The driver's objects: one of each, the window marked as the driver marks its own
typedef struct
{
    uint32_t magic;
} AdmObject;

static AdmObject device = {0x56454441}, modeConfig = {0x45444F4D}, fbConfig = {0x47464246}, screen = {0x4E524353},
                 context = {0x58544E43}, window = {0x57444E57}, pbuffer = {0x46554250};
static AdmObject *modeConfigs[] = {&modeConfig, NULL};
static AdmObject *fbConfigs[] = {&fbConfig, NULL};
static int width = 640, height = 480, swapInterval = 1;

static void traceDump(void);

// The (attribute, value) list's value for that attribute, else fallback
static int attribute(const int *list, int name, int fallback)
{
    for (; list && list[0]; list += 2)
        if (list[0] == name)
            return list[1];
    return fallback;
}

static int admTrue(void)
{
    return 1;
}

static int admShutdown(int mode)
{
    (void)mode;
    return 1;
}

static const char *admGetString(int name)
{
    (void)name;
    return "NVIDIA arcade display manager (linuxloader)";
}

static void *admInitDevicei(int index, const int *attributes)
{
    (void)index;
    (void)attributes;
    return &device;
}

static int admGetDeviceAttribi(void *dev, int name, int *value)
{
    (void)dev;
    if (value)
        *value = name == ADM_TEXTURE_MEMORY ? 256 << 20 : name == ADM_TEXTURE_MEMORY_FREE ? 192 << 20 : 0;
    return 1;
}

// The game asks for its render resolution (640x480 for Maximum Tune 3): it is the one mode
static AdmObject **admChooseModeConfigi(void *dev, int screenIndex, const int *attributes, int max, int *count, int flags)
{
    (void)dev;
    (void)screenIndex;
    (void)max;
    (void)flags;
    width = attribute(attributes, ADM_WIDTH, width);
    height = attribute(attributes, ADM_HEIGHT, height);
    if (count)
        *count = 1;
    return modeConfigs;
}

static int admGetModeConfigAttribi(void *config, int name, int *value)
{
    (void)config;
    if (value)
        *value = name == ADM_WIDTH ? width : name == ADM_HEIGHT ? height : name == ADM_REFRESH ? 60000 : name == ADM_DEPTH ? 24 : 0;
    return 1;
}

static AdmObject **admChooseFBConfigi(void *dev, int depth, const int *attributes, int max, int *count)
{
    (void)dev;
    (void)depth;
    (void)attributes;
    (void)max;
    if (count)
        *count = 1;
    return fbConfigs;
}

static int admGetFBConfigAttribi(void *config, int name, int *value)
{
    (void)config;
    (void)name;
    if (value)
        *value = 0;
    return 1;
}

static void *admCreateScreeni(void *dev, const int *attributes)
{
    (void)dev;
    width = attribute(attributes, ADM_WIDTH, width);
    height = attribute(attributes, ADM_HEIGHT, height);
    return &screen;
}

static void *admCreateGraphicsContext(void *dev, void *config, void *scr, void *share)
{
    (void)dev;
    (void)config;
    (void)scr;
    (void)share;
    return &context;
}

// The game's window is the loader's, at the game's render resolution
static void *admCreateWindowi(void *dev, void *scr, void *config, const int *attributes)
{
    (void)dev;
    (void)scr;
    (void)config;
    int w = attribute(attributes, ADM_WINDOW_WIDTH, width), h = attribute(attributes, ADM_WINDOW_HEIGHT, height);
    log_info("adm: window %dx%d", w, h);
    sdl12SetVideoMode(w, h, 32, 0);
    return &window;
}

static void *admCreatePbufferi(void *dev, void *config, const int *attributes)
{
    (void)dev;
    (void)config;
    (void)attributes;
    log_warn("adm: a pbuffer asked, none made");
    return &pbuffer;
}

// One context, made current again on a thread that has none
static int admMakeContextCurrent(void *ctx, void *draw, void *read)
{
    (void)draw;
    (void)read;
    if (ctx && g_SdlWindow && SDL_GL_GetCurrentContext() != g_SdlContext)
        SDL_GL_MakeCurrent(g_SdlWindow, g_SdlContext);
    return 1;
}

static int admSwapInterval(int interval)
{
    swapInterval = interval; // the loader's frame limiter paces the frames
    return 1;
}

static int admGetSwapInterval(void)
{
    return swapInterval;
}

static void *admGetCurrentContext(void)
{
    return &context;
}

static void *admGetCurrentDrawable(void)
{
    return &window;
}

// The loader's swap, as SDL 1.2 games': the picture scaled to the window, the overlays, the agent's frame. The game
// reads no SDL event (its inputs are the N2 board's), so the loader's own are handled here: controllers, keyboard, F10
static int admSwapBuffers(void *drawable)
{
    (void)drawable;
    traceDump();
    sdl12GLSwapBuffers();
    pollEvents();
    return 1;
}

static int admGetDrawableAttribi(void *drawable, int name, int *value)
{
    (void)drawable;
    if (value)
        *value = name == ADM_WIDTH || name == ADM_WINDOW_WIDTH ? width : name == ADM_HEIGHT || name == ADM_WINDOW_HEIGHT ? height : 0;
    return 1;
}

// No video memory of the driver's to give (NV_vertex_array_range): the game keeps its arrays in its own memory
static void *admAllocateMemory(int size, float readFrequency, float writeFrequency, float priority)
{
    (void)size;
    (void)readFrequency;
    (void)writeFrequency;
    (void)priority;
    return NULL;
}

static void admFreeMemory(void *pointer)
{
    (void)pointer;
}

static const char *const *glNames;
static uint8_t *const *glSlots;
static unsigned glCount, glStride;

// LL_ADM_TRACE: each slot counts its calls first ("inc dword [count]"), the counts logged every 600 swaps
static uint32_t *glCalls;
static uint32_t *glCallsSeen;

static void traceDump(void)
{
    static int swaps;
    if (!glCalls || ++swaps % 600)
        return;
    for (unsigned i = 0; i < glCount; i++)
        if (glCalls[i] != glCallsSeen[i])
        {
            log_warn("adm trace: %-36s %u", *(const char *const *)((const uint8_t *)glNames + i * glStride),
                     glCalls[i] - glCallsSeen[i]);
            glCallsSeen[i] = glCalls[i];
        }
}

static void *admGetProcAddress(const char *name)
{
    for (unsigned i = 0; i < glCount; i++)
        if (strcmp(*(const char *const *)((const uint8_t *)glNames + i * glStride), name) == 0)
            return *(uint8_t *const *)((const uint8_t *)glSlots + i * glStride);
    return GLHooks_GetProcAddress(name);
}

// An OpenGL function the loader has no wrapper for: reported once, 0 answered
static int admMissingGl(const char *name)
{
    static const char *reported[256];
    static int count;
    for (int i = 0; i < count; i++)
        if (reported[i] == name)
            return 0;
    if (count < 256)
        reported[count++] = name;
    log_warn("adm: OpenGL function %s has no wrapper", name);
    return 0;
}

static int emptySlot(const uint8_t *slot)
{
    for (int i = 0; i < 32; i++)
        if (slot[i])
            return 0;
    return 1;
}

// admGetProcAddress walks a table of {name, slot, ...} entries: "push ebp; mov ebp, TABLE", the count in a
// "cmp esi, LAST". Each empty slot gets a jump to the loader's function, or "push name; call admMissingGl; add esp, 4;
// ret"; the driver's own functions (glpm*) are left as they are.
static void fillGlSlots(const uint8_t *code)
{
    if (code[0] != 0x55 || code[1] != 0xBD)
    {
        log_error("adm: admGetProcAddress is not the one known, the OpenGL entry points stay empty");
        return;
    }
    const uint8_t *table = *(const uint8_t *const *)(code + 2);
    for (int i = 6; i < 0x40 && !glCount; i++)
        if (code[i] == 0x81 && code[i + 1] == 0xFE)
            glCount = *(const uint32_t *)(code + i + 2) + 1;
    glStride = 0x18;
    glNames = (const char *const *)table;
    glSlots = (uint8_t *const *)(table + 4);

    if (getenv("LL_ADM_TRACE"))
    {
        glCalls = (uint32_t *)calloc(glCount, sizeof(uint32_t));
        glCallsSeen = (uint32_t *)calloc(glCount, sizeof(uint32_t));
    }
    int jumps = 0, stubs = 0;
    for (unsigned i = 0; i < glCount; i++)
    {
        const char *name = *(const char *const *)(table + i * glStride);
        uint8_t *slot = *(uint8_t *const *)(table + i * glStride + 4);
        if (!emptySlot(slot))
            continue;
        DWORD protection;
        VirtualProtect(slot, 32, PAGE_EXECUTE_READWRITE, &protection);
        void *target = GLHooks_GetProcAddress(name);
        uint8_t *code = slot;
        if (glCalls)
        {
            code[0] = 0xFF; // inc dword [glCalls + i]
            code[1] = 0x05;
            *(uint32_t **)(code + 2) = glCalls + i;
            code += 6;
        }
        if (target)
        {
            code[0] = 0xE9;
            *(int32_t *)(code + 1) = (int32_t)((uint8_t *)target - (code + 5));
            jumps++;
        }
        else
        {
            code[0] = 0x68;
            *(const char **)(code + 1) = name;
            code[5] = 0xE8;
            *(int32_t *)(code + 6) = (int32_t)((uint8_t *)admMissingGl - (code + 10));
            memcpy(code + 10, "\x83\xC4\x04\xC3", 4);
            stubs++;
        }
        VirtualProtect(slot, 32, protection, &protection);
        FlushInstructionCache(GetCurrentProcess(), slot, 32);
    }
    log_info("adm: %d OpenGL entry points sent to the loader's, %d without a wrapper", jumps, stubs);
}

static void replace(const char *name, void *function)
{
    void *address = findStaticFnAddr(name);
    if (address)
        detourFunction((size_t)address, function);
    else
        log_warn("adm: %s not found", name);
}

void admBridgeInstall(void)
{
    const uint8_t *getProcAddress = (const uint8_t *)findStaticFnAddr("admGetProcAddress");
    if (!getProcAddress)
    {
        log_error("adm: no admGetProcAddress, this program has no NVIDIA arcade driver");
        return;
    }
    fillGlSlots(getProcAddress);

    static const char *const answeredTrue[] = {"admvt_setup", "admvt_init", "admvt_open", "admvt_close", "admvt_register",
                                               "admvt_switch", "admvt_sync", "admGetNumDevices", "admFreeDevice",
                                               "admModeConfigi", "admDestroyScreen", "admDisplayScreen", "admDestroyContext",
                                               "admCopyContext", "admDestroyWindow", "admDestroyPbuffer", "admCursorAttribi",
                                               "admCursorLoadImage", "admWaitForVBlank", "admSuspend", "admResume", "admFree",
                                               "admGetMonitorAttribi", "admGetMonitorAttribf", "admSetMonitorGamma"};
    for (size_t i = 0; i < sizeof(answeredTrue) / sizeof(answeredTrue[0]); i++)
        replace(answeredTrue[i], admTrue);
    replace("admShutdown", admShutdown);
    replace("admGetString", admGetString);
    replace("admGetDeviceString", admGetString);
    replace("admInitDevicei", admInitDevicei);
    replace("admGetDeviceAttribi", admGetDeviceAttribi);
    replace("admChooseModeConfigi", admChooseModeConfigi);
    replace("admGetModeConfigAttribi", admGetModeConfigAttribi);
    replace("admChooseFBConfigi", admChooseFBConfigi);
    replace("admGetFBConfigAttribi", admGetFBConfigAttribi);
    replace("admCreateScreeni", admCreateScreeni);
    replace("admCreateGraphicsContext", admCreateGraphicsContext);
    replace("admCreateWindowi", admCreateWindowi);
    replace("admCreatePbufferi", admCreatePbufferi);
    replace("admMakeContextCurrent", admMakeContextCurrent);
    replace("admSwapInterval", admSwapInterval);
    replace("admGetSwapInterval", admGetSwapInterval);
    replace("admGetCurrentContext", admGetCurrentContext);
    replace("admGetCurrentDrawDrawable", admGetCurrentDrawable);
    replace("admGetCurrentReadDrawable", admGetCurrentDrawable);
    replace("admSwapBuffers", admSwapBuffers);
    replace("admGetDrawableAttribi", admGetDrawableAttribi);
    replace("admAllocateMemory", admAllocateMemory);
    replace("admFreeMemory", admFreeMemory);
    replace("admGetProcAddress", admGetProcAddress);
}
