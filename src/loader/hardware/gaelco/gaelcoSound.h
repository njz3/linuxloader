#pragma once

// Gaelco's sound board, a sampler of 16 channels (gaelcoSound.c), fed by the calls gaelcoIo.c replaces
void gaelcoSoundLoad(const void *data, int length);
void gaelcoSoundCommand(int command, int channel, int first, int second);
void gaelcoSoundMasterVolume(float volume);
void gaelcoSoundBalance(const float outputs[4]);
int gaelcoSoundChannelsPlaying(void);
void gaelcoSoundStart(const int *params);
void gaelcoSoundStopAll(void);
