// The JVS packets passed to a real I/O board (EMULATE_JVS false, JVS_PATH): Linux's serial device, or a COM port on
// Windows (serialPort.c), the board's answers read by a thread into one frame at a time.

#include <stdio.h>   /* Standard input/output definitions */
#include <string.h>  /* String function definitions */

#include "passthrough.h"

#ifdef __linux__
#include <unistd.h>  /* UNIX standard function definitions */
#include <fcntl.h>   /* File control definitions */
#include <errno.h>   /* Error number definitions */
#include <termios.h> /* POSIX terminal control definitions */
#include <sys/ioctl.h> /* Ioctl function to control device drivers in the kernel */
#include <pthread.h>   /* POSIX threads API to create and manage threads in the program */
#include <time.h>
#define LOCK_FRAME() pthread_mutex_lock(&jvsBuffer_lock)
#define UNLOCK_FRAME() pthread_mutex_unlock(&jvsBuffer_lock)
#define SERIAL_READ(fd, buffer, count) read(fd, buffer, count)
#else
#include <windows.h>
#include "serialPort.h"
static CRITICAL_SECTION jvsBufferLock;
#define LOCK_FRAME() EnterCriticalSection(&jvsBufferLock)
#define UNLOCK_FRAME() LeaveCriticalSection(&jvsBufferLock)
#define SERIAL_READ(fd, buffer, count) serialPortRead(fd, buffer, count)
#endif

#define TIMEOUT_SELECT 200
#define CTS_ON_RETRY 20

#define JVS_ANSWER_WAIT 200 // ms

// Used to read JVS frame in a non-blocking way
JVSFrame jvsFrameBuffer;
static volatile unsigned int jvsLastRequest; // when the last request began, in ms
#ifdef __linux__
pthread_mutex_t jvsBuffer_lock = PTHREAD_MUTEX_INITIALIZER;
#endif

static unsigned int jvsMilliseconds(void)
{
#ifdef _WIN32
    return GetTickCount();
#else
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (unsigned int)(now.tv_sec * 1000 + now.tv_nsec / 1000000);
#endif
}

// A request's first byte (its sync): the frame the board answered before is not this one's
static void jvsRequestStarts(const void *buffer, int count)
{
    if (count < 1 || *(const unsigned char *)buffer != 0xE0)
        return;
    LOCK_FRAME();
    jvsFrameBuffer.ready = 0;
    jvsLastRequest = jvsMilliseconds();
    UNLOCK_FRAME();
}

#ifdef _WIN32
// The port as Linux's initJVSSerial sets it: 115200 bauds, 8N1, no flow control, a read waiting 0.1 s for its byte
int openJVSSerial(char *jvsPath)
{
    int fd = serialPortOpen(jvsPath, 2); // O_RDWR
    if (fd < 0)
        printf("Failed to open '%s' for JVS.\n", jvsPath);
    return fd;
}

int initJVSSerial(int fd)
{
    serialPortSetup(fd, 0x1002, 0x30, 0, 1); // B115200, CS8, VMIN 0, VTIME 1
    return 0;
}

int jvsSerialWrite(int fd, const void *buffer, int count)
{
    jvsRequestStarts(buffer, count);
    return serialPortWrite(fd, buffer, count);
}
#else
int jvsSerialWrite(int fd, const void *buffer, int count)
{
    jvsRequestStarts(buffer, count);
    return write(fd, buffer, count);
}
#endif

#ifdef __linux__


/**
 * Open the serial interface and return a file descriptor
 * @param jvsPath The serial port path. Ex: "/dev/ttyS3"
 * @return A file descriptor
 */
int openJVSSerial(char *jvsPath) 
{
    int jvsFileDescriptor = -1;

    // TODO: check O_NOCTTY declaration
    jvsFileDescriptor = open(jvsPath, O_RDWR | O_NOCTTY);
    if (jvsFileDescriptor < 0)
    {
        printf("Failed to open '%s' for JVS.\n", jvsPath);
    }

    return jvsFileDescriptor;
}


/**
 * Init a serial port (using file descriptor) so it behaves correctly for JVS usage
 * @param fd
 * @return 0|1
 */
int initJVSSerial(int fd) 
{
    struct termios options;
    int status;

    //  Get the current options
    if (tcgetattr(fd, &options) != 0) {
        printf("Error %i from tcgetattr: %s\n", errno, strerror(errno));
        return 1;
    }

    // Set rates
    cfsetispeed(&options, B115200);
    cfsetospeed(&options, B115200);

    /* From doc:
     * If the CLOCAL flag for a line is off, the hardware carrier detect (DCD) signal is significant,
     * an open(2) of the corresponding terminal will block until DCD is asserted, unless the O_NONBLOCK
     * flag is given. If CLOCAL is set, the line behaves as if DCD is always asserted. The software
     * carrier flag is usually turned on for local devices, and is off for lines with modems.
     */
    // options.c_cflag |= CREAD | CLOCAL; // Turn on READ & ignore ctrl lines
    options.c_cflag |= CREAD; // Turn on READ, let ctrl lines work

    options.c_cflag &= ~PARENB; // Clear parity bit & disable parity
    options.c_cflag &= ~CSTOPB; // Clear stop field, 1 stop bit
    options.c_cflag &= ~CSIZE; // Clear all bits that set the data size
    options.c_cflag |= CS8; // 8 bits
    options.c_cflag &= ~CRTSCTS; // Disable RTS/CTS hardware flow control

    options.c_lflag &= ~ICANON; // Disable canonical mode, so no input processing is performed
    options.c_lflag &= ~ECHO; // Disable echo
    options.c_lflag &= ~ECHOE; // Disable erasure
    options.c_lflag &= ~ECHONL; // Disable new-line echo
    options.c_lflag &= ~ISIG; // Disable interpretation of INTR, QUIT and SUSP
    options.c_iflag &= ~(IXON | IXOFF | IXANY); // Turn off s/w flow ctrl
    options.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR |
                         ICRNL); // Disable any special handling of received bytes

    options.c_oflag &= ~OPOST; // Prevent special interpretation of output bytes (e.g. newline chars)
    options.c_oflag &= ~ONLCR; // Prevent conversion of newline to carriage return/line feed

    // Set VMIN and VTIME to 0, so it returns immediately when no data are present
    // options.c_cc[VMIN] = 0;
    // options.c_cc[VTIME] = 0;

    // With threaded serial read we should rely on a blocking read() function so the loop doesn't run crazy => read() could block indefinitely.
    // options.c_cc[VMIN] = 1;
    // options.c_cc[VTIME] = 0;

    // Block until either VMIN characters have been received or VTIME **after first character** has been received
    options.c_cc[VMIN] = 0;
    options.c_cc[VTIME] = 1;


    tcsetattr(fd, TCSANOW, &options);

    /* No use ? Save it for later
    // Set the serial port to non-blocking mode
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    */

    return 0;
}


/**
 * The DCD (Data Carrier Detect) status of a serial port indicates whether a carrier is present on the line, meaning that a connection has been established with another device.
 * @param fd File descriptor of JVS (serial) port
 * @return 0|1 According to control line status
 */
int getDCD(int fd)
{
    int status;
    ioctl(fd, TIOCMGET, &status);
    return (status & TIOCM_CAR) != 0;
}

/**
 * The DSR (Data Set Ready) status of a serial port indicates whether the device at the other end of the connection is ready to receive data.
 * @param fd File descriptor of JVS (serial) port
 * @return 0|1 According to control line status
 */
int getDSR(int fd)
{
    int status;
    ioctl(fd, TIOCMGET, &status);
    return (status & TIOCM_DSR) != 0;
}

/**
 * The CTS (Clear To Send) status of a serial port indicates whether the device at the other end of the connection is ready to accept data.
 * @param fd File descriptor of JVS (serial) port
 * @return 0|1 According to control line status
 */
int getCTS(int fd)
{
    int status;
    ioctl(fd, TIOCMGET, &status);
    return (status & TIOCM_CTS) != 0;
}

#endif

/**
 * In charge of reading serial port and bufferize JVS frame
 * @param arg
 * @return
 */
#ifdef _WIN32
static DWORD WINAPI jvsFrameThreadMain(LPVOID arg)
{
    readJVSFrameThread(arg);
    return 0;
}
#endif

void *readJVSFrameThread(void * arg)
{
    int fd = *((int *) arg);
    int byteCount, bytesRead, ackSize, waitForEnd;
    int ctsRetry = CTS_ON_RETRY;
    char localBuffer[JVSBUFFER_SIZE];

    while (1)
    {
        // Reset local variable
        byteCount = 0;
        ackSize = 0;
        waitForEnd = 0;

        do
        {
            // printf("SERIAL thread debug: trying to read byte.\n");
            // Try to read a byte from serial, this call will be blocking if VMIN > 0 and VTIME = 0
            bytesRead = SERIAL_READ(fd, &localBuffer[byteCount], 1);

            // If nothing on serial and CTS is ON, we try to read "CTS_ON_RETRY" times
            ctsRetry = CTS_ON_RETRY;
            while (bytesRead < 1 && --ctsRetry > 0 && jvsFrameBuffer.ready == 0) {
                bytesRead = SERIAL_READ(fd, &localBuffer[byteCount], 1);
                // printf("SERIAL: Retry number %d.\n", ctsRetry);
            }

            if (bytesRead > 0) {
                // Sync byte, we will stick in the loop
                if (byteCount == 0 && localBuffer[byteCount] == (char) 0xE0) {
                    waitForEnd = 1;
                }

                // Size byte
                if (byteCount == 2) {
                    ackSize = localBuffer[byteCount] + 3;
                }

                // Start counting bytes only if SYNC has been found
                if (waitForEnd) {
                    byteCount++;
                }

                // Reached the end of the message
                if (byteCount == ackSize) {
                    waitForEnd = 0;
                }
            }
        } while (waitForEnd);

        // Nothing came: an empty frame tells the game there is no answer (a reset's), but not while the board may still
        // answer the last request (the game would take it for that answer)
        if (byteCount == 0 && jvsMilliseconds() - jvsLastRequest < JVS_ANSWER_WAIT)
            continue;

        // Lock the buffer while we write to it
        LOCK_FRAME();
        memcpy(jvsFrameBuffer.buffer, localBuffer, byteCount);
        jvsFrameBuffer.size = byteCount;
        jvsFrameBuffer.ready = 1;
        ctsRetry = CTS_ON_RETRY;
        UNLOCK_FRAME();
    }

    return NULL;
}

/**
 * Init the thread in charge of reading serial port
 * @param fd
 * @return 0|1
 */
int startJVSFrameThread(int *fd)
{
    printf("SERIAL: starting thread.\n");

    // Clean shared JVS frame buffer
    jvsFrameBuffer.ready = 0;
    jvsFrameBuffer.size = 0;
    memset(jvsFrameBuffer.buffer, 0, JVSBUFFER_SIZE);

#ifdef _WIN32
    InitializeCriticalSection(&jvsBufferLock);
    HANDLE jvsFrameThread = CreateThread(NULL, 0, jvsFrameThreadMain, fd, 0, NULL);
    int ret = jvsFrameThread == NULL;
    if (jvsFrameThread)
        CloseHandle(jvsFrameThread);
#else
    pthread_t jvsFrameThread;
    int ret = pthread_create(&jvsFrameThread, NULL, readJVSFrameThread, fd);
#endif
    if (ret != 0)
    {
        printf("SERIAL: Failed to create reader thread\n");
        return 1;
    }

    return 0;
}

/**
 * Return a JVSFrame structure with empty or full data, no in between
 * @return
 */
JVSFrame readJVSFrameFromThread()
{
    JVSFrame frame;
    // Lock while reading/writing to shared frame
    LOCK_FRAME();

    // Check if we have a valid frame
    if (jvsFrameBuffer.ready == 1)
    {
        frame = jvsFrameBuffer;
        // It has been read, we disable this frame
        jvsFrameBuffer.ready = 0;
    }
    else
    {
        frame.ready = 0;
        frame.size = 0;
        memset(frame.buffer, 0, JVSBUFFER_SIZE);
    }
    UNLOCK_FRAME();

    return frame;
}
