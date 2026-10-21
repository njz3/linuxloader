/*
 * msvc_compat.c - POSIX functions MinGW-w64 provides and MSVC does not.
 * Declarations live in msvc_compat.h and the stand-in headers next to it.
 */
#ifdef _MSC_VER

#include <winsock2.h>
#include <windows.h>
#include <errno.h>
#include <io.h>
#include <string.h>

#include "dirent.h"
#include "libgen.h"
#include "pthread.h"
#include "sys/time.h"
#include "unistd.h"

/* ---------------------------------------------------------------------------
 * Time
 * ------------------------------------------------------------------------- */

struct tm *gmtime_r(const time_t *timer, struct tm *result)
{
    return gmtime_s(result, timer) == 0 ? result : NULL;
}

struct tm *localtime_r(const time_t *timer, struct tm *result)
{
    return localtime_s(result, timer) == 0 ? result : NULL;
}

int clock_gettime(clockid_t clockId, struct timespec *tp)
{
    if (!tp)
    {
        errno = EINVAL;
        return -1;
    }

    if (clockId == CLOCK_MONOTONIC)
    {
        static LARGE_INTEGER frequency;
        LARGE_INTEGER counter;
        if (frequency.QuadPart == 0)
            QueryPerformanceFrequency(&frequency);
        QueryPerformanceCounter(&counter);
        tp->tv_sec = (time_t)(counter.QuadPart / frequency.QuadPart);
        tp->tv_nsec = (long)((counter.QuadPart % frequency.QuadPart) * 1000000000LL / frequency.QuadPart);
        return 0;
    }

    if (clockId == CLOCK_REALTIME)
        return timespec_get(tp, TIME_UTC) == TIME_UTC ? 0 : -1;

    errno = EINVAL;
    return -1;
}

int gettimeofday(struct timeval *tv, void *tz)
{
    (void)tz;
    if (tv)
    {
        struct timespec ts;
        timespec_get(&ts, TIME_UTC);
        tv->tv_sec = (long)ts.tv_sec;
        tv->tv_usec = ts.tv_nsec / 1000;
    }
    return 0;
}

/* Sleep() only has millisecond granularity; use a high resolution waitable timer
   for sub-millisecond waits, like winpthreads' nanosleep does. */
static void sleepHundredNs(LONGLONG hundredNs)
{
    if (hundredNs <= 0)
    {
        Sleep(0);
        return;
    }

    HANDLE timer = CreateWaitableTimerExW(NULL, NULL, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
    if (!timer)
        timer = CreateWaitableTimerW(NULL, TRUE, NULL);
    if (!timer)
    {
        Sleep((DWORD)((hundredNs + 9999) / 10000));
        return;
    }

    LARGE_INTEGER due;
    due.QuadPart = -hundredNs; /* relative */
    SetWaitableTimer(timer, &due, 0, NULL, NULL, FALSE);
    WaitForSingleObject(timer, INFINITE);
    CloseHandle(timer);
}

int nanosleep(const struct timespec *request, struct timespec *remaining)
{
    if (!request || request->tv_nsec < 0 || request->tv_nsec >= 1000000000L)
    {
        errno = EINVAL;
        return -1;
    }
    sleepHundredNs((LONGLONG)request->tv_sec * 10000000LL + request->tv_nsec / 100);
    if (remaining)
    {
        remaining->tv_sec = 0;
        remaining->tv_nsec = 0;
    }
    return 0;
}

int usleep(useconds_t usec)
{
    sleepHundredNs((LONGLONG)usec * 10);
    return 0;
}

unsigned int sleep(unsigned int seconds)
{
    Sleep(seconds * 1000);
    return 0;
}

int ftruncate(int fd, long length)
{
    return _chsize(fd, length) == 0 ? 0 : -1;
}

/* ---------------------------------------------------------------------------
 * libgen.h
 * ------------------------------------------------------------------------- */

static int isSeparator(char c)
{
    return c == '/' || c == '\\';
}

char *basename(char *path)
{
    static char dot[] = ".";
    if (!path || !*path)
        return dot;

    char *end = path + strlen(path) - 1;
    while (end > path && isSeparator(*end))
        *end-- = '\0';

    char *base = end;
    while (base > path && !isSeparator(base[-1]))
        base--;
    return base;
}

char *dirname(char *path)
{
    static char dot[] = ".";
    if (!path || !*path)
        return dot;

    char *end = path + strlen(path) - 1;
    while (end > path && isSeparator(*end))
        end--;
    while (end >= path && !isSeparator(*end))
        end--;
    if (end < path)
        return dot;
    while (end > path && isSeparator(*end))
        end--;
    end[1] = '\0';
    return path;
}

/* ---------------------------------------------------------------------------
 * dirent.h
 * ------------------------------------------------------------------------- */

struct DIR
{
    HANDLE hFind;
    WIN32_FIND_DATAA findData;
    int first;
    struct dirent entry;
    char pattern[MAX_PATH + 3];
};

DIR *opendir(const char *name)
{
    if (!name || !*name)
    {
        errno = ENOENT;
        return NULL;
    }

    DIR *dir = (DIR *)calloc(1, sizeof(DIR));
    if (!dir)
    {
        errno = ENOMEM;
        return NULL;
    }

    size_t len = strlen(name);
    if (len > MAX_PATH)
    {
        free(dir);
        errno = ENAMETOOLONG;
        return NULL;
    }
    memcpy(dir->pattern, name, len);
    if (!isSeparator(name[len - 1]))
        dir->pattern[len++] = '\\';
    dir->pattern[len++] = '*';
    dir->pattern[len] = '\0';

    dir->hFind = FindFirstFileA(dir->pattern, &dir->findData);
    if (dir->hFind == INVALID_HANDLE_VALUE)
    {
        free(dir);
        errno = ENOENT;
        return NULL;
    }
    dir->first = 1;
    return dir;
}

struct dirent *readdir(DIR *dir)
{
    if (!dir)
        return NULL;

    if (!dir->first && !FindNextFileA(dir->hFind, &dir->findData))
        return NULL;
    dir->first = 0;

    strncpy(dir->entry.d_name, dir->findData.cFileName, sizeof(dir->entry.d_name) - 1);
    dir->entry.d_name[sizeof(dir->entry.d_name) - 1] = '\0';
    dir->entry.d_namlen = (unsigned short)strlen(dir->entry.d_name);
    dir->entry.d_reclen = sizeof(struct dirent);
    dir->entry.d_ino = 0;
    return &dir->entry;
}

void rewinddir(DIR *dir)
{
    if (!dir)
        return;
    FindClose(dir->hFind);
    dir->hFind = FindFirstFileA(dir->pattern, &dir->findData);
    dir->first = dir->hFind != INVALID_HANDLE_VALUE;
}

int closedir(DIR *dir)
{
    if (!dir)
    {
        errno = EBADF;
        return -1;
    }
    if (dir->hFind != INVALID_HANDLE_VALUE)
        FindClose(dir->hFind);
    free(dir);
    return 0;
}

/* ---------------------------------------------------------------------------
 * libgcc helpers. Linux ELFs import these from libgcc_s; the MinGW build gets
 * them from its own libgcc, MSVC has its own (_alldiv, ...) under other names.
 * ------------------------------------------------------------------------- */

long long __divdi3(long long a, long long b)
{
    return a / b;
}

long long __moddi3(long long a, long long b)
{
    return a % b;
}

unsigned long long __udivdi3(unsigned long long a, unsigned long long b)
{
    return a / b;
}

unsigned long long __umoddi3(unsigned long long a, unsigned long long b)
{
    return a % b;
}

unsigned long long __fixunsdfdi(double a)
{
    return (unsigned long long)a;
}

unsigned long long __fixunssfdi(float a)
{
    return (unsigned long long)a;
}

/* ---------------------------------------------------------------------------
 * pthread.h (mutexes only)
 * ------------------------------------------------------------------------- */

int pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr)
{
    (void)attr;
    InitializeSRWLock((PSRWLOCK)&mutex->lock);
    return 0;
}

int pthread_mutex_destroy(pthread_mutex_t *mutex)
{
    (void)mutex;
    return 0;
}

int pthread_mutex_lock(pthread_mutex_t *mutex)
{
    AcquireSRWLockExclusive((PSRWLOCK)&mutex->lock);
    return 0;
}

int pthread_mutex_trylock(pthread_mutex_t *mutex)
{
    return TryAcquireSRWLockExclusive((PSRWLOCK)&mutex->lock) ? 0 : EBUSY;
}

int pthread_mutex_unlock(pthread_mutex_t *mutex)
{
    ReleaseSRWLockExclusive((PSRWLOCK)&mutex->lock);
    return 0;
}

#endif /* _MSC_VER */
