#include <SDL3/SDL.h>
#include <stdlib.h>
#include "../../../log/log.h"
#include "../../../agent/agent.h"

#define SND_PCM_STREAM_PLAYBACK 0
#define SND_PCM_STREAM_CAPTURE 1

#define SND_PCM_FORMAT_S8 0
#define SND_PCM_FORMAT_U8 1
#define SND_PCM_FORMAT_S16_LE 2
#define SND_PCM_FORMAT_S16_BE 3
#define SND_PCM_FORMAT_U16_LE 4
#define SND_PCM_FORMAT_U16_BE 5
#define SND_PCM_FORMAT_S32_LE 10
#define SND_PCM_FORMAT_FLOAT_LE 14

#define SND_PCM_ACCESS_RW_INTERLEAVED 3

#define SND_PCM_STATE_OPEN 0
#define SND_PCM_STATE_SETUP 1
#define SND_PCM_STATE_PREPARED 2
#define SND_PCM_STATE_RUNNING 3
#define SND_PCM_STATE_XRUN 4

typedef long snd_pcm_sframes_t;
typedef unsigned long snd_pcm_uframes_t;

struct _snd_pcm_hw_params
{
    int access;
    int format;
    int channels;
    unsigned int rate;
    unsigned int periods;
    snd_pcm_uframes_t buffer_size;
    snd_pcm_uframes_t period_size;
};

struct _snd_pcm_sw_params
{
    int dummy;
};

struct _snd_pcm
{
    SDL_AudioStream *stream;
    SDL_AudioSpec spec;
    int state;
    char name[64];
    snd_pcm_uframes_t buffer_size;
    int nonblock;     // SND_PCM_NONBLOCK: a full buffer answers -EAGAIN instead of waiting
    Uint64 drainedNs; // no device: the queue played out at the sound's pace, up to this time
};

typedef struct _snd_pcm snd_pcm_t;
typedef struct _snd_pcm_hw_params snd_pcm_hw_params_t;
typedef struct _snd_pcm_sw_params snd_pcm_sw_params_t;

static SDL_AudioDeviceID g_alsaDevice = 0;
static bool g_alsaPaused = false; // Tab settings open: snd_pcm_start must not resume the device

// Tab settings open: the game waits, and its sound with it
void alsaPause(int paused)
{
    g_alsaPaused = paused != 0;
    if (!g_alsaDevice)
        return;
    if (g_alsaPaused)
        SDL_PauseAudioDevice(g_alsaDevice);
    else
        SDL_ResumeAudioDevice(g_alsaDevice);
}

static void EnsureAudioInit()
{
    if (!SDL_WasInit(SDL_INIT_AUDIO))
    {
        SDL_Init(SDL_INIT_AUDIO);
    }
    if (g_alsaDevice == 0)
    {
        g_alsaDevice = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, NULL);
        if (g_alsaDevice == 0)
        {
            log_error("ALSA: Failed to open SDL audio device: %s", SDL_GetError());
        }
        else
        {
            log_info("ALSA: Opened SDL audio device ID %u", g_alsaDevice);
            if (agentHidden()) // --agent: the sound runs, silent
                SDL_SetAudioDeviceGain(g_alsaDevice, 0.0f);
        }
    }
}

// The playback device, shared with Gaelco's sound board (hardware/gaelco/gaelcoSound.c): paused by Tab and silent in
// --agent with the ALSA streams
unsigned int alsaSharedDevice(void)
{
    EnsureAudioInit();
    return g_alsaDevice;
}

int shared_snd_pcm_open(snd_pcm_t **pcm, const char *name, int stream, int mode)
{
    log_info("snd_pcm_open('%s', stream=%d, mode=%d)", name, stream, mode);

    EnsureAudioInit();

    if (stream != SND_PCM_STREAM_PLAYBACK)
    {
        log_error("ALSA: Only playback streams supported");
        return -22; // -EINVAL
    }

    snd_pcm_t *handle = (snd_pcm_t *)calloc(1, sizeof(snd_pcm_t));
    strncpy(handle->name, name, 63);

    handle->spec.format = SDL_AUDIO_S16LE;
    handle->spec.channels = 2;
    handle->spec.freq = 44100;
    handle->buffer_size = 4096;

    handle->stream = SDL_CreateAudioStream(&handle->spec, NULL);
    if (!handle->stream)
    {
        log_error("ALSA: Failed to create audio stream: %s", SDL_GetError());
        free(handle);
        return -1;
    }

    if (g_alsaDevice)
    {
        SDL_BindAudioStream(g_alsaDevice, handle->stream);
    }

    handle->state = SND_PCM_STATE_OPEN;
    handle->nonblock = (mode & 1) != 0; // SND_PCM_NONBLOCK
    handle->drainedNs = SDL_GetTicksNS();

    if (pcm)
        *pcm = handle;
    return 0;
}

int shared_snd_pcm_close(snd_pcm_t *pcm)
{
    log_info("snd_pcm_close(%p)", pcm);
    if (pcm)
    {
        if (pcm->stream)
        {
            SDL_DestroyAudioStream(pcm->stream);
        }
        free(pcm);
    }
    return 0;
}

// HW Params
int shared_snd_pcm_hw_params_malloc(snd_pcm_hw_params_t **ptr)
{
    if (ptr)
        *ptr = (snd_pcm_hw_params_t *)calloc(1, sizeof(snd_pcm_hw_params_t));
    return 0;
}

void shared_snd_pcm_hw_params_free(snd_pcm_hw_params_t *obj)
{
    free(obj);
}

int shared_snd_pcm_hw_params_any(snd_pcm_t *pcm, snd_pcm_hw_params_t *params)
{

    if (params)
    {
        params->access = SND_PCM_ACCESS_RW_INTERLEAVED;
        params->format = SND_PCM_FORMAT_S16_LE;
        params->channels = 2;
        params->rate = 44100;
        params->periods = 4;
        params->buffer_size = 4096;
        params->period_size = 1024;
    }
    return 0;
}

int shared_snd_pcm_hw_params_set_access(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, int access)
{
    if (params)
        params->access = access;
    return 0;
}

int shared_snd_pcm_hw_params_set_format(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, int val)
{
    if (params)
        params->format = val;
    return 0;
}

int shared_snd_pcm_hw_params_set_channels(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, int val)
{
    if (params)
        params->channels = val;
    return 0;
}

int shared_snd_pcm_hw_params_set_rate_near(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int *val, int *dir)
{
    if (params && val)
        params->rate = *val;
    return 0;
}

int shared_snd_pcm_hw_params_set_periods(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int val, int dir)
{
    if (params)
        params->periods = val;
    return 0;
}

int shared_snd_pcm_hw_params_set_buffer_size(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, snd_pcm_uframes_t val)
{
    if (params)
        params->buffer_size = val;
    return 0;
}

int shared_snd_pcm_hw_params_set_buffer_size_near(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned long *val)
{
    if (params && val)
        params->buffer_size = *val;
    return 0;
}

int shared_snd_pcm_hw_params_set_period_size_near(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned long *val, int *dir)
{
    if (params && val)
        params->period_size = *val;
    return 0;
}

int shared_snd_pcm_hw_params(snd_pcm_t *pcm, snd_pcm_hw_params_t *params)
{
    if (!pcm || !params)
        return -22; // EINVAL

    pcm->spec.freq = params->rate;
    pcm->spec.channels = params->channels;
    pcm->buffer_size = params->buffer_size;

    switch (params->format)
    {
        case SND_PCM_FORMAT_S8:
            pcm->spec.format = SDL_AUDIO_S8;
            break;
        case SND_PCM_FORMAT_U8:
            pcm->spec.format = SDL_AUDIO_U8;
            break;
        case SND_PCM_FORMAT_S16_LE:
            pcm->spec.format = SDL_AUDIO_S16LE;
            break;
        case SND_PCM_FORMAT_S16_BE:
            pcm->spec.format = SDL_AUDIO_S16BE;
            break;
        case SND_PCM_FORMAT_S32_LE: // Namco's sound driver (Dead Heat)
            pcm->spec.format = SDL_AUDIO_S32LE;
            break;
        case SND_PCM_FORMAT_FLOAT_LE:
            pcm->spec.format = SDL_AUDIO_F32LE;
            break;
        default:
            log_warn("ALSA: Unknown format %d, defaulting to S16LE", params->format);
            pcm->spec.format = SDL_AUDIO_S16LE;
            break;
    }

    log_info("ALSA: Configured %s: %d Hz, %d channels, fmt=%d, buf=%lu", pcm->name, pcm->spec.freq, pcm->spec.channels, pcm->spec.format,
             pcm->buffer_size);

    if (pcm->stream)
    {
        SDL_SetAudioStreamFormat(pcm->stream, &pcm->spec, NULL);
    }

    pcm->state = SND_PCM_STATE_PREPARED;
    return 0;
}

// SW Params
int shared_snd_pcm_sw_params_malloc(snd_pcm_sw_params_t **ptr)
{
    if (ptr)
        *ptr = (snd_pcm_sw_params_t *)calloc(1, sizeof(snd_pcm_sw_params_t));
    return 0;
}
void shared_snd_pcm_sw_params_free(snd_pcm_sw_params_t *obj)
{
    free(obj);
}

int shared_snd_pcm_sw_params_current(snd_pcm_t *pcm, snd_pcm_sw_params_t *params)
{
    return 0;
}
int shared_snd_pcm_sw_params_set_avail_min(snd_pcm_t *pcm, snd_pcm_sw_params_t *params, snd_pcm_uframes_t val)
{
    return 0;
}
int shared_snd_pcm_sw_params_set_start_threshold(snd_pcm_t *pcm, snd_pcm_sw_params_t *params, snd_pcm_uframes_t val)
{
    return 0;
}
int shared_snd_pcm_sw_params_set_xfer_align(snd_pcm_t *pcm, snd_pcm_sw_params_t *params, snd_pcm_uframes_t val)
{
    return 0;
}

int shared_snd_pcm_sw_params(snd_pcm_t *pcm, snd_pcm_sw_params_t *params)
{
    return 0;
}

// Operations
int shared_snd_pcm_prepare(snd_pcm_t *pcm)
{
    if (pcm)
    {
        if (pcm->stream)
            SDL_ClearAudioStream(pcm->stream);
        pcm->state = SND_PCM_STATE_PREPARED;
    }
    return 0;
}

int shared_snd_pcm_start(snd_pcm_t *pcm)
{
    if (pcm)
    {
        if (pcm->stream && !g_alsaPaused)
            SDL_ResumeAudioStreamDevice(pcm->stream);
        pcm->state = SND_PCM_STATE_RUNNING;
    }
    return 0;
}

int shared_snd_pcm_drop(snd_pcm_t *pcm)
{
    if (pcm)
    {
        if (pcm->stream)
            SDL_ClearAudioStream(pcm->stream);
        pcm->state = SND_PCM_STATE_SETUP;
    }
    return 0;
}

int shared_snd_pcm_resume(snd_pcm_t *pcm)
{
    return shared_snd_pcm_start(pcm);
}

int shared_snd_pcm_drain(snd_pcm_t *pcm)
{
    if (pcm && pcm->stream)
    {
        SDL_FlushAudioStream(pcm->stream);
    }
    return 0;
}

int shared_snd_pcm_delay(snd_pcm_t *pcm, snd_pcm_sframes_t *delayp)
{
    if (pcm && delayp)
    {

        if (pcm->stream)
        {
            int queued_bytes = SDL_GetAudioStreamQueued(pcm->stream);
            int frame_size = pcm->spec.channels * SDL_AUDIO_BYTESIZE(pcm->spec.format);
            if (frame_size > 0)
            {
                *delayp = queued_bytes / frame_size;
            }
            else
            {
                *delayp = 0;
            }
        }
        else
        {
            *delayp = 0;
        }
    }
    return 0;
}

// No device to play the stream (the agent's hidden mode, no audio device): what it would have played since the last
// call taken out of it, so that the writer is held to the sound's pace and the queue stays the buffer's size
static void drainWithoutDevice(snd_pcm_t *pcm)
{
    Uint64 now = SDL_GetTicksNS();
    if (g_alsaDevice || !pcm->stream || pcm->spec.freq <= 0)
    {
        pcm->drainedNs = now;
        return;
    }
    int frame_size = pcm->spec.channels * SDL_AUDIO_BYTESIZE(pcm->spec.format);
    Uint64 frames = (now - pcm->drainedNs) * (Uint64)pcm->spec.freq / 1000000000ull;
    if (frames == 0 || frame_size <= 0)
        return;
    pcm->drainedNs += frames * 1000000000ull / (Uint64)pcm->spec.freq;
    int queued = SDL_GetAudioStreamQueued(pcm->stream);
    Uint64 bytes = frames * (Uint64)frame_size;
    if (queued <= 0)
        return;
    if (bytes >= (Uint64)queued)
    {
        SDL_ClearAudioStream(pcm->stream);
        return;
    }
    static Uint8 sink[16384];
    while (bytes > 0)
    {
        int chunk = bytes > sizeof(sink) ? (int)sizeof(sink) : (int)bytes;
        int got = SDL_GetAudioStreamData(pcm->stream, sink, chunk);
        if (got <= 0)
            break;
        bytes -= (Uint64)got;
    }
}

static int framesQueued(snd_pcm_t *pcm, int frame_size)
{
    int queued = SDL_GetAudioStreamQueued(pcm->stream);
    return queued > 0 ? queued / frame_size : 0;
}

// As ALSA's: no more than the buffer holds; non-blocking, what fits or -EAGAIN; blocking, all of it as room comes
snd_pcm_sframes_t shared_snd_pcm_writei(snd_pcm_t *pcm, const void *buffer, unsigned long size)
{

    if (!pcm || !pcm->stream)
        return -32; // -EPIPE

    int frame_size = pcm->spec.channels * SDL_AUDIO_BYTESIZE(pcm->spec.format);
    if (frame_size <= 0)
        return -22; // EINVAL

    const Uint8 *data = (const Uint8 *)buffer;
    unsigned long written = 0;
    Uint64 start = SDL_GetTicks();
    while (written < size)
    {
        drainWithoutDevice(pcm);
        long room = (long)pcm->buffer_size - framesQueued(pcm, frame_size);
        if (room <= 0)
        {
            if (pcm->nonblock)
                return written ? (snd_pcm_sframes_t)written : -11; // -EAGAIN
            if (SDL_GetTicks() - start > 2000) // nothing plays it: what was taken
                return written ? (snd_pcm_sframes_t)written : -32;
            SDL_Delay(1);
            continue;
        }
        unsigned long chunk = size - written < (unsigned long)room ? size - written : (unsigned long)room;
        if (!SDL_PutAudioStreamData(pcm->stream, data + written * frame_size, (int)(chunk * frame_size)))
            return written ? (snd_pcm_sframes_t)written : -32;
        written += chunk;
    }
    return (snd_pcm_sframes_t)written;
}

int shared_snd_pcm_avail_update(snd_pcm_t *pcm)
{
    if (!pcm || !pcm->stream)
        return 4096;

    int frame_size = pcm->spec.channels * SDL_AUDIO_BYTESIZE(pcm->spec.format);
    if (frame_size <= 0)
        return 4096;

    drainWithoutDevice(pcm);
    int queued_bytes = SDL_GetAudioStreamQueued(pcm->stream);
    int queued_frames = queued_bytes / frame_size;

    int avail = (int)pcm->buffer_size - queued_frames;
    if (avail < 0)
        avail = 0;

    return avail;
}

int shared_snd_pcm_state(snd_pcm_t *pcm)
{
    if (pcm)
        return pcm->state;
    return SND_PCM_STATE_OPEN;
}

int shared_snd_pcm_format_width(int format)
{
    switch (format)
    {
        case SND_PCM_FORMAT_S8:
            return 8;
        case SND_PCM_FORMAT_U8:
            return 8;
        case SND_PCM_FORMAT_S16_LE:
            return 16;
        case SND_PCM_FORMAT_S16_BE:
            return 16;
        case SND_PCM_FORMAT_S32_LE:
        case SND_PCM_FORMAT_FLOAT_LE:
            return 32;
    }
    return 16;
}

size_t shared_snd_pcm_hw_params_sizeof(void)
{
    return sizeof(snd_pcm_hw_params_t);
}

size_t shared_snd_pcm_sw_params_sizeof(void)
{
    return sizeof(snd_pcm_sw_params_t);
}

int shared_snd_pcm_hw_params_set_rate(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int val, int dir)
{
    (void)pcm;
    (void)dir;
    if (params)
        params->rate = val;
    return 0;
}

int shared_snd_pcm_hw_params_test_rate(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int val, int dir)
{
    (void)pcm;
    (void)params;
    (void)val;
    (void)dir;
    return 0;
}

int shared_snd_pcm_nonblock(snd_pcm_t *pcm, int nonblock)
{
    if (pcm)
        pcm->nonblock = nonblock != 0;
    return 0;
}

// No capture device
long shared_snd_pcm_readi(snd_pcm_t *pcm, void *buffer, unsigned long size)
{
    (void)pcm;
    (void)buffer;
    (void)size;
    return -5; // -EIO
}

const char *shared_snd_asoundlib_version(void)
{
    return "1.0.27.2";
}

const char *shared_snd_strerror(int errnum)
{
    return "ALSA Error";
}

// --- what Namco's sound driver (Dead Heat's nsAdrv) asks more of ALSA

int shared_snd_pcm_hw_params_set_channels_max(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int *val)
{
    (void)pcm;
    if (params && val && params->channels > (int)*val)
        params->channels = (int)*val;
    return 0;
}

int shared_snd_pcm_hw_params_set_buffer_time_near(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int *val, int *dir)
{
    (void)pcm;
    (void)dir;
    if (params && val && params->rate)
        params->buffer_size = (snd_pcm_uframes_t)(((unsigned long long)params->rate * *val + 999999) / 1000000); // rounded up
    return 0;
}

int shared_snd_pcm_hw_params_set_period_time_near(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int *val, int *dir)
{
    (void)pcm;
    (void)dir;
    if (params && val && params->rate)
        params->period_size = (snd_pcm_uframes_t)(((unsigned long long)params->rate * *val + 999999) / 1000000); // rounded up
    return 0;
}

int shared_snd_pcm_hw_params_get_buffer_size(const snd_pcm_hw_params_t *params, snd_pcm_uframes_t *val)
{
    if (!params || !val)
        return -22;
    *val = params->buffer_size;
    return 0;
}

int shared_snd_pcm_hw_params_get_period_size(const snd_pcm_hw_params_t *params, snd_pcm_uframes_t *frames, int *dir)
{
    if (!params || !frames)
        return -22;
    *frames = params->period_size;
    if (dir)
        *dir = 0;
    return 0;
}

int shared_snd_pcm_sw_params_set_xrun_mode(snd_pcm_t *pcm, snd_pcm_sw_params_t *params, int val)
{
    (void)pcm;
    (void)params;
    (void)val;
    return 0;
}

// Ready when the buffer has room; 1 then, 0 after timeout milliseconds (negative: no limit)
int shared_snd_pcm_wait(snd_pcm_t *pcm, int timeout)
{
    Uint64 start = SDL_GetTicks();
    while (shared_snd_pcm_avail_update(pcm) <= 0)
    {
        if (timeout >= 0 && SDL_GetTicks() - start >= (Uint64)timeout)
            return 0;
        SDL_Delay(1);
    }
    return 1;
}

// A control device and a mixer with nothing in them: the volume loops find no element
static int dummyHandle;

int shared_snd_ctl_open(void **ctl, const char *name, int mode)
{
    (void)name;
    (void)mode;
    if (ctl)
        *ctl = &dummyHandle;
    return 0;
}

int shared_snd_ctl_close(void *ctl)
{
    (void)ctl;
    return 0;
}

size_t shared_snd_ctl_card_info_sizeof(void)
{
    return 512;
}

int shared_snd_ctl_card_info(void *ctl, void *info)
{
    (void)ctl;
    if (info)
        SDL_memset(info, 0, 512);
    return 0;
}

int shared_snd_mixer_open(void **mixer, int mode)
{
    (void)mode;
    if (mixer)
        *mixer = &dummyHandle;
    return 0;
}

int shared_snd_mixer_ok(void)
{
    return 0;
}

void *shared_snd_mixer_none(void)
{
    return NULL;
}

const char *shared_snd_mixer_selem_get_name(void *element)
{
    (void)element;
    return "";
}

int shared_snd_mixer_selem_get_volume_range(void *element, long *min, long *max)
{
    (void)element;
    if (min)
        *min = 0;
    if (max)
        *max = 100;
    return 0;
}
