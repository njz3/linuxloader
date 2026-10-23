#pragma once

// The real serial ports a board is passed to: the drive board (EMULATE_DRIVEBOARD false, [Paths] SERIAL_1_PATH) and
// the JVS I/O board (EMULATE_JVS false, JVS_PATH). The ports the system has, for the "Game options" page; on
// Windows, a COM port opened for the game and set as Linux's termios would set it (serialPort.c).

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SERIAL_PORT_NAME 32

// The ports the system knows, sorted by number: "COM3" (Windows), "/dev/ttyUSB0" (Linux); their count
int serialPortList(char names[][SERIAL_PORT_NAME], int max);
// A port as the settings write it: "COM12" for "COM12" or "\\.\COM12" (Windows); false for a path that is not one
bool serialPortName(const char *path, char *name, size_t size);

#ifdef _WIN32
// The game's open of its serial port, given to a COM port: a descriptor of the C runtime, -1 when it does not open
int serialPortOpen(const char *path, int linuxFlags);
bool serialPortIs(int fd);
// Linux's termios: the speed (B9600...), c_cflag's size, stop bits, parity and flow control, VMIN and VTIME
void serialPortSetup(int fd, unsigned int speed, unsigned int cflag, int vmin, int vtime);
void serialPortSetNonBlocking(int fd, bool nonBlocking);
int serialPortFlags(int fd); // the Linux flags of the open, O_NONBLOCK as it is now
int serialPortRead(int fd, void *buffer, size_t count);
int serialPortWrite(int fd, const void *buffer, size_t count);
int serialPortPending(int fd);             // the bytes waiting to be read
void serialPortFlush(int fd, int queue);   // tcflush: 0 input, 1 output, 2 both
void serialPortDrain(int fd);              // tcdrain
// Linux's serial ioctls: FIONREAD, TIOCOUTQ, the modem lines (TIOCMGET, TIOCMSET, TIOCMBIS, TIOCMBIC), TCFLSH
int serialPortIoctl(int fd, unsigned long request, void *argument);
int serialPortClose(int fd);
#endif

#ifdef __cplusplus
}
#endif
