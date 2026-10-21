/* sys/time.h - MSVC stand-in for the MinGW header (see msvc_compat.h) */
#pragma once

#include <winsock2.h> /* struct timeval */
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _TIMEZONE_DEFINED
#define _TIMEZONE_DEFINED
struct timezone
{
    int tz_minuteswest;
    int tz_dsttime;
};
#endif

int gettimeofday(struct timeval *tv, void *tz);

#ifdef __cplusplus
}
#endif
