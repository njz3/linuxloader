/* unistd.h - MSVC stand-in for the MinGW header (see msvc_compat.h) */
#pragma once

#include <io.h>
#include <direct.h>
#include <process.h>
#include <stdlib.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* F_OK/R_OK/W_OK/X_OK come from msvc_compat.h, like MinGW's <io.h> */

#ifndef STDIN_FILENO
#define STDIN_FILENO 0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2
#endif

int usleep(useconds_t usec);
unsigned int sleep(unsigned int seconds);
int ftruncate(int fd, long length);

#ifdef __cplusplus
}
#endif
