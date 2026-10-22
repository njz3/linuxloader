#pragma once

// Gaelco's PC-based games (Ring Riders, Tokyo Cop, Championship Tuning Race): their interface board (controls, motor,
// sound chip, RS485), reached through system calls of Gaelco's kernel, emulated where the game calls them
void gaelcoIoInstall(void);
