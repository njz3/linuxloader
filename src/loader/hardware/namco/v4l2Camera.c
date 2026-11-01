// Dead Heat and Maximum Heat 3D's camera: a UVC webcam on /dev/video0, read by Namco's nmUVCCamera code through
// Linux's V4L2 (open O_RDWR | O_NONBLOCK; QUERYCAP, CROPCAP, S_CROP, S_FMT 640x480 YUYV, REQBUFS of at least two
// buffers, QUERYBUF and mmap of each, QBUF, STREAMON; then each frame DQBUF, its picture turned into RGB, QBUF; EAGAIN
// when no picture is ready). The device is answered here, its pictures from cameraSource.cpp: the PC's camera, a
// still picture or black ([Game.<short name>] CAMERA, CAMERA_PICTURE). CAMERA = OFF keeps the game's camera off, as
// the loader did before (deadHeatIo.c clears USE_CAMERA_DEVICE then).

#include "v4l2Camera.h"

#ifndef _WIN32
// Linux's own /dev/video0: the camera stays off, as before
bool v4l2CameraEnabled(void)
{
    return false;
}
#else
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <windows.h>

#include "cameraSource.h"
#include "../../config/config.h"
#include "../../config/gameOptions.h"
#include "../../log/log.h"

#define CAMERA_FD 0x7D000000 // a number of the loader's own (sockets from 900, sysBridge's from 0x7E000000)
#define WIDTH 640
#define HEIGHT 480
#define FRAME_SIZE (WIDTH * HEIGHT * 2)
// Each buffer a page more than a picture, as uvcvideo gives them: nmUVCCamera refuses one of 0x96000 bytes or less
#define BUFFER_LENGTH (FRAME_SIZE + 0x1000)
#define MAX_BUFFERS 4
#define BUFFER_SPACING 0x100000 // the offsets QUERYBUF gives, one per buffer
#define STILL_INTERVAL 100      // ms: a still picture given again, the camera's pictures as they come

// Linux's errno values, as the game reads them
#define L_EAGAIN 11
#define L_EINVAL 22

// The requests (32-bit Linux) and their structures' fields
#define VIDIOC_QUERYCAP 0x80685600
#define VIDIOC_S_FMT 0xC0CC5605
#define VIDIOC_REQBUFS 0xC0145608
#define VIDIOC_QUERYBUF 0xC0445609
#define VIDIOC_QBUF 0xC044560F
#define VIDIOC_DQBUF 0xC0445611
#define VIDIOC_STREAMON 0x40045612
#define VIDIOC_STREAMOFF 0x40045613
#define VIDIOC_CROPCAP 0xC02C563A
#define VIDIOC_S_CROP 0x4014563C
#define V4L2_PIX_FMT_YUYV 0x56595559
#define V4L2_FIELD_NONE 1
#define V4L2_COLORSPACE_SMPTE170M 1
#define V4L2_MEMORY_MMAP 1
#define V4L2_BUF_FLAG_MAPPED 0x1
#define V4L2_BUF_FLAG_QUEUED 0x2
#define V4L2_BUF_FLAG_DONE 0x4

static bool opened, streaming;
static uint8_t *buffers[MAX_BUFFERS];
static bool queued[MAX_BUFFERS];
static uint32_t bufferCount, sequence, next;
static unsigned serial;
static DWORD lastDelivery;
static unsigned delivered, refused, noBuffer;

static void put32(void *structure, size_t offset, uint32_t value)
{
    memcpy((uint8_t *)structure + offset, &value, 4);
}

static uint32_t get32(const void *structure, size_t offset)
{
    uint32_t value;
    memcpy(&value, (const uint8_t *)structure + offset, 4);
    return value;
}

static int fail(int error)
{
    errno = error;
    return -1;
}

bool v4l2CameraEnabled(void)
{
    return gameHasCamera() && _stricmp(getConfig()->camera, "OFF") != 0;
}

int v4l2CameraOpen(void)
{
    if (!v4l2CameraEnabled())
        return fail(2); // ENOENT
    if (opened)
        return fail(16); // EBUSY
    opened = true;
    streaming = false;
    cameraSourceStart(WIDTH, HEIGHT);
    return CAMERA_FD;
}

bool v4l2CameraIs(int fd)
{
    return opened && fd == CAMERA_FD;
}

// struct v4l2_buffer (0x44 bytes): index, type, bytesused, flags, field, timestamp (sec, usec), timecode (16),
// sequence at 0x2C, memory at 0x30, m.offset at 0x34, length at 0x38
static void describeBuffer(uint8_t *buffer, uint32_t index)
{
    put32(buffer, 0x00, index);
    put32(buffer, 0x04, 1); // V4L2_BUF_TYPE_VIDEO_CAPTURE
    put32(buffer, 0x08, FRAME_SIZE);
    put32(buffer, 0x0C, V4L2_BUF_FLAG_MAPPED | (queued[index] ? V4L2_BUF_FLAG_QUEUED : V4L2_BUF_FLAG_DONE));
    put32(buffer, 0x10, V4L2_FIELD_NONE);
    put32(buffer, 0x30, V4L2_MEMORY_MMAP);
    put32(buffer, 0x34, index * BUFFER_SPACING);
    put32(buffer, 0x38, BUFFER_LENGTH);
}

static int dequeue(uint8_t *buffer)
{
    if (!streaming)
        return fail(L_EINVAL);
    uint32_t index = bufferCount;
    for (uint32_t i = 0; i < bufferCount && index == bufferCount; i++)
        if (queued[(next + i) % bufferCount])
            index = (next + i) % bufferCount;
    if (index == bufferCount)
    {
        noBuffer++;
        return fail(L_EAGAIN);
    }
    // A new picture of the PC's camera, or the still picture again now and then
    DWORD now = GetTickCount();
    if (!cameraSourceFrame(buffers[index], &serial) && now - lastDelivery < STILL_INTERVAL)
    {
        refused++;
        return fail(L_EAGAIN);
    }
    delivered++;
    lastDelivery = now;
    queued[index] = false;
    next = (index + 1) % bufferCount;
    memset(buffer, 0, 0x44);
    describeBuffer(buffer, index);
    put32(buffer, 0x14, now / 1000);
    put32(buffer, 0x18, (now % 1000) * 1000);
    put32(buffer, 0x2C, sequence++);
    return 0;
}

static void freeBuffers(void)
{
    for (uint32_t i = 0; i < MAX_BUFFERS; i++)
    {
        if (buffers[i])
            VirtualFree(buffers[i], 0, MEM_RELEASE);
        buffers[i] = NULL;
        queued[i] = false;
    }
    bufferCount = 0;
}

int v4l2CameraIoctl(int fd, unsigned long request, void *argument)
{
    (void)fd;
    uint8_t *data = argument;
    if (request != VIDIOC_DQBUF && request != VIDIOC_QBUF)
        log_debug("camera: ioctl 0x%08lX", request);
    switch (request)
    {
    case VIDIOC_QUERYCAP: // struct v4l2_capability: driver, card, bus_info, version, capabilities, device_caps
        memset(data, 0, 0x68);
        strcpy((char *)data, "uvcvideo");
        strcpy((char *)data + 0x10, "linuxloader camera");
        strcpy((char *)data + 0x30, "usb-0000:00:1d.0-1");
        put32(data, 0x50, 0x00020620);
        put32(data, 0x54, 0x84000001); // VIDEO_CAPTURE, STREAMING, DEVICE_CAPS
        put32(data, 0x58, 0x04000001);
        return 0;
    case VIDIOC_CROPCAP: // struct v4l2_cropcap: type, bounds, defrect, pixelaspect
        put32(data, 0x04, 0);
        put32(data, 0x08, 0);
        put32(data, 0x0C, WIDTH);
        put32(data, 0x10, HEIGHT);
        put32(data, 0x14, 0);
        put32(data, 0x18, 0);
        put32(data, 0x1C, WIDTH);
        put32(data, 0x20, HEIGHT);
        put32(data, 0x24, 1);
        put32(data, 0x28, 1);
        return 0;
    case VIDIOC_S_CROP:
        return 0;
    case VIDIOC_S_FMT: // struct v4l2_format: type, then v4l2_pix_format; the camera's own is answered
        if (get32(data, 0) != 1)
            return fail(L_EINVAL);
        if (get32(data, 0x04) != WIDTH || get32(data, 0x08) != HEIGHT || get32(data, 0x0C) != V4L2_PIX_FMT_YUYV)
            log_warn("camera: the game asks %ux%u in %.4s, given %dx%d YUYV", get32(data, 0x04), get32(data, 0x08),
                     (const char *)data + 0x0C, WIDTH, HEIGHT);
        put32(data, 0x04, WIDTH);
        put32(data, 0x08, HEIGHT);
        put32(data, 0x0C, V4L2_PIX_FMT_YUYV);
        put32(data, 0x10, V4L2_FIELD_NONE);
        put32(data, 0x14, WIDTH * 2);
        put32(data, 0x18, FRAME_SIZE);
        put32(data, 0x1C, V4L2_COLORSPACE_SMPTE170M);
        return 0;
    case VIDIOC_REQBUFS: // struct v4l2_requestbuffers: count, type, memory
    {
        uint32_t count = get32(data, 0);
        if (get32(data, 0x08) != V4L2_MEMORY_MMAP)
            return fail(L_EINVAL);
        freeBuffers();
        if (count > MAX_BUFFERS)
            count = MAX_BUFFERS;
        for (uint32_t i = 0; i < count; i++)
        {
            buffers[i] = VirtualAlloc(NULL, BUFFER_LENGTH, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
            if (!buffers[i])
            {
                freeBuffers();
                return fail(12); // ENOMEM
            }
        }
        bufferCount = count;
        next = 0;
        put32(data, 0, count);
        return 0;
    }
    case VIDIOC_QUERYBUF:
    {
        uint32_t index = get32(data, 0);
        if (index >= bufferCount)
            return fail(L_EINVAL);
        describeBuffer(data, index);
        return 0;
    }
    case VIDIOC_QBUF:
    {
        uint32_t index = get32(data, 0);
        if (index >= bufferCount)
            return fail(L_EINVAL);
        queued[index] = true;
        put32(data, 0x0C, V4L2_BUF_FLAG_MAPPED | V4L2_BUF_FLAG_QUEUED);
        return 0;
    }
    case VIDIOC_DQBUF:
        return dequeue(data);
    case VIDIOC_STREAMON:
        if (!streaming)
            log_info("camera: the game starts the camera, %u buffers", (unsigned)bufferCount);
        streaming = true;
        lastDelivery = GetTickCount() - STILL_INTERVAL;
        return 0;
    case VIDIOC_STREAMOFF:
        log_info("camera: the game stops the camera, %u pictures given (%u asked too soon, %u without a buffer)",
                 delivered, refused, noBuffer);
        streaming = false;
        for (uint32_t i = 0; i < MAX_BUFFERS; i++)
            queued[i] = false;
        return 0;
    default:
        log_warn("camera: ioctl 0x%08lX not answered", request);
        return fail(L_EINVAL);
    }
}

void *v4l2CameraMmap(size_t length, long offset)
{
    uint32_t index = (uint32_t)offset / BUFFER_SPACING;
    if ((uint32_t)offset % BUFFER_SPACING || index >= bufferCount || length > BUFFER_LENGTH)
    {
        errno = L_EINVAL;
        return (void *)-1;
    }
    return buffers[index];
}

bool v4l2CameraMunmap(void *address)
{
    for (uint32_t i = 0; i < bufferCount; i++)
        if (address && address == buffers[i])
            return true;
    return false;
}

int v4l2CameraClose(int fd)
{
    (void)fd;
    log_info("camera: the game closes the camera");
    freeBuffers();
    opened = streaming = false;
    return 0;
}
#endif
