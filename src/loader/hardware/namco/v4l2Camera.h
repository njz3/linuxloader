#pragma once

// Dead Heat and Maximum Heat 3D's camera, /dev/video0 as Linux's V4L2 gives it (v4l2Camera.c)

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// The game's camera is on: one of these games, CAMERA not OFF
bool v4l2CameraEnabled(void);
// open("/dev/video0"): the camera's descriptor, -1 when it is off
int v4l2CameraOpen(void);
bool v4l2CameraIs(int fd);
int v4l2CameraIoctl(int fd, unsigned long request, void *argument);
// mmap of the camera's descriptor: one of its buffers, by the offset QUERYBUF gave
void *v4l2CameraMmap(size_t length, long offset);
// munmap of one of its buffers (kept until the camera is closed)
bool v4l2CameraMunmap(void *address);
int v4l2CameraClose(int fd);

#ifdef __cplusplus
}
#endif
