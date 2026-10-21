/* libgen.h - MSVC stand-in for the MinGW header (see msvc_compat.h) */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* Like MinGW: may modify path, accepts both '/' and '\' separators. */
char *dirname(char *path);
char *basename(char *path);

#ifdef __cplusplus
}
#endif
