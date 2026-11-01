#pragma once

// The pictures of the camera the loader gives Dead Heat and Maximum Heat 3D (v4l2Camera.c): the PC's camera, a still
// picture, or black, chosen by [Game.<short name>] CAMERA and CAMERA_PICTURE (cameraSource.cpp)

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Starts the pictures, width x height in YUYV (two pixels in four bytes: Y0 U Y1 V)
void cameraSourceStart(int width, int height);
// Copies the latest picture into yuyv (width * height * 2 bytes); 1 when it is another one than *serial, which it
// updates
int cameraSourceFrame(uint8_t *yuyv, unsigned *serial);
void cameraSourceStop(void);

#ifdef __cplusplus
}
#endif
