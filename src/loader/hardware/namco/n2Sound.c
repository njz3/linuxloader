// The sound of Namco's N2 games (Maximum Tune 3): the program has NVIDIA's OpenAL for the nForce sound chip built in,
// which drives the chip itself (its "Aos" layer, DeviceIoControl). Each of its al* and alc* functions jumps to the
// same function of OpenAL Soft (openal32.dll, beside the loader): the API is OpenAL's on both sides, cdecl, and the
// game's sounds are plain PCM (NVIDIA's ADPCM extension unused). The game opens the default device.
//
// With --agent (no device touched) the sound goes to OpenAL Soft's null output, or with LL_SOUND_WAV=<file> to that
// WAV file (to check it without listening).

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#include "n2Sound.h"
#include "../../agent/agent.h"
#include "../../log/log.h"
#include "../../patching/flowControl.h"

static const char *const functions[] = {
    "alBufferData", "alDeleteBuffers", "alDeleteSources", "alDisable", "alDistanceModel", "alDopplerFactor",
    "alDopplerVelocity", "alEnable", "alGenBuffers", "alGenSources", "alGetBoolean", "alGetBooleanv", "alGetBufferf",
    "alGetBufferi", "alGetDouble", "alGetDoublev", "alGetEnumValue", "alGetError", "alGetFloat", "alGetFloatv",
    "alGetInteger", "alGetIntegerv", "alGetListener3f", "alGetListenerf", "alGetListenerfv", "alGetListeneri",
    "alGetProcAddress", "alGetSource3f", "alGetSourcef", "alGetSourcefv", "alGetSourcei", "alGetString", "alIsBuffer",
    "alIsEnabled", "alIsExtensionPresent", "alIsSource", "alListener3f", "alListenerf", "alListenerfv", "alListeneri",
    "alSource3f", "alSourcePause", "alSourcePausev", "alSourcePlay", "alSourcePlayv", "alSourceQueueBuffers",
    "alSourceRewind", "alSourceRewindv", "alSourceStop", "alSourceStopv", "alSourceUnqueueBuffers", "alSourcef",
    "alSourcefv", "alSourcei", "alcCloseDevice", "alcCreateContext", "alcDestroyContext", "alcGetContextsDevice",
    "alcGetCurrentContext", "alcGetEnumValue", "alcGetError", "alcGetIntegerv", "alcGetProcAddress", "alcGetString",
    "alcIsExtensionPresent", "alcMakeContextCurrent", "alcOpenDevice", "alcProcessContext", "alcSuspendContext",
};

// NVIDIA's extensions: the channels' volume (left as it is), its ADPCM buffers (none of the game's sounds)
static void channelVolumeNothing(void)
{
}

static int bufferDataNone(void)
{
    log_warn("N2 sound: an NVIDIA ADPCM buffer, not played");
    return 0;
}

// OpenAL Soft's settings, in a configuration file of its own read after the user's. Its mixers in SSE keep their
// vectors on the stack with movdqa and expect it 16-byte aligned, which its mixing thread's is not always on Windows:
// now and then a fault ends the game. Its C mixers are used. With --agent, its null output, or its WAV writer to
// LL_SOUND_WAV.
static void openalSettings(void)
{
    char path[MAX_PATH];
    DWORD length = GetTempPathA(sizeof(path), path);
    if (!length || length > sizeof(path) - 32)
        return;
    strcat(path, "linuxloader-alsoft.ini");
    FILE *file = fopen(path, "w");
    if (!file)
        return;
    fprintf(file, "[general]\ndisable-cpu-exts = all\n");
    const char *wav = getenv("LL_SOUND_WAV");
    if (agentHidden() && wav)
    {
        fprintf(file, "drivers = wave\nchannels = stereo\nsample-type = int16\nfrequency = 48000\n\n[wave]\nfile = %s\n", wav);
        SetEnvironmentVariableA("ALSOFT_DRIVERS", "wave");
    }
    else if (agentHidden())
        SetEnvironmentVariableA("ALSOFT_DRIVERS", "null");
    fclose(file);
    SetEnvironmentVariableA("ALSOFT_CONF", path);
}

void n2SoundInstall(void)
{
    openalSettings();
    HMODULE openal = LoadLibraryA("openal32.dll");
    if (!openal)
    {
        log_error("N2 sound: no openal32.dll beside the loader, the game is silent");
        return;
    }
    int routed = 0;
    for (size_t i = 0; i < sizeof(functions) / sizeof(functions[0]); i++)
    {
        void *game = findStaticFnAddr(functions[i]);
        void *soft = (void *)GetProcAddress(openal, functions[i]);
        if (game && soft)
        {
            detourFunction((size_t)game, soft);
            routed++;
        }
    }
    void *game = findStaticFnAddr("alChannelVolumeNV");
    if (game)
        detourFunction((size_t)game, channelVolumeNothing);
    game = findStaticFnAddr("alBufferDataNV");
    if (game)
        detourFunction((size_t)game, bufferDataNone);
    log_info("N2 sound: %d OpenAL functions sent to OpenAL Soft", routed);
}
