#if defined(_WIN32) || defined(__MINGW32__)

#include <winsock2.h>
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include <map>
#include <mutex>
#include <vector>

#include "sysBridge.hpp"
#include "filesystemBridge.hpp"
#include "libcBridge.hpp"
#include "symbolResolver.hpp"
#include "../log/log.h"
#include "../config/config.h"

#define MAP(name, func) SymbolResolver::GetInstance().RegisterVTable(name, reinterpret_cast<void *>(func))

// Linux's numbers where they differ from Windows'
#define LINUX_EAGAIN 11
#define LINUX_ENOSYS 38
#define LINUX_EPERM 1
#define LINUX_EINVAL 22
#define LINUX_AF_INET 2
#define LINUX_AF_INET6 10
#define LINUX_EPOLLIN 0x001
#define LINUX_EPOLLOUT 0x004
#define LINUX_EPOLL_CTL_ADD 1
#define LINUX_EPOLL_CTL_DEL 2
#define LINUX_EPOLL_CTL_MOD 3
#define LINUX_EFD_SEMAPHORE 1
#define LINUX_EFD_NONBLOCK 04000

// --- virtual descriptors: eventfd and epoll, numbers no CRT descriptor or socket reaches

#define VIRTUAL_FD_BASE 0x7E000000

#pragma pack(push, 1)
struct LinuxEpollEvent // packed on i386: 12 bytes
{
    uint32_t events;
    uint64_t data;
};
#pragma pack(pop)

struct EventFd
{
    uint64_t counter = 0;
    int flags = 0;
};

struct Epoll
{
    struct Entry
    {
        int fd;
        uint32_t events;
        uint64_t data;
    };
    std::vector<Entry> entries;
};

struct VirtualFd
{
    bool isEpoll = false;
    EventFd eventFd;
    Epoll epoll;
};

static std::mutex virtualLock;
static std::map<int, VirtualFd> virtualFds;
static int nextVirtualFd = VIRTUAL_FD_BASE;
static HANDLE eventFdChanged = NULL; // set at each eventfd write: an epoll_wait or a blocking read looks again

static int newVirtualFd(bool isEpoll)
{
    std::lock_guard<std::mutex> lock(virtualLock);
    if (!eventFdChanged)
        eventFdChanged = CreateEventA(NULL, TRUE, FALSE, NULL);
    int fd = nextVirtualFd++;
    virtualFds[fd].isEpoll = isEpoll;
    return fd;
}

extern "C" int sysVirtualFd(int fd)
{
    if (fd < VIRTUAL_FD_BASE)
        return 0;
    std::lock_guard<std::mutex> lock(virtualLock);
    return virtualFds.count(fd) != 0;
}

// An eventfd's read: its counter (or 1 for a semaphore), waiting for it unless non-blocking
extern "C" int sysVirtualRead(int fd, void *buffer, size_t count)
{
    if (count < 8)
    {
        errno = LINUX_EINVAL;
        return -1;
    }
    for (;;)
    {
        {
            std::lock_guard<std::mutex> lock(virtualLock);
            auto it = virtualFds.find(fd);
            if (it == virtualFds.end() || it->second.isEpoll)
            {
                errno = LINUX_EINVAL;
                return -1;
            }
            EventFd &e = it->second.eventFd;
            if (e.counter)
            {
                uint64_t value = (e.flags & LINUX_EFD_SEMAPHORE) ? 1 : e.counter;
                e.counter -= value;
                memcpy(buffer, &value, 8);
                return 8;
            }
            if (e.flags & LINUX_EFD_NONBLOCK)
            {
                errno = LINUX_EAGAIN;
                return -1;
            }
            ResetEvent(eventFdChanged);
        }
        WaitForSingleObject(eventFdChanged, 10);
    }
}

extern "C" int sysVirtualWrite(int fd, const void *buffer, size_t count)
{
    if (count < 8)
    {
        errno = LINUX_EINVAL;
        return -1;
    }
    std::lock_guard<std::mutex> lock(virtualLock);
    auto it = virtualFds.find(fd);
    if (it == virtualFds.end() || it->second.isEpoll)
    {
        errno = LINUX_EINVAL;
        return -1;
    }
    uint64_t value;
    memcpy(&value, buffer, 8);
    it->second.eventFd.counter += value;
    SetEvent(eventFdChanged);
    return 8;
}

extern "C" int sysVirtualClose(int fd)
{
    std::lock_guard<std::mutex> lock(virtualLock);
    for (auto &entry : virtualFds) // out of the epolls watching it
        if (entry.second.isEpoll)
        {
            auto &list = entry.second.epoll.entries;
            for (size_t i = 0; i < list.size();)
                list[i].fd == fd ? (void)list.erase(list.begin() + i) : (void)i++;
        }
    return virtualFds.erase(fd) ? 0 : -1;
}

extern "C" int bridgeEventfd(unsigned int initval, int flags)
{
    int fd = newVirtualFd(false);
    std::lock_guard<std::mutex> lock(virtualLock);
    virtualFds[fd].eventFd.counter = initval;
    virtualFds[fd].eventFd.flags = flags;
    return fd;
}

extern "C" int bridgeEpollCreate(int size)
{
    (void)size;
    return newVirtualFd(true);
}

extern "C" int bridgeEpollCtl(int epfd, int op, int fd, LinuxEpollEvent *event)
{
    std::lock_guard<std::mutex> lock(virtualLock);
    auto it = virtualFds.find(epfd);
    if (it == virtualFds.end() || !it->second.isEpoll)
    {
        errno = LINUX_EINVAL;
        return -1;
    }
    auto &list = it->second.epoll.entries;
    for (size_t i = 0; i < list.size(); i++)
        if (list[i].fd == fd)
        {
            if (op == LINUX_EPOLL_CTL_DEL)
                list.erase(list.begin() + i);
            else if (event)
            {
                list[i].events = event->events;
                list[i].data = event->data;
            }
            return 0;
        }
    if (op == LINUX_EPOLL_CTL_ADD && event)
        list.push_back({fd, event->events, event->data});
    return 0;
}

static bool isSocket(int fd)
{
    int type = 0, length = sizeof(type);
    return getsockopt((SOCKET)fd, SOL_SOCKET, SO_TYPE, (char *)&type, &length) == 0;
}

// What is ready among an epoll's descriptors: eventfds by their counter, sockets by select, pipes by what they hold
static int epollReady(int epfd, LinuxEpollEvent *events, int maxevents)
{
    std::vector<Epoll::Entry> list;
    {
        std::lock_guard<std::mutex> lock(virtualLock);
        auto it = virtualFds.find(epfd);
        if (it == virtualFds.end() || !it->second.isEpoll)
            return -1;
        list = it->second.epoll.entries;
    }
    int ready = 0;
    for (const Epoll::Entry &entry : list)
    {
        if (ready >= maxevents)
            break;
        uint32_t got = 0;
        if (entry.fd >= VIRTUAL_FD_BASE)
        {
            std::lock_guard<std::mutex> lock(virtualLock);
            auto it = virtualFds.find(entry.fd);
            if (it != virtualFds.end() && !it->second.isEpoll)
            {
                if (it->second.eventFd.counter)
                    got |= LINUX_EPOLLIN;
                got |= LINUX_EPOLLOUT;
            }
        }
        else if (isSocket(entry.fd))
        {
            fd_set readable, writable;
            FD_ZERO(&readable);
            FD_ZERO(&writable);
            FD_SET((SOCKET)entry.fd, &readable);
            FD_SET((SOCKET)entry.fd, &writable);
            timeval now = {0, 0};
            if (select(0, &readable, &writable, NULL, &now) > 0)
            {
                if (FD_ISSET((SOCKET)entry.fd, &readable))
                    got |= LINUX_EPOLLIN;
                if (FD_ISSET((SOCKET)entry.fd, &writable))
                    got |= LINUX_EPOLLOUT;
            }
        }
        else
        {
            HANDLE handle = (HANDLE)_get_osfhandle(entry.fd);
            DWORD available = 0;
            if (handle != INVALID_HANDLE_VALUE && GetFileType(handle) == FILE_TYPE_PIPE)
            {
                if (PeekNamedPipe(handle, NULL, 0, NULL, &available, NULL) && available)
                    got |= LINUX_EPOLLIN;
            }
            else if (handle != INVALID_HANDLE_VALUE)
                got |= LINUX_EPOLLIN;
            got |= LINUX_EPOLLOUT;
        }
        got &= entry.events | 0x18; // EPOLLERR and EPOLLHUP are always reported
        if (got)
        {
            events[ready].events = got;
            events[ready].data = entry.data;
            ready++;
        }
    }
    return ready;
}

extern "C" int bridgeEpollWait(int epfd, LinuxEpollEvent *events, int maxevents, int timeout)
{
    ULONGLONG start = GetTickCount64();
    for (;;)
    {
        int ready = epollReady(epfd, events, maxevents);
        if (ready != 0)
        {
            if (ready < 0)
                errno = LINUX_EINVAL;
            return ready;
        }
        if (timeout == 0 || (timeout > 0 && GetTickCount64() - start >= (ULONGLONG)timeout))
            return 0;
        WaitForSingleObject(eventFdChanged, 1);
    }
}

// --- pipes, files

extern "C" int bridgePipe(int fds[2])
{
    return _pipe(fds, 65536, _O_BINARY);
}

extern "C" int bridgeLink(const char *oldpath, const char *newpath)
{
    log_warn("link(%s, %s): not supported", oldpath, newpath);
    errno = LINUX_EPERM;
    return -1;
}

extern "C" int bridgeSymlink(const char *target, const char *linkpath)
{
    log_warn("symlink(%s, %s): not supported", target, linkpath);
    errno = LINUX_EPERM;
    return -1;
}

extern "C" long bridgePathconf(const char *path, int name)
{
    (void)path;
    switch (name)
    {
        case 3: // _PC_NAME_MAX
            return 255;
        case 4: // _PC_PATH_MAX
            return 4096;
        default:
            return -1;
    }
}

// i386 glibc's struct statvfs64: 96 bytes
#pragma pack(push, 4)
struct LinuxStatvfs64
{
    uint32_t f_bsize, f_frsize;
    uint64_t f_blocks, f_bfree, f_bavail;
    uint64_t f_files, f_ffree, f_favail;
    uint32_t f_fsid;
    int32_t unused;
    uint32_t f_flag, f_namemax;
    int32_t spare[6];
};
#pragma pack(pop)

extern "C" int bridgeStatvfs64(const char *path, LinuxStatvfs64 *buffer)
{
    (void)path;
    ULARGE_INTEGER available, total, free;
    if (!GetDiskFreeSpaceExA(NULL, &available, &total, &free))
        return -1;
    memset(buffer, 0, sizeof(*buffer));
    buffer->f_bsize = buffer->f_frsize = 4096;
    buffer->f_blocks = total.QuadPart / 4096;
    buffer->f_bfree = free.QuadPart / 4096;
    buffer->f_bavail = available.QuadPart / 4096;
    buffer->f_files = buffer->f_ffree = buffer->f_favail = 1000000;
    buffer->f_namemax = 255;
    return 0;
}

extern "C" struct linux_dirent64 *bridgeReaddir64(void *dirp)
{
    static struct linux_dirent64 entry;
    struct linux_dirent64 *result = NULL;
    bridgeReaddir64R(dirp, &entry, &result);
    return result;
}

// --- time

extern "C" long bridgeClock(void)
{
    return (long)clock() * (1000000 / CLOCKS_PER_SEC); // Linux counts microseconds
}

extern "C" tm32 *bridgeGmtime(const time_t *timep)
{
    static tm32 result;
    return LibcBridge::bridgeGmtime_R(timep, &result);
}

extern "C" time_t bridgeTimegm(tm32 *tm)
{
    struct tm t;
    memset(&t, 0, sizeof(t));
    memcpy(&t, tm, 9 * sizeof(int)); // the nine fields both have, in the same order
    return _mkgmtime(&t);
}

// --- processes, threads, users

extern "C" int bridgeAbs(int value)
{
    return value < 0 ? -value : value;
}

static int processors(void)
{
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    return (int)info.dwNumberOfProcessors;
}

extern "C" int bridgeGetNprocs(void)
{
    return processors();
}

extern "C" long bridgeSysconf(int name)
{
    switch (name)
    {
        case 2: // _SC_CLK_TCK
            return 100;
        case 4: // _SC_OPEN_MAX
            return 1024;
        case 30: // _SC_PAGESIZE
            return 4096;
        case 67: // _SC_THREADS
        case 68: // _SC_THREAD_SAFE_FUNCTIONS: boost::filesystem then takes readdir_r
            return 200809L;
        case 69: // _SC_GETGR_R_SIZE_MAX
        case 70: // _SC_GETPW_R_SIZE_MAX
            return 1024;
        case 71: // _SC_LOGIN_NAME_MAX
            return 256;
        case 72: // _SC_TTY_NAME_MAX
            return 32;
        case 73: // _SC_THREAD_DESTRUCTOR_ITERATIONS
            return 4;
        case 74: // _SC_THREAD_KEYS_MAX
            return 1024;
        case 75: // _SC_THREAD_STACK_MIN
            return 16384;
        case 83: // _SC_NPROCESSORS_CONF
        case 84: // _SC_NPROCESSORS_ONLN
            return processors();
        case 85: // _SC_PHYS_PAGES
        case 86: // _SC_AVPHYS_PAGES
        {
            MEMORYSTATUSEX memory;
            memory.dwLength = sizeof(memory);
            GlobalMemoryStatusEx(&memory);
            return (long)((name == 85 ? memory.ullTotalPhys : memory.ullAvailPhys) / 4096);
        }
        default:
            log_warn("sysconf(%d): unknown", name);
            errno = LINUX_EINVAL;
            return -1;
    }
}

extern "C" long bridgeSyscall(long number, ...)
{
    switch (number)
    {
        case 224: // gettid
            return (long)GetCurrentThreadId();
        case 158: // sched_yield
            SwitchToThread();
            return 0;
        default:
            log_warn("syscall(%ld): not supported", number);
            errno = LINUX_ENOSYS;
            return -1;
    }
}

extern "C" int bridgeGetgid(void)
{
    return 1000;
}

extern "C" int bridgeMlockall(int flags)
{
    (void)flags;
    return 0;
}

extern "C" int bridgePthreadKill(void *thread, int sig)
{
    (void)thread;
    if (sig)
        log_warn("pthread_kill(%d): ignored", sig);
    return 0;
}

extern "C" int bridgePthreadSigmask(int how, const void *set, void *oldset)
{
    (void)how;
    (void)set;
    if (oldset)
        memset(oldset, 0, 128); // sigset_t
    return 0;
}

struct LinuxPasswd
{
    char *pw_name, *pw_passwd;
    uint32_t pw_uid, pw_gid;
    char *pw_gecos, *pw_dir, *pw_shell;
};

extern "C" int bridgeGetpwuidR(uint32_t uid, LinuxPasswd *pwd, char *buffer, size_t length, LinuxPasswd **result)
{
    static const char strings[] = "arcade\0x\0arcade\0/home/arcade\0/bin/sh";
    *result = NULL;
    if (length < sizeof(strings))
        return 34; // ERANGE
    memcpy(buffer, strings, sizeof(strings));
    pwd->pw_name = buffer;
    pwd->pw_passwd = buffer + 7;
    pwd->pw_uid = uid;
    pwd->pw_gid = 1000;
    pwd->pw_gecos = buffer + 9;
    pwd->pw_dir = buffer + 16;
    pwd->pw_shell = buffer + 29;
    *result = pwd;
    return 0;
}

// --- message catalogs: none, the messages given are used

extern "C" void *bridgeCatopen(const char *name, int flag)
{
    (void)name;
    (void)flag;
    return (void *)-1;
}

extern "C" char *bridgeCatgets(void *catalog, int set, int number, char *message)
{
    (void)catalog;
    (void)set;
    (void)number;
    return message;
}

extern "C" char *bridgeStrerrorR(int errnum, char *buffer, size_t length) // GNU's: the text is returned
{
    if (buffer && length)
    {
        strncpy(buffer, strerror(errnum), length - 1);
        buffer[length - 1] = '\0';
    }
    return buffer;
}

// --- network

extern "C" void networkFromThisPc(void *address); // networkBridge.cpp: this PC as the game's own address

extern "C" int bridgeGetpeername(int fd, struct sockaddr *address, int *length)
{
    if (getpeername((SOCKET)fd, address, length) != 0)
        return -1;
    networkFromThisPc(address);
    return 0;
}

// --- libusb 1.0: Dead Heat Riders' USB key, the only device, found by its ids and the serial number string it reads
// (clHASP::Check: 0B9A:0C10, a serial beginning "2706", then "2", then "01" at the seventh character); for any
// other game, a bus without any device
static int usbContext;
static int usbKey; // the key's device and its handle

static bool usbKeyPresent(void)
{
    return getConfig()->crc32 == DEAD_HEAT_RIDERS;
}

extern "C" int bridgeLibusbInit(void **context)
{
    if (context)
        *context = &usbContext;
    return 0;
}

extern "C" void bridgeLibusbExit(void *context)
{
    (void)context;
}

extern "C" int bridgeLibusbGetDeviceList(void *context, void ***list)
{
    (void)context;
    static void *none[1] = {NULL};
    static void *key[2] = {&usbKey, NULL};
    if (list)
        *list = usbKeyPresent() ? key : none;
    return usbKeyPresent() ? 1 : 0;
}

extern "C" int bridgeLibusbGetDeviceDescriptor(void *device, uint8_t *descriptor)
{
    if (device != &usbKey)
        return -4; // LIBUSB_ERROR_NO_DEVICE
    memset(descriptor, 0, 18);
    descriptor[0] = 18;                         // bLength
    descriptor[1] = 1;                          // bDescriptorType: device
    *(uint16_t *)(descriptor + 2) = 0x0200;     // bcdUSB
    descriptor[7] = 8;                          // bMaxPacketSize0
    *(uint16_t *)(descriptor + 8) = 0x0B9A;     // idVendor
    *(uint16_t *)(descriptor + 10) = 0x0C10;    // idProduct
    descriptor[16] = 3;                         // iSerialNumber
    descriptor[17] = 1;                         // bNumConfigurations
    return 0;
}

extern "C" int bridgeLibusbOpen(void *device, void **handle)
{
    if (device != &usbKey)
        return -4;
    *handle = &usbKey;
    return 0;
}

extern "C" int bridgeLibusbGetStringDescriptorAscii(void *handle, uint8_t index, char *data, int length)
{
    static const char serial[] = "270620010001";
    if (handle != &usbKey || index != 3 || length <= 0)
        return -4;
    int count = length - 1 < (int)sizeof(serial) - 1 ? length - 1 : (int)sizeof(serial) - 1;
    memcpy(data, serial, count);
    data[count] = '\0';
    log_info("libusb: Dead Heat Riders' key, serial %s", serial);
    return count;
}

extern "C" void bridgeLibusbClose(void *handle)
{
    (void)handle;
}

extern "C" unsigned int bridgeIfNametoindex(const char *name)
{
    (void)name;
    return 1;
}

extern "C" char *bridgeIfIndextoname(unsigned int index, char *name)
{
    (void)index;
    strcpy(name, "eth0");
    return name;
}

extern "C" const char *bridgeInetNtop(int family, const void *source, char *destination, unsigned int size)
{
    char text[64];
    const uint8_t *b = (const uint8_t *)source;
    if (family == LINUX_AF_INET)
        snprintf(text, sizeof(text), "%u.%u.%u.%u", b[0], b[1], b[2], b[3]);
    else if (family == LINUX_AF_INET6)
    {
        int n = 0;
        for (int i = 0; i < 16; i += 2)
            n += snprintf(text + n, sizeof(text) - n, i ? ":%x" : "%x", (b[i] << 8) | b[i + 1]);
    }
    else
    {
        errno = 97; // EAFNOSUPPORT
        return NULL;
    }
    if (strlen(text) + 1 > size)
    {
        errno = 28; // ENOSPC
        return NULL;
    }
    strcpy(destination, text);
    return destination;
}

namespace SysBridge
{
    void initBridges()
    {
        MAP("epoll_create", bridgeEpollCreate);
        MAP("epoll_ctl", bridgeEpollCtl);
        MAP("epoll_wait", bridgeEpollWait);
        MAP("eventfd", bridgeEventfd);
        MAP("pipe", bridgePipe);
        MAP("link", bridgeLink);
        MAP("symlink", bridgeSymlink);
        MAP("pathconf", bridgePathconf);
        MAP("statvfs64", bridgeStatvfs64);
        MAP("readdir64", bridgeReaddir64);
        MAP("clock", bridgeClock);
        MAP("gmtime", bridgeGmtime);
        MAP("timegm", bridgeTimegm);
        MAP("abs", bridgeAbs);
        MAP("get_nprocs", bridgeGetNprocs);
        MAP("sysconf", bridgeSysconf);
        MAP("syscall", bridgeSyscall);
        MAP("getgid", bridgeGetgid);
        MAP("mlockall", bridgeMlockall);
        MAP("munlockall", bridgeMlockall);
        MAP("pthread_kill", bridgePthreadKill);
        MAP("pthread_sigmask", bridgePthreadSigmask);
        MAP("getpwuid_r", bridgeGetpwuidR);
        MAP("catopen", bridgeCatopen);
        MAP("catgets", bridgeCatgets);
        MAP("strerror_r", bridgeStrerrorR);
        MAP("getpeername", bridgeGetpeername);
        MAP("if_nametoindex", bridgeIfNametoindex);
        MAP("libusb_init", bridgeLibusbInit);
        MAP("libusb_exit", bridgeLibusbExit);
        MAP("libusb_get_device_list", bridgeLibusbGetDeviceList);
        MAP("libusb_get_device_descriptor", bridgeLibusbGetDeviceDescriptor);
        MAP("libusb_open", bridgeLibusbOpen);
        MAP("libusb_get_string_descriptor_ascii", bridgeLibusbGetStringDescriptorAscii);
        MAP("libusb_close", bridgeLibusbClose);
        MAP("if_indextoname", bridgeIfIndextoname);
        MAP("inet_ntop", bridgeInetNtop);
    }
}

#endif
