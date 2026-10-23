// The real serial ports a board is passed to (serialPort.h). On Windows the game's own serial port, opened by name
// in Linux's way and set with Linux's termios, becomes a COM port: its speed, data size, stop bits, parity and flow
// control from the game's tcsetattr, its reads as VMIN, VTIME and O_NONBLOCK would make them (COMMTIMEOUTS), select
// and FIONREAD from the bytes waiting. On Linux the game opens the device itself and termios is the system's.

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "serialPort.h"
#include "../../log/log.h"

#ifdef _WIN32
#include <errno.h>
#include <fcntl.h>
#include <io.h>
#include <windows.h>
#else
#include <dirent.h>
#endif

typedef struct
{
    char name[SERIAL_PORT_NAME];
    int number;
} PortEntry;

static int comparePorts(const void *a, const void *b)
{
    const PortEntry *left = a, *right = b;
    int byName = strncmp(left->name, right->name, strcspn(left->name, "0123456789"));
    return byName ? byName : left->number - right->number;
}

static int sortedList(PortEntry *entries, int count, char names[][SERIAL_PORT_NAME], int max)
{
    qsort(entries, count, sizeof(PortEntry), comparePorts);
    int listed = count < max ? count : max;
    for (int i = 0; i < listed; i++)
        snprintf(names[i], SERIAL_PORT_NAME, "%s", entries[i].name);
    return listed;
}

#ifdef _WIN32

// "COM12" or "\\.\COM12": the port's number, else 0
static int comNumber(const char *path)
{
    if (strncmp(path, "\\\\.\\", 4) == 0)
        path += 4;
    if (_strnicmp(path, "COM", 3) != 0 || !isdigit((unsigned char)path[3]))
        return 0;
    char *end;
    long number = strtol(path + 3, &end, 10);
    return *end == '\0' || *end == ':' ? (int)number : 0;
}

bool serialPortName(const char *path, char *name, size_t size)
{
    int number = path ? comNumber(path) : 0;
    if (number <= 0)
        return false;
    snprintf(name, size, "COM%d", number);
    return true;
}

int serialPortList(char names[][SERIAL_PORT_NAME], int max)
{
    DWORD size = 65536;
    char *devices = NULL;
    for (;;)
    {
        devices = realloc(devices, size);
        if (!devices || QueryDosDeviceA(NULL, devices, size))
            break;
        if (GetLastError() != ERROR_INSUFFICIENT_BUFFER || size >= (1u << 24))
        {
            free(devices);
            return 0;
        }
        size *= 2;
    }
    if (!devices)
        return 0;
    PortEntry entries[256];
    int count = 0;
    for (const char *device = devices; *device && count < 256; device += strlen(device) + 1)
    {
        int number = comNumber(device);
        if (number <= 0)
            continue;
        snprintf(entries[count].name, SERIAL_PORT_NAME, "COM%d", number);
        entries[count++].number = number;
    }
    free(devices);
    return sortedList(entries, count, names, max);
}

// Linux's termios as the game sets it
#define LINUX_O_NONBLOCK 04000
#define LINUX_EAGAIN 11
#define LINUX_CBAUD 0x100F
#define LINUX_CSIZE 0x30
#define LINUX_CSTOPB 0x40
#define LINUX_PARENB 0x100
#define LINUX_PARODD 0x200
#define LINUX_CRTSCTS 0x80000000u

typedef struct
{
    bool used;
    int fd;
    HANDLE handle;      // overlapped: a read waiting in one thread does not hold the writes of another (JVS's reader)
    HANDLE readDone, writeDone;
    char name[SERIAL_PORT_NAME];
    int flags;
    bool nonBlocking;
    int vmin, vtime;
} SerialPort;

#define MAX_PORTS 8
static SerialPort ports[MAX_PORTS];

static SerialPort *findPort(int fd)
{
    for (int i = 0; i < MAX_PORTS; i++)
        if (ports[i].used && ports[i].fd == fd)
            return &ports[i];
    return NULL;
}

static SerialPort *freePort(void)
{
    for (int i = 0; i < MAX_PORTS; i++)
        if (!ports[i].used)
            return &ports[i];
    return NULL;
}

bool serialPortIs(int fd)
{
    return findPort(fd) != NULL;
}

// A read returns what is there (O_NONBLOCK, or VMIN and VTIME 0), waits VTIME tenths of a second for the first byte
// (VMIN 0), or waits for the first byte (VMIN above 0: Windows hands over what came, which may be less than VMIN)
static void applyTimeouts(SerialPort *port)
{
    COMMTIMEOUTS timeouts = {0};
    timeouts.ReadIntervalTimeout = MAXDWORD;
    if (!port->nonBlocking && (port->vmin > 0 || port->vtime > 0))
    {
        timeouts.ReadTotalTimeoutMultiplier = MAXDWORD;
        timeouts.ReadTotalTimeoutConstant = port->vmin > 0 ? MAXDWORD - 1 : (DWORD)port->vtime * 100;
    }
    timeouts.WriteTotalTimeoutConstant = 1000;
    if (!SetCommTimeouts(port->handle, &timeouts))
        log_warn("%s: SetCommTimeouts failed (%lu)", port->name, GetLastError());
}

static DWORD baudRate(unsigned int speed)
{
    static const struct
    {
        unsigned int code;
        DWORD rate;
    } codes[] = {{7, 600},       {9, 1200},       {11, 2400},      {12, 4800},      {13, 9600},
                 {14, 19200},    {15, 38400},     {0x1001, 57600}, {0x1002, 115200}, {0x1003, 230400},
                 {0x1004, 460800}, {0x1005, 500000}, {0x1006, 576000}, {0x1007, 921600}};
    for (size_t i = 0; i < sizeof(codes) / sizeof(codes[0]); i++)
        if (codes[i].code == speed)
            return codes[i].rate;
    return speed >= 300 ? speed : 0; // a rate written as it is (the bridge's default: 115200)
}

int serialPortOpen(const char *path, int linuxFlags)
{
    SerialPort *port = freePort();
    char name[SERIAL_PORT_NAME], device[SERIAL_PORT_NAME + 4];
    if (!port || !serialPortName(path, name, sizeof(name)))
    {
        // The settings' Linux default (/dev/ttyS0) when no real board was chosen: the port stays closed, as before
        log_info("Serial passthrough: %s is not a COM port, the port stays closed", path ? path : "(none)");
        return -1;
    }
    snprintf(device, sizeof(device), "\\\\.\\%s", name);
    HANDLE handle = CreateFileA(device, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
    if (handle == INVALID_HANDLE_VALUE)
    {
        log_error("Serial passthrough: %s does not open (%lu)", name, GetLastError());
        return -1;
    }
    int fd = _open_osfhandle((intptr_t)handle, _O_BINARY);
    if (fd < 0)
    {
        CloseHandle(handle);
        return -1;
    }
    SetupComm(handle, 4096, 4096);
    port->used = true;
    port->fd = fd;
    port->handle = handle;
    port->readDone = CreateEventA(NULL, TRUE, FALSE, NULL);
    port->writeDone = CreateEventA(NULL, TRUE, FALSE, NULL);
    snprintf(port->name, sizeof(port->name), "%s", name);
    port->flags = linuxFlags;
    port->nonBlocking = (linuxFlags & LINUX_O_NONBLOCK) != 0;
    port->vmin = 1; // Linux's after an open, until the game sets its own
    port->vtime = 0;
    serialPortSetup(fd, 0x1002, 0x30, port->vmin, port->vtime); // 115200, 8N1
    PurgeComm(handle, PURGE_RXCLEAR | PURGE_TXCLEAR);
    log_info("Serial passthrough: %s opened for the game (descriptor %d)", name, fd);
    return fd;
}

void serialPortSetup(int fd, unsigned int speed, unsigned int cflag, int vmin, int vtime)
{
    SerialPort *port = findPort(fd);
    if (!port)
        return;
    DCB dcb = {0};
    dcb.DCBlength = sizeof(dcb);
    GetCommState(port->handle, &dcb);
    DWORD rate = baudRate(speed ? speed : cflag & LINUX_CBAUD);
    if (rate)
        dcb.BaudRate = rate;
    static const BYTE sizes[4] = {5, 6, 7, 8};
    dcb.ByteSize = sizes[(cflag & LINUX_CSIZE) >> 4];
    dcb.StopBits = cflag & LINUX_CSTOPB ? TWOSTOPBITS : ONESTOPBIT;
    dcb.fParity = (cflag & LINUX_PARENB) != 0;
    dcb.Parity = !(cflag & LINUX_PARENB) ? NOPARITY : cflag & LINUX_PARODD ? ODDPARITY : EVENPARITY;
    dcb.fBinary = TRUE;
    dcb.fOutxCtsFlow = (cflag & LINUX_CRTSCTS) != 0;
    dcb.fRtsControl = cflag & LINUX_CRTSCTS ? RTS_CONTROL_HANDSHAKE : RTS_CONTROL_ENABLE;
    dcb.fOutxDsrFlow = FALSE;
    dcb.fDtrControl = DTR_CONTROL_ENABLE;
    dcb.fDsrSensitivity = FALSE;
    dcb.fOutX = dcb.fInX = FALSE;
    dcb.fNull = FALSE;
    dcb.fAbortOnError = FALSE;
    if (!SetCommState(port->handle, &dcb))
        log_warn("%s: SetCommState failed (%lu)", port->name, GetLastError());
    port->vmin = vmin;
    port->vtime = vtime;
    applyTimeouts(port);
    log_info("Serial passthrough: %s at %lu bauds, %d%c%d, VMIN %d VTIME %d%s", port->name, dcb.BaudRate, dcb.ByteSize,
             dcb.Parity == NOPARITY ? 'N' : dcb.Parity == ODDPARITY ? 'O' : 'E', dcb.StopBits == TWOSTOPBITS ? 2 : 1, vmin,
             vtime, port->nonBlocking ? ", non-blocking" : "");
}

void serialPortSetNonBlocking(int fd, bool nonBlocking)
{
    SerialPort *port = findPort(fd);
    if (!port || port->nonBlocking == nonBlocking)
        return;
    port->nonBlocking = nonBlocking;
    port->flags = nonBlocking ? port->flags | LINUX_O_NONBLOCK : port->flags & ~LINUX_O_NONBLOCK;
    applyTimeouts(port);
}

int serialPortFlags(int fd)
{
    SerialPort *port = findPort(fd);
    return port ? port->flags : 0;
}

// An overlapped transfer, waited for: as long as COMMTIMEOUTS let it last
static bool transfer(SerialPort *port, bool reading, void *buffer, DWORD count, DWORD *done)
{
    OVERLAPPED overlapped = {0};
    overlapped.hEvent = reading ? port->readDone : port->writeDone;
    ResetEvent(overlapped.hEvent);
    BOOL finished = reading ? ReadFile(port->handle, buffer, count, done, &overlapped)
                            : WriteFile(port->handle, buffer, count, done, &overlapped);
    if (!finished && GetLastError() != ERROR_IO_PENDING)
        return false;
    return GetOverlappedResult(port->handle, &overlapped, done, TRUE) != 0;
}

int serialPortRead(int fd, void *buffer, size_t count)
{
    SerialPort *port = findPort(fd);
    DWORD done = 0;
    if (!port || !transfer(port, true, buffer, (DWORD)count, &done))
        return -1;
    if (done == 0 && port->nonBlocking)
    {
        errno = LINUX_EAGAIN; // the game reads errno as Linux's
        return -1;
    }
    return (int)done;
}

int serialPortWrite(int fd, const void *buffer, size_t count)
{
    SerialPort *port = findPort(fd);
    DWORD done = 0;
    if (!port || !transfer(port, false, (void *)buffer, (DWORD)count, &done))
        return -1;
    return (int)done;
}

int serialPortPending(int fd)
{
    SerialPort *port = findPort(fd);
    COMSTAT status;
    DWORD errors;
    if (!port || !ClearCommError(port->handle, &errors, &status))
        return 0;
    return (int)status.cbInQue;
}

void serialPortFlush(int fd, int queue)
{
    SerialPort *port = findPort(fd);
    if (port)
        PurgeComm(port->handle, (queue != 1 ? PURGE_RXCLEAR : 0) | (queue != 0 ? PURGE_TXCLEAR : 0));
}

void serialPortDrain(int fd)
{
    SerialPort *port = findPort(fd);
    if (port)
        FlushFileBuffers(port->handle);
}

#define LINUX_TIOCOUTQ 0x5411
#define LINUX_TCFLSH 0x540B
#define LINUX_TIOCMGET 0x5415
#define LINUX_TIOCMBIS 0x5416
#define LINUX_TIOCMBIC 0x5417
#define LINUX_TIOCMSET 0x5418
#define LINUX_FIONREAD 0x541B
#define LINUX_TIOCM_DTR 0x002
#define LINUX_TIOCM_RTS 0x004
#define LINUX_TIOCM_CTS 0x020
#define LINUX_TIOCM_CAR 0x040
#define LINUX_TIOCM_RNG 0x080
#define LINUX_TIOCM_DSR 0x100

int serialPortIoctl(int fd, unsigned long request, void *argument)
{
    SerialPort *port = findPort(fd);
    int *value = argument;
    if (!port)
        return -1;
    switch (request)
    {
        case LINUX_FIONREAD:
            *value = serialPortPending(fd);
            return 0;
        case LINUX_TIOCOUTQ:
        {
            COMSTAT status;
            DWORD errors;
            *value = ClearCommError(port->handle, &errors, &status) ? (int)status.cbOutQue : 0;
            return 0;
        }
        case LINUX_TCFLSH:
            serialPortFlush(fd, (int)(intptr_t)argument);
            return 0;
        case LINUX_TIOCMGET:
        {
            DWORD lines = 0;
            GetCommModemStatus(port->handle, &lines);
            *value = LINUX_TIOCM_DTR | LINUX_TIOCM_RTS | (lines & MS_CTS_ON ? LINUX_TIOCM_CTS : 0) |
                     (lines & MS_DSR_ON ? LINUX_TIOCM_DSR : 0) | (lines & MS_RING_ON ? LINUX_TIOCM_RNG : 0) |
                     (lines & MS_RLSD_ON ? LINUX_TIOCM_CAR : 0);
            return 0;
        }
        case LINUX_TIOCMSET:
        case LINUX_TIOCMBIS:
        case LINUX_TIOCMBIC:
        {
            bool set = request != LINUX_TIOCMBIC;
            if (request == LINUX_TIOCMSET || (*value & LINUX_TIOCM_DTR))
                EscapeCommFunction(port->handle, set && (*value & LINUX_TIOCM_DTR) ? SETDTR : CLRDTR);
            if (request == LINUX_TIOCMSET || (*value & LINUX_TIOCM_RTS))
                EscapeCommFunction(port->handle, set && (*value & LINUX_TIOCM_RTS) ? SETRTS : CLRRTS);
            return 0;
        }
        default:
            log_debug("%s: ioctl 0x%lx not handled", port->name, request);
            return 0;
    }
}

int serialPortClose(int fd)
{
    SerialPort *port = findPort(fd);
    if (!port)
        return -1;
    log_info("Serial passthrough: %s closed", port->name);
    port->used = false;
    CloseHandle(port->readDone);
    CloseHandle(port->writeDone);
    return _close(fd); // the handle with it
}

#else

bool serialPortName(const char *path, char *name, size_t size)
{
    if (!path || strncmp(path, "/dev/", 5) != 0)
        return false;
    snprintf(name, size, "%s", path);
    return true;
}

// The USB adapters, then the first four of the machine's own ports
int serialPortList(char names[][SERIAL_PORT_NAME], int max)
{
    PortEntry entries[256];
    int count = 0;
    DIR *dev = opendir("/dev");
    if (!dev)
        return 0;
    for (struct dirent *entry; (entry = readdir(dev)) && count < 256;)
    {
        const char *prefixes[] = {"ttyUSB", "ttyACM", "ttyS"};
        for (int i = 0; i < 3; i++)
        {
            size_t length = strlen(prefixes[i]);
            if (strncmp(entry->d_name, prefixes[i], length) != 0 || !isdigit((unsigned char)entry->d_name[length]))
                continue;
            int number = atoi(entry->d_name + length);
            if (i == 2 && number > 3)
                break;
            snprintf(entries[count].name, SERIAL_PORT_NAME, "/dev/%s", entry->d_name);
            entries[count++].number = number;
            break;
        }
    }
    closedir(dev);
    return sortedList(entries, count, names, max);
}

#endif
