// Network outputs, MAME style, so tools such as BackForceFeeder or MAMEHooker get lamps and drive board data
// without game-side plugins (OutputBlaster, FFB Arcade Plugin).
//
// Same protocol as the njz3 sm2-emu and Supermodel forks:
//  - TCP server (default port 8000), every client receives the same "name = value\r" lines.
//  - On connect a client gets "mame_start = <game>\r" and every known value.
//  - One UDP datagram "mame_start = <game>\rtcp = <port>\r" to 127.0.0.1:<udp port> (default 8001) at start.
//  - "mame_stop = 1\r" when the loader exits.

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include <SDL3/SDL.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "../../config/config.h"
#include "../../log/log.h"
#include "netOutputs.h"

#ifdef _WIN32
typedef SOCKET NetSocket;
#define NET_INVALID_SOCKET INVALID_SOCKET
#define netCloseSocket closesocket
#define NET_SEND_FLAGS 0
static bool netWouldBlock(void)
{
    return WSAGetLastError() == WSAEWOULDBLOCK;
}
#else
typedef int NetSocket;
#define NET_INVALID_SOCKET (-1)
#define netCloseSocket close
#define NET_SEND_FLAGS MSG_NOSIGNAL
static bool netWouldBlock(void)
{
    return errno == EAGAIN || errno == EWOULDBLOCK;
}
#endif

#define NET_MAX_CLIENTS 8
#define NET_CLIENT_BUFFER 65536 // a client further behind than this is dropped
#define NET_GPO_BYTES 4
#define NET_POLL_MS 10

typedef struct
{
    NetSocket socket;
    char out[NET_CLIENT_BUFFER];
    size_t outLen;
} NetClient;

static bool running = false;
static bool stopping = false;
static SDL_Thread *serverThread = NULL;
static SDL_Mutex *stateMutex = NULL;
static NetSocket listenSocket = NET_INVALID_SOCKET;
static NetClient clients[NET_MAX_CLIENTS];

static char gameName[64];
static int tcpPort;

// Known output values, sent to clients that connect later. Protected by stateMutex.
static uint8_t gpo[NET_GPO_BYTES];
static bool gpoKnown[NET_GPO_BYTES];
static int lastDrive = -1;

// Lines produced since the server thread last ran. Protected by stateMutex.
static char *pending = NULL;
static size_t pendingLen = 0;
static size_t pendingCap = 0;

static void setNonBlocking(NetSocket s)
{
#ifdef _WIN32
    u_long mode = 1;
    ioctlsocket(s, FIONBIO, &mode);
#else
    fcntl(s, F_SETFL, fcntl(s, F_GETFL, 0) | O_NONBLOCK);
#endif
}

static void pendingAppend(const char *text, size_t len)
{
    if (pendingLen + len > pendingCap)
    {
        size_t newCap = pendingCap ? pendingCap * 2 : 4096;
        while (newCap < pendingLen + len)
            newCap *= 2;
        char *grown = (char *)SDL_realloc(pending, newCap);
        if (!grown)
            return;
        pending = grown;
        pendingCap = newCap;
    }
    memcpy(pending + pendingLen, text, len);
    pendingLen += len;
}

static int formatLine(char *buf, size_t size, const char *name, int value)
{
    return SDL_snprintf(buf, size, "%s = %d\r", name, value);
}

static void pendingLine(const char *name, int value)
{
    char line[96];
    int len = formatLine(line, sizeof(line), name, value);
    if (len > 0)
        pendingAppend(line, (size_t)len);
}

static void gpoNames(int byteIndex, char *rawName, size_t rawSize)
{
    if (byteIndex == 0)
        SDL_snprintf(rawName, rawSize, "RawLamps");
    else
        SDL_snprintf(rawName, rawSize, "RawLamps%d", byteIndex);
}

// JVS output n (1-based) is bit (7 - (n - 1) % 8) of byte (n - 1) / 8; lamp names are 0-based like MAME's
static void lampName(int byteIndex, int bit, char *name, size_t size)
{
    SDL_snprintf(name, size, "lamp%d", byteIndex * 8 + (7 - bit));
}

// Appends every known value, for a client that just connected
static size_t buildState(char *buf, size_t size)
{
    size_t len = 0;
    int written = SDL_snprintf(buf, size, "mame_start = %s\r", gameName);
    if (written > 0)
        len += (size_t)written;

    for (int i = 0; i < NET_GPO_BYTES && len < size; i++)
    {
        if (!gpoKnown[i])
            continue;
        char name[32];
        gpoNames(i, name, sizeof(name));
        written = formatLine(buf + len, size - len, name, gpo[i]);
        if (written > 0)
            len += (size_t)written;
        for (int bit = 7; bit >= 0 && len < size; bit--)
        {
            lampName(i, bit, name, sizeof(name));
            written = formatLine(buf + len, size - len, name, (gpo[i] >> bit) & 1);
            if (written > 0)
                len += (size_t)written;
        }
    }

    if (lastDrive >= 0 && len < size)
    {
        written = formatLine(buf + len, size - len, "RawDrive", lastDrive);
        if (written > 0)
            len += (size_t)written;
    }
    return len < size ? len : size;
}

static void closeClient(NetClient *client)
{
    if (client->socket != NET_INVALID_SOCKET)
        netCloseSocket(client->socket);
    client->socket = NET_INVALID_SOCKET;
    client->outLen = 0;
}

static void clientQueue(NetClient *client, const char *data, size_t len)
{
    if (client->socket == NET_INVALID_SOCKET)
        return;
    if (client->outLen + len > NET_CLIENT_BUFFER)
    {
        log_warn("Network outputs: client too slow, dropping it");
        closeClient(client);
        return;
    }
    memcpy(client->out + client->outLen, data, len);
    client->outLen += len;
}

static void clientFlush(NetClient *client)
{
    while (client->socket != NET_INVALID_SOCKET && client->outLen > 0)
    {
        int sent = send(client->socket, client->out, (int)client->outLen, NET_SEND_FLAGS);
        if (sent > 0)
        {
            memmove(client->out, client->out + sent, client->outLen - (size_t)sent);
            client->outLen -= (size_t)sent;
        }
        else
        {
            if (sent < 0 && netWouldBlock())
                return;
            closeClient(client);
        }
    }
}

// Nothing is read from clients yet: drain what they send and notice disconnections
static void clientDrain(NetClient *client)
{
    char buf[256];
    while (client->socket != NET_INVALID_SOCKET)
    {
        int got = recv(client->socket, buf, sizeof(buf), 0);
        if (got > 0)
            continue;
        if (got < 0 && netWouldBlock())
            return;
        closeClient(client);
    }
}

static void acceptClients(void)
{
    for (;;)
    {
        NetSocket s = accept(listenSocket, NULL, NULL);
        if (s == NET_INVALID_SOCKET)
            return;

        NetClient *client = NULL;
        for (int i = 0; i < NET_MAX_CLIENTS; i++)
        {
            if (clients[i].socket == NET_INVALID_SOCKET)
            {
                client = &clients[i];
                break;
            }
        }
        if (!client)
        {
            netCloseSocket(s);
            continue;
        }

        setNonBlocking(s);
        int noDelay = 1;
        setsockopt(s, IPPROTO_TCP, TCP_NODELAY, (const char *)&noDelay, sizeof(noDelay));
        client->socket = s;
        client->outLen = 0;

        char state[2048];
        SDL_LockMutex(stateMutex);
        size_t len = buildState(state, sizeof(state));
        SDL_UnlockMutex(stateMutex);
        clientQueue(client, state, len);
        log_info("Network outputs: client connected");
    }
}

static int SDLCALL serverThreadMain(void *data)
{
    (void)data;
    char *batch = NULL;
    size_t batchCap = 0;

    for (;;)
    {
        acceptClients();

        // Take the lines produced since last time
        SDL_LockMutex(stateMutex);
        bool stop = stopping;
        size_t batchLen = pendingLen;
        if (batchLen > batchCap)
        {
            char *grown = (char *)SDL_realloc(batch, batchLen);
            if (grown)
            {
                batch = grown;
                batchCap = batchLen;
            }
            else
                batchLen = 0;
        }
        if (batchLen)
            memcpy(batch, pending, batchLen);
        pendingLen = 0;
        SDL_UnlockMutex(stateMutex);

        for (int i = 0; i < NET_MAX_CLIENTS; i++)
        {
            if (batchLen)
                clientQueue(&clients[i], batch, batchLen);
            clientFlush(&clients[i]);
            clientDrain(&clients[i]);
        }

        if (stop)
            break;
        SDL_Delay(NET_POLL_MS);
    }

    SDL_free(batch);
    return 0;
}

static void announceStart(int udpPort)
{
    if (udpPort <= 0)
        return;
    NetSocket s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == NET_INVALID_SOCKET)
        return;
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((unsigned short)udpPort);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    char message[128];
    int len = SDL_snprintf(message, sizeof(message), "mame_start = %s\rtcp = %d\r", gameName, tcpPort);
    if (len > 0)
        sendto(s, message, len, 0, (const struct sockaddr *)&addr, sizeof(addr));
    netCloseSocket(s);
}

void netOutputsInit(void)
{
    EmulatorConfig *config = getConfig();
    if (running || !config->outputsNetwork)
        return;

#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        log_error("Network outputs: WSAStartup failed");
        return;
    }
#endif

    tcpPort = config->outputsNetworkPort;
    SDL_snprintf(gameName, sizeof(gameName), "%s", config->gameShortTitle ? config->gameShortTitle : "lindbergh");

    listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSocket == NET_INVALID_SOCKET)
    {
        log_error("Network outputs: cannot create the TCP socket");
        return;
    }
#ifndef _WIN32
    int reuse = 1;
    setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
#endif
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((unsigned short)tcpPort);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(listenSocket, (const struct sockaddr *)&addr, sizeof(addr)) != 0 || listen(listenSocket, NET_MAX_CLIENTS) != 0)
    {
        log_error("Network outputs: cannot listen on TCP port %d", tcpPort);
        netCloseSocket(listenSocket);
        listenSocket = NET_INVALID_SOCKET;
        return;
    }
    setNonBlocking(listenSocket);

    for (int i = 0; i < NET_MAX_CLIENTS; i++)
        clients[i].socket = NET_INVALID_SOCKET;

    stateMutex = SDL_CreateMutex();
    running = true;
    stopping = false;
    serverThread = SDL_CreateThread(serverThreadMain, "NetOutputs", NULL);
    if (!serverThread)
    {
        log_error("Network outputs: cannot start the server thread");
        running = false;
        netCloseSocket(listenSocket);
        listenSocket = NET_INVALID_SOCKET;
        return;
    }

    announceStart(config->outputsNetworkUdpPort);
    log_warn("Network outputs: listening on TCP port %d (game \"%s\")", tcpPort, gameName);
}

void netOutputsShutdown(void)
{
    if (!running)
        return;

    SDL_LockMutex(stateMutex);
    pendingAppend("mame_stop = 1\r", strlen("mame_stop = 1\r"));
    stopping = true;
    SDL_UnlockMutex(stateMutex);
    SDL_WaitThread(serverThread, NULL); // the thread flushes mame_stop before it ends
    serverThread = NULL;
    running = false;

    for (int i = 0; i < NET_MAX_CLIENTS; i++)
        closeClient(&clients[i]);
    netCloseSocket(listenSocket);
    listenSocket = NET_INVALID_SOCKET;
    SDL_DestroyMutex(stateMutex);
    stateMutex = NULL;
#ifdef _WIN32
    WSACleanup();
#endif
}

void netOutputsSetGpo(int byteIndex, uint8_t value)
{
    if (!running || byteIndex < 0 || byteIndex >= NET_GPO_BYTES)
        return;

    SDL_LockMutex(stateMutex);
    if (!gpoKnown[byteIndex] || gpo[byteIndex] != value)
    {
        uint8_t changed = gpoKnown[byteIndex] ? (uint8_t)(gpo[byteIndex] ^ value) : 0xFF;
        gpo[byteIndex] = value;
        gpoKnown[byteIndex] = true;

        char name[32];
        gpoNames(byteIndex, name, sizeof(name));
        pendingLine(name, value);
        for (int bit = 7; bit >= 0; bit--)
        {
            if (changed & (1 << bit))
            {
                lampName(byteIndex, bit, name, sizeof(name));
                pendingLine(name, (value >> bit) & 1);
            }
        }
    }
    SDL_UnlockMutex(stateMutex);
}

void netOutputsDrive(const uint8_t *bytes, size_t count)
{
    if (!running || !bytes)
        return;

    SDL_LockMutex(stateMutex);
    for (size_t i = 0; i < count; i++)
    {
        pendingLine("RawDrive", bytes[i]);
        lastDrive = bytes[i];
    }
    SDL_UnlockMutex(stateMutex);
}
