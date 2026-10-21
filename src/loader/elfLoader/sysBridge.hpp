#pragma once

#include <stddef.h>

// Linux system functions the Lindbergh games did not need and later ones do (Namco's Dead Heat): epoll, eventfd,
// pipes, users, interfaces, file system sizes, sysconf... sysBridge.cpp
namespace SysBridge
{
    void initBridges();
}

// The descriptors made here (eventfd, epoll) are numbers of the loader's own: read, write and close come here for them
extern "C" int sysVirtualFd(int fd);
extern "C" int sysVirtualRead(int fd, void *buffer, size_t count);
extern "C" int sysVirtualWrite(int fd, const void *buffer, size_t count);
extern "C" int sysVirtualClose(int fd);
