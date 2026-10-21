/* dirent.h - MSVC stand-in for the MinGW header (see msvc_compat.h) */
#pragma once

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

struct dirent
{
    long d_ino;
    unsigned short d_reclen;
    unsigned short d_namlen;
    char d_name[260];
};

typedef struct DIR DIR;

DIR *opendir(const char *name);
struct dirent *readdir(DIR *dir);
int closedir(DIR *dir);
void rewinddir(DIR *dir);

#ifdef __cplusplus
}
#endif
