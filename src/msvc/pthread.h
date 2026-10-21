/* pthread.h - minimal MSVC stand-in for winpthreads.
 *
 * The host side of the loader only needs plain mutexes; Linux games get their
 * pthreads from the emulation in elfLoader/pthread. Backed by SRWLOCK, so
 * PTHREAD_MUTEX_INITIALIZER works for static initialisation. Not recursive.
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    void *lock; /* SRWLOCK */
} pthread_mutex_t;

typedef struct
{
    int type;
} pthread_mutexattr_t;

#define PTHREAD_MUTEX_INITIALIZER {0}

int pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr);
int pthread_mutex_destroy(pthread_mutex_t *mutex);
int pthread_mutex_lock(pthread_mutex_t *mutex);
int pthread_mutex_trylock(pthread_mutex_t *mutex);
int pthread_mutex_unlock(pthread_mutex_t *mutex);

#ifdef __cplusplus
}
#endif
