#pragma once

#define JVSBUFFER_SIZE 1024

typedef struct {
    int ctsCounter;
    int ready;
    int size;
    char buffer[JVSBUFFER_SIZE];
} JVSFrame;

#ifdef __linux__
int getDCD(int fd);
int getDSR(int fd);
int getCTS(int fd);
#endif

JVSFrame readJVSFrameFromThread();
int startJVSFrameThread(int * fd);
void * readJVSFrameThread(void * arg);

int openJVSSerial(char *jvsPath);
int initJVSSerial(int fd);
int readJVSFrame(int fd, unsigned char *buffer);
int jvsSerialWrite(int fd, const void *buffer, int count);