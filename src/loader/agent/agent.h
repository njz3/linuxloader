#ifndef AGENT_H
#define AGENT_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// --agent / --agent-show: the game driven over stdin/stdout by another program, a test or a learning agent, frame
// by frame, in the protocol of sm2-emu's and Supermodel's agent modes (agent.c has it). --agent: no window shown,
// no pacing, no device touched. --agent-show: the same, the game in its window at its own pace.
void agentInit(int show);
int agentActive(void);
int agentHidden(void);

// Each frame of the game, its picture drawn in the framebuffer `fbo` of width x height: the agent's commands are
// served here, between two frames
void agentFrame(unsigned int fbo, int width, int height);

// The bytes the game writes to the drive board (driveBoard.c), for `step`'s answer and `servo`
void agentDriveboard(const unsigned char *bytes, size_t count);

#ifdef __cplusplus
}
#endif

#endif
