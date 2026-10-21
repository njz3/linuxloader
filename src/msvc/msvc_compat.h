/*
 * msvc_compat.h - Force-included (/FI) in every MSVC translation unit of linuxloader.
 *
 * The Windows port was written against MinGW-w64/GCC. This header fills the gaps
 * MSVC leaves: GCC builtins/attributes and the POSIX bits MinGW's CRT provides.
 * The other headers in this folder (unistd.h, dirent.h, pthread.h, ...) stand in
 * for the MinGW headers of the same name. Implementations live in msvc_compat.c.
 */
#pragma once

#ifdef _MSC_VER

#ifndef _M_IX86
#error "linuxloader must be built for x86 (32-bit)"
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <intrin.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------------------
 * GCC builtins and attributes
 * ------------------------------------------------------------------------- */

/* cdecl is MSVC's default calling convention on x86 and the remaining attributes
   (naked, used, force_align_arg_pointer) are handled explicitly where they matter. */
#define __attribute__(x)

#define __builtin_return_address(level) _ReturnAddress()
#define __builtin_unreachable() __assume(0)
#define __builtin_expect(expr, value) (expr)

/* GCC defines __i386__ on 32-bit x86; some files test it. */
#ifndef __i386__
#define __i386__ 1
#endif

/* ---------------------------------------------------------------------------
 * Types MinGW's <sys/types.h> provides
 * ------------------------------------------------------------------------- */

#ifndef _SSIZE_T_DEFINED
#define _SSIZE_T_DEFINED
typedef int ssize_t;
#endif

#ifndef _PID_T_
#define _PID_T_
typedef int pid_t;
#endif

#ifndef _MODE_T_
#define _MODE_T_
typedef unsigned short mode_t;
#endif

typedef unsigned int useconds_t;

#ifndef PATH_MAX
#define PATH_MAX 260
#endif

/* MinGW's <io.h> defines these. _access() rejects any mode other than 0/2/4/6,
   so X_OK maps to an existence check. */
#ifndef F_OK
#define F_OK 0
#define X_OK 0
#define W_OK 2
#define R_OK 4
#endif

/* MinGW's <sys/stat.h> defines these */
#ifndef S_ISDIR
#define S_ISDIR(mode) (((mode) & 0xF000) == 0x4000) /* _S_IFMT, _S_IFDIR */
#define S_ISREG(mode) (((mode) & 0xF000) == 0x8000) /* _S_IFMT, _S_IFREG */
#endif

/* MinGW's <fcntl.h> defines O_ACCMODE */
#ifndef O_ACCMODE
#define O_ACCMODE 3 /* _O_RDONLY | _O_WRONLY | _O_RDWR */
#endif

/* ---------------------------------------------------------------------------
 * String / time functions MinGW exposes under POSIX names
 * ------------------------------------------------------------------------- */

#define strcasecmp _stricmp
#define strncasecmp _strnicmp
#define strtok_r strtok_s

/* Files are never > 2GB here; MinGW's fopen64 is plain fopen with 64-bit offsets. */
static __inline FILE *fopen64(const char *pathname, const char *mode)
{
    return fopen(pathname, mode);
}

/* Declared against incomplete types on purpose: <time.h> must not be pulled in
   here, because elfLoader/pthread defines its own Linux i386 struct timespec
   (two longs) and relies on being the first definition, as with MinGW. */
struct tm;
struct timespec;

#ifndef CLOCK_REALTIME
typedef int clockid_t;
#define CLOCK_REALTIME 0
#define CLOCK_MONOTONIC 1
#endif

struct tm *gmtime_r(const time_t *timer, struct tm *result);
struct tm *localtime_r(const time_t *timer, struct tm *result);
int clock_gettime(clockid_t clockId, struct timespec *tp);
int nanosleep(const struct timespec *request, struct timespec *remaining);

#ifdef __cplusplus
}
#endif

#endif /* _MSC_VER */
