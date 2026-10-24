// Gaelco's sound board (Ring Riders, Tokyo Cop, Championship Tuning Race): a sampler of 16 channels at 48 kHz.
//
// At start the game sends it its whole sample bank (roms.new, iogaelco_dump_sound_buffer): a table of headers of 32
// bytes, then the samples. A header (the games' sound.h, SHEADER): start (offset in the bank), Ns (the number of
// samples), two loop fields the board ignores, phinc (the step at the board's 48 kHz, 0x1000 = one sample a frame:
// 2730 for 32 kHz, 3763 for 44.1 kHz), Vnorm (the sample's gain, 0x4000 = 1), two ramp rates, flags (bits 1 and 2 the
// type: 8 bits, 16 bits, IMA ADPCM of 4 bits high nibble first, MP3 unused).
//
// The game drives the channels with commands (iogaelco_add_comm: command, channel, then two words), the numbers of
// sound.h: SC_SETUP the sample (bit 16 the loop, the whole sample), SC_START, SC_FREQ (0x0FFF = the sample's own
// pitch), SC_PAN (four outputs of 0 to 0x0FFF: 0 left, 1 right, 2 and 3 not heard on these cabinets), SC_VOL (the
// volume, 0x0FFF = 1, and in the high word the ramp's rate), SC_KOFF (a fade at a rate, the channels' mask in the high
// word), SC_STOP (a mask). The interface commands bring the master volume (0, a float: the game's volume setting
// squared, 0.25 at its default half), the balance of the four
// outputs (1), the playing channels' mask (2), sound_start (3, the older call) and the flush of the commands (4, they
// run at once here).
//
// LL_SOUND_WAV=<file>: what the board plays is also written to that WAV file (to check it without listening).

#include <SDL3/SDL.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gaelcoSound.h"
#include "../../agent/agent.h"
#include "../../log/log.h"

#ifdef _WIN32
unsigned int alsaSharedDevice(void); // alsa2sdl.c: paused by Tab with the other streams, silent in --agent
#endif

#define OUTPUT_RATE 48000
#define CHANNELS 16
#define PHASE_ONE 0x1000 // the phase in 1/4096 of a sample, as the board counts
#define FULL_SCALE 4095.0f

enum
{
    SC_SETUP = 1,
    SC_START,
    SC_FREQ,
    SC_PAN,
    SC_VOL,
    SC_KOFF,
    SC_STOP
};

enum
{
    TYPE_8BIT = 0,
    TYPE_16BIT = 2,
    TYPE_ADPCM = 4,
    TYPE_MASK = 6
};

#define SETUP_LOOP 0x10000
#define START_LOOP 0x00200000 // sound_start's flags
#define START_NOW 0x00400000

typedef struct
{
    uint32_t start;
    uint32_t count;
    uint32_t loopStart, loopEnd; // unused by the board
    uint16_t phinc;
    int16_t vnorm, rkon, rkoff, ramping, flags;
    int32_t pad;
} SampleHeader;

typedef struct
{
    bool active, loop, keyoff;
    const SampleHeader *header;
    uint64_t phase;
    uint32_t step;
    int freq; // SC_FREQ, 0x0FFF = the sample's pitch
    float volume, target, rate; // rate: the volume's change a frame
    float pan[4];
    // decoding, forward: decoded is the index of the last sample read, previous and current its last two values
    int64_t decoded;
    int previous, current;
    int predictor, stepIndex;
} Channel;

static uint8_t *bank;
static uint32_t bankSize, sampleCount;
static Channel channels[CHANNELS];
static float masterVolume = 1.0f, balance[4] = {1.0f, 1.0f, 1.0f, 1.0f};
static SDL_AudioStream *stream;
static FILE *wavFile;
static uint32_t wavBytes;

static const int adpcmSteps[89] = {
    7,     8,     9,     10,    11,    12,    13,    14,    16,    17,    19,    21,    23,    25,    28,    31,
    34,    37,    41,    45,    50,    55,    60,    66,    73,    80,    88,    97,    107,   118,   130,   143,
    157,   173,   190,   209,   230,   253,   279,   307,   337,   371,   408,   449,   494,   544,   598,   658,
    724,   796,   876,   963,   1060,  1166,  1282,  1411,  1552,  1707,  1878,  2066,  2272,  2499,  2749,  3024,
    3327,  3660,  4026,  4428,  4871,  5358,  5894,  6484,  7132,  7845,  8630,  9493,  10442, 11487, 12635, 13899,
    15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767};
static const int adpcmIndexChange[8] = {-1, -1, -1, -1, 2, 4, 6, 8};

// --- decoding

static void resetDecoding(Channel *c)
{
    c->decoded = -1;
    c->previous = c->current = 0;
    c->predictor = 0;
    c->stepIndex = 0;
}

static int readSample(Channel *c, int64_t index)
{
    const SampleHeader *h = c->header;
    uint32_t type = (uint16_t)h->flags & TYPE_MASK;
    if (type == TYPE_16BIT)
    {
        uint32_t at = h->start + (uint32_t)index * 2;
        return at + 2 <= bankSize ? (int16_t)(bank[at] | bank[at + 1] << 8) : 0;
    }
    if (type == TYPE_8BIT)
    {
        uint32_t at = h->start + (uint32_t)index;
        return at < bankSize ? (int8_t)bank[at] * 256 : 0;
    }
    // IMA ADPCM, in order only (the caller reads each index once)
    uint32_t at = h->start + (uint32_t)(index >> 1);
    int nibble = at < bankSize ? (bank[at] >> ((index & 1) ? 0 : 4)) & 0x0F : 0;
    int step = adpcmSteps[c->stepIndex];
    int difference = step >> 3;
    if (nibble & 1)
        difference += step >> 2;
    if (nibble & 2)
        difference += step >> 1;
    if (nibble & 4)
        difference += step;
    c->predictor += (nibble & 8) ? -difference : difference;
    c->predictor = c->predictor < -32768 ? -32768 : c->predictor > 32767 ? 32767 : c->predictor;
    c->stepIndex += adpcmIndexChange[nibble & 7];
    c->stepIndex = c->stepIndex < 0 ? 0 : c->stepIndex > 88 ? 88 : c->stepIndex;
    return c->predictor;
}

// The sample at index + 1 decoded, with the one before it
static void decodeTo(Channel *c, int64_t index)
{
    while (c->decoded < index)
    {
        c->decoded++;
        c->previous = c->current;
        c->current = readSample(c, c->decoded);
    }
}

static void updateStep(Channel *c)
{
    c->step = (uint32_t)((uint64_t)c->header->phinc * (uint32_t)(c->freq < 0 ? -c->freq : c->freq) / 0x0FFF);
}

// --- the mixer, on SDL's audio thread (the stream locked)

static void mix(float *out, int frames)
{
    memset(out, 0, sizeof(float) * 2 * frames);
    for (int n = 0; n < CHANNELS; n++)
    {
        Channel *c = &channels[n];
        if (!c->active)
            continue;
        uint32_t count = c->header->count;
        float gain = c->header->vnorm / 16384.0f * masterVolume / 32768.0f;
        float left = gain * c->pan[0] * balance[0], right = gain * c->pan[1] * balance[1];
        for (int i = 0; i < frames && c->active; i++)
        {
            int64_t index = (int64_t)(c->phase / PHASE_ONE);
            if ((uint64_t)index + 1 >= count)
            {
                if (!c->loop || count < 2)
                {
                    c->active = false;
                    break;
                }
                c->phase -= (uint64_t)(count - 1) * PHASE_ONE; // around, the decoder from the start again
                index = (int64_t)(c->phase / PHASE_ONE);
                resetDecoding(c);
            }
            decodeTo(c, index + 1);
            float fraction = (float)(c->phase % PHASE_ONE) / PHASE_ONE;
            float value = (c->previous + (c->current - c->previous) * fraction) * c->volume;
            out[2 * i] += value * left;
            out[2 * i + 1] += value * right;
            c->phase += c->step;
            if (c->volume != c->target)
            {
                float delta = c->target - c->volume;
                c->volume = (delta > 0 ? delta < c->rate : -delta < c->rate) ? c->target
                                                                             : c->volume + (delta > 0 ? c->rate : -c->rate);
            }
            else if (c->keyoff && c->volume <= 0.0f)
                c->active = false;
        }
    }
    for (int i = 0; i < 2 * frames; i++)
        out[i] = out[i] > 1.0f ? 1.0f : out[i] < -1.0f ? -1.0f : out[i];
}

static void writeWavHeader(void)
{
    uint8_t header[44];
    uint32_t values[] = {wavBytes + 36, 16, 1 | 2 << 16, OUTPUT_RATE, OUTPUT_RATE * 4, 4 | 16 << 16, wavBytes};
    memcpy(header, "RIFF", 4);
    memcpy(header + 4, &values[0], 4);
    memcpy(header + 8, "WAVEfmt ", 8);
    memcpy(header + 16, &values[1], 4);
    memcpy(header + 20, &values[2], 4);
    memcpy(header + 24, &values[3], 4);
    memcpy(header + 28, &values[4], 4);
    memcpy(header + 32, &values[5], 4);
    memcpy(header + 36, "data", 4);
    memcpy(header + 40, &values[6], 4);
    fseek(wavFile, 0, SEEK_SET);
    fwrite(header, 1, sizeof header, wavFile);
    fseek(wavFile, 0, SEEK_END);
}

static void SDLCALL feed(void *userdata, SDL_AudioStream *audio, int additional, int total)
{
    (void)userdata;
    (void)total;
    float buffer[2 * 512];
    while (additional > 0)
    {
        int frames = additional / (int)(2 * sizeof(float));
        frames = frames < 1 ? 1 : frames > 512 ? 512 : frames;
        mix(buffer, frames);
        SDL_PutAudioStreamData(audio, buffer, frames * 2 * (int)sizeof(float));
        additional -= frames * 2 * (int)sizeof(float);
        if (wavFile)
        {
            int16_t pcm[2 * 512];
            for (int i = 0; i < 2 * frames; i++)
                pcm[i] = (int16_t)(buffer[i] * 32767.0f);
            fwrite(pcm, sizeof(int16_t), 2 * frames, wavFile);
            wavBytes += frames * 4;
            if ((wavBytes & 0xFFFF) < (uint32_t)(frames * 4)) // the header kept true now and then
                writeWavHeader();
        }
    }
}

static void openOutput(void)
{
    if (stream)
        return;
    if (!SDL_WasInit(SDL_INIT_AUDIO))
        SDL_InitSubSystem(SDL_INIT_AUDIO);
    SDL_AudioSpec spec = {SDL_AUDIO_F32, 2, OUTPUT_RATE};
#ifdef _WIN32
    SDL_AudioDeviceID device = alsaSharedDevice();
#else
    SDL_AudioDeviceID device = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, NULL);
    if (device && agentHidden())
        SDL_SetAudioDeviceGain(device, 0.0f);
#endif
    stream = device ? SDL_CreateAudioStream(&spec, NULL) : NULL;
    if (!stream || !SDL_SetAudioStreamGetCallback(stream, feed, NULL) || !SDL_BindAudioStream(device, stream))
    {
        log_error("Gaelco sound: no audio output (%s)", SDL_GetError());
        return;
    }
    const char *wav = getenv("LL_SOUND_WAV");
    if (wav && (wavFile = fopen(wav, "wb")) != NULL)
        writeWavHeader();
    log_info("Gaelco sound: 16 channels at %d Hz", OUTPUT_RATE);
}

// --- the game's calls (its thread)

void gaelcoSoundLoad(const void *data, int length)
{
    if (!data || length < (int)sizeof(SampleHeader))
        return;
    openOutput();
    if (stream)
        SDL_LockAudioStream(stream);
    memset(channels, 0, sizeof channels);
    free(bank);
    bank = (uint8_t *)malloc((size_t)length);
    if (bank)
    {
        memcpy(bank, data, (size_t)length);
        bankSize = (uint32_t)length;
        uint32_t first;
        memcpy(&first, bank, 4);
        sampleCount = first / sizeof(SampleHeader);
        if ((uint64_t)sampleCount * sizeof(SampleHeader) > bankSize)
            sampleCount = 0;
    }
    if (stream)
        SDL_UnlockAudioStream(stream);
    log_info("Gaelco sound: bank of %d bytes, %u samples", length, sampleCount);
}

static void setup(Channel *c, int sample, bool loop)
{
    if (!bank || sample < 0 || (uint32_t)sample >= sampleCount)
    {
        c->header = NULL;
        c->active = false;
        return;
    }
    c->header = (const SampleHeader *)(bank + (size_t)sample * sizeof(SampleHeader));
    c->active = false;
    c->loop = loop;
    c->keyoff = false;
    c->freq = 0x0FFF;
    c->target = 1.0f;
    c->rate = 1.0f;
    for (int i = 0; i < 4; i++)
        c->pan[i] = 1.0f;
    updateStep(c);
}

static void start(Channel *c)
{
    if (!c->header || c->header->count == 0 || c->step == 0)
        return;
    c->phase = 0;
    c->volume = 0.0f; // raised to its volume at its rate: no click
    c->keyoff = false;
    resetDecoding(c);
    c->active = true;
}

// A rate of the board (0x0FFF = 1) as the volume's change a frame: 1 the whole range in a millisecond
static float frameRate(int rate)
{
    float r = (rate < 0 ? -rate : rate) / FULL_SCALE / (OUTPUT_RATE / 1000.0f);
    return r > 0.0f ? r : 1.0f / OUTPUT_RATE;
}

void gaelcoSoundCommand(int command, int channel, int first, int second)
{
    if (!stream)
        return;
    SDL_LockAudioStream(stream);
    Channel *c = channel >= 0 && channel < CHANNELS ? &channels[channel] : &channels[0];
    switch (command)
    {
        case SC_SETUP:
            setup(c, first & 0xFFFF, (first & SETUP_LOOP) != 0);
            break;
        case SC_START:
            start(c);
            break;
        case SC_FREQ:
            if (c->header)
            {
                c->freq = (int16_t)first;
                updateStep(c);
            }
            break;
        case SC_PAN:
            c->pan[0] = (first & 0xFFFF) / FULL_SCALE;
            c->pan[1] = ((first >> 16) & 0xFFFF) / FULL_SCALE;
            c->pan[2] = (second & 0xFFFF) / FULL_SCALE;
            c->pan[3] = ((second >> 16) & 0xFFFF) / FULL_SCALE;
            break;
        case SC_VOL:
            c->target = (int16_t)first / FULL_SCALE;
            c->target = c->target < 0.0f ? 0.0f : c->target;
            c->rate = frameRate((first >> 16) & 0xFFFF);
            break;
        case SC_KOFF:
            for (int n = 0; n < CHANNELS; n++)
                if ((first >> 16) & (1 << n))
                {
                    channels[n].target = 0.0f;
                    channels[n].rate = frameRate((int16_t)first);
                    channels[n].keyoff = true;
                }
            break;
        case SC_STOP:
            for (int n = 0; n < CHANNELS; n++)
                if (first & (1 << n))
                    channels[n].active = false;
            break;
    }
    SDL_UnlockAudioStream(stream);
}

void gaelcoSoundMasterVolume(float volume)
{
    masterVolume = volume < 0.0f ? 0.0f : volume;
}

void gaelcoSoundBalance(const float outputs[4])
{
    for (int i = 0; i < 4; i++)
        balance[i] = outputs[i];
}

int gaelcoSoundChannelsPlaying(void)
{
    int mask = 0;
    if (stream)
        SDL_LockAudioStream(stream);
    for (int n = 0; n < CHANNELS; n++)
        if (channels[n].active)
            mask |= 1 << n;
    if (stream)
        SDL_UnlockAudioStream(stream);
    return mask;
}

// sound_start(channel, sample, flags, the four outputs' levels as floats)
void gaelcoSoundStart(const int *params)
{
    if (!stream || !params)
        return;
    const float *levels = (const float *)params;
    int channel = params[0] >= 0 && params[0] < CHANNELS ? params[0] : 0;
    SDL_LockAudioStream(stream);
    Channel *c = &channels[channel];
    setup(c, params[1], (params[2] & START_LOOP) != 0);
    for (int i = 0; i < 4; i++)
        c->pan[i] = levels[3 + i] < 0.0f ? 0.0f : levels[3 + i] > 1.0f ? 1.0f : levels[3 + i];
    if (params[2] & START_NOW)
        start(c);
    SDL_UnlockAudioStream(stream);
}

void gaelcoSoundStopAll(void)
{
    if (!stream)
        return;
    SDL_LockAudioStream(stream);
    for (int n = 0; n < CHANNELS; n++)
        channels[n].active = false;
    SDL_UnlockAudioStream(stream);
}
