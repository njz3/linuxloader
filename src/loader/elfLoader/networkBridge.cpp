#if defined(_WIN32) || defined(__MINGW32__)
// #include <mutex>
#include "networkBridge.hpp"
#include "symbolResolver.hpp"
#include "../log/log.h"
#include "../config/config.h"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <mutex>
#include <cerrno>

static std::mutex g_net_mutex;

#define MAP(name, func) SymbolResolver::GetInstance().RegisterVTable(name, reinterpret_cast<void *>(func))

// Maps a Winsock error to the errno value the Linux game expects. The game reads errno through __errno_location, so
// the numbers must be Linux i386 ones: the CRT <errno.h> of MSVC and MinGW uses other values for the socket errors
// (EINPROGRESS is 112 there, 115 on Linux).
static int mapWSAErrorToErrno(int wsaError)
{
    switch (wsaError)
    {
        case WSAEINTR:
            return 4; // EINTR
        case WSAEBADF:
            return 9; // EBADF
        case WSAEACCES:
            return 13; // EACCES
        case WSAEFAULT:
            return 14; // EFAULT
        case WSAEINVAL:
            return 22; // EINVAL
        case WSAEMFILE:
            return 24; // EMFILE
        case WSAEWOULDBLOCK:
            return 11; // EAGAIN
        case WSAEINPROGRESS:
            return 115; // EINPROGRESS
        case WSAEALREADY:
            return 114; // EALREADY
        case WSAENOTSOCK:
            return 88; // ENOTSOCK
        case WSAEDESTADDRREQ:
            return 89; // EDESTADDRREQ
        case WSAEMSGSIZE:
            return 90; // EMSGSIZE
        case WSAEPROTOTYPE:
            return 91; // EPROTOTYPE
        case WSAENOPROTOOPT:
            return 92; // ENOPROTOOPT
        case WSAEPROTONOSUPPORT:
            return 93; // EPROTONOSUPPORT
        case WSAESOCKTNOSUPPORT:
            return 94; // ESOCKTNOSUPPORT
        case WSAEOPNOTSUPP:
            return 95; // EOPNOTSUPP
        case WSAEPFNOSUPPORT:
            return 96; // EPFNOSUPPORT
        case WSAEAFNOSUPPORT:
            return 97; // EAFNOSUPPORT
        case WSAEADDRINUSE:
            return 98; // EADDRINUSE
        case WSAEADDRNOTAVAIL:
            return 99; // EADDRNOTAVAIL
        case WSAENETDOWN:
            return 100; // ENETDOWN
        case WSAENETUNREACH:
            return 101; // ENETUNREACH
        case WSAENETRESET:
            return 102; // ENETRESET
        case WSAECONNABORTED:
            return 103; // ECONNABORTED
        case WSAECONNRESET:
            return 104; // ECONNRESET
        case WSAENOBUFS:
            return 105; // ENOBUFS
        case WSAEISCONN:
            return 106; // EISCONN
        case WSAENOTCONN:
            return 107; // ENOTCONN
        case WSAESHUTDOWN:
            return 108; // ESHUTDOWN
        case WSAETOOMANYREFS:
            return 109; // ETOOMANYREFS
        case WSAETIMEDOUT:
            return 110; // ETIMEDOUT
        case WSAECONNREFUSED:
            return 111; // ECONNREFUSED
        case WSAELOOP:
            return 40; // ELOOP
        case WSAENAMETOOLONG:
            return 36; // ENAMETOOLONG
        case WSAEHOSTDOWN:
            return 112; // EHOSTDOWN
        case WSAEHOSTUNREACH:
            return 113; // EHOSTUNREACH
        case WSAENOTEMPTY:
            return 39; // ENOTEMPTY
        case WSAEUSERS:
            return 87; // EUSERS
        case WSAEDQUOT:
            return 122; // EDQUOT
        case WSAESTALE:
            return 116; // ESTALE
        case WSAEREMOTE:
            return 66; // EREMOTE
        default:
            return 22; // EINVAL
    }
}

namespace NetworkBridge
{
    void initBridges()
    {
        log_info("Initializing Network Bridges...");

        MAP("socket", bridgeSocket);
        MAP("connect", bridgeConnect);
        MAP("bind", bridgeBind);
        MAP("listen", bridgeListen);
        MAP("accept", bridgeAccept);
        MAP("send", bridgeSend);
        MAP("recv", bridgeRecv);
        MAP("sendto", bridgeSendto);
        MAP("recvfrom", bridgeRecvfrom);
        MAP("sendmsg", bridgeSendmsg);
        MAP("recvmsg", bridgeRecvmsg);
        MAP("getsockname", bridgeGetsockname);
        MAP("shutdown", shutdown);
        MAP("setsockopt", bridgeSetsockopt);
        MAP("getsockopt", bridgeGetsockopt);
        MAP("inet_pton", bridgeInet_pton);
        MAP("inet_aton", bridgeInet_aton);
        MAP("inet_addr", bridgeInet_addr);
        MAP("inet_ntoa", bridgeInet_ntoa);
        MAP("ntohs", bridgeNtohs);
        MAP("htons", bridgeHtons);
        MAP("htonl", bridgeHtonl);
        MAP("ntohl", bridgeNtohl);
        MAP("gethostbyname_r", bridgeGethostbyname_r);
        MAP("gethostbyaddr_r", bridgeGethostbyaddr_r);
        MAP("gethostbyaddr", bridgeGethostbyaddr_r);
        MAP("gethostbyname", bridgeGethostbyname_r);
        MAP("gethostname", bridgeGethostname);
        MAP("getservbyname", bridgeGethostbyname_r);
    }

    unsigned long bridgeInet_addr(const char *cp)
    {
        // Same redirections as the inet_addr() hook in patchNetwork.c on Linux
        if (cp && getConfig()->enableNetworkPatches)
        {
            uint32_t id = getConfig()->crc32;
            if (id == TOO_SPICY_SBMV)
            {
                if (strcmp("10.0.0.1", cp) == 0 && strcmp(getConfig()->tooSpicyIpCab1, "") != 0)
                    cp = getConfig()->tooSpicyIpCab1;
                else if (strcmp("10.0.0.2", cp) == 0 && strcmp(getConfig()->tooSpicyIpCab2, "") != 0)
                    cp = getConfig()->tooSpicyIpCab2;
            }
            else if (id == SEGA_RACE_TV_SBPF)
            {
                // Standalone = 4, Main = 0, 2-Sub = 1, 3-Sub = 2 and 4-Sub = 3
                if (strcmp("192.168.49.1", cp) == 0 || strcmp("192.168.49.2", cp) == 0 || strcmp("192.168.49.3", cp) == 0 ||
                    strcmp("192.168.49.4", cp) == 0)
                {
                    // An IP that is not the PC's makes the game crash in klib::lan::GetLanParam
                    cp = strcmp(getConfig()->srtvIP, "") != 0 ? getConfig()->srtvIP : "127.0.0.1";
                }
            }
        }
        return inet_addr(cp);
    }

    int bridgeInet_aton(const char *cp, struct in_addr *inp)
    {
        unsigned long addr = bridgeInet_addr(cp);
        if (addr == INADDR_NONE && strcmp(cp, "255.255.255.255") != 0)
            return 0;
        if (inp)
            inp->s_addr = addr;
        return 1;
    }

    int bridgeInet_pton(int af, const char *src, void *dst)
    {
        return InetPtonA(af, src, dst);
    }

    char *bridgeInet_ntoa(struct in_addr in)
    {
        return inet_ntoa(in);
    }
    uint16_t bridgeNtohs(uint16_t netshort)
    {
        return ntohs(netshort);
    }
    uint16_t bridgeHtons(uint16_t hostshort)
    {
        return htons(hostshort);
    }
    uint32_t bridgeNtohl(uint32_t netlong)
    {
        return ntohl(netlong);
    }
    uint32_t bridgeHtonl(uint32_t hostlong)
    {
        return htonl(hostlong);
    }
}; // namespace NetworkBridge

extern "C" SOCKET bridgeSocket(int af, int type, int protocol)
{
    log_info(">>> socket called: af=%d, type=%d, protocol=%d", af, type, protocol);
    SOCKET s = socket(af, type, protocol);
    if (s == INVALID_SOCKET)
    {
        errno = mapWSAErrorToErrno(WSAGetLastError());
        return (SOCKET)-1;
    }
    log_info(">>> socket EXIT: returning %lld", (long long)s);
    return s;
}

extern "C" int bridgeConnect(SOCKET s, const struct sockaddr *name, int namelen)
{
    log_info(">>> connect called: socket=%lld", (long long)s);
    int ret = connect(s, name, namelen);
    if (ret == SOCKET_ERROR)
    {
        // A non-blocking connect in progress is WSAEWOULDBLOCK on Windows, EINPROGRESS on Linux
        int wsaError = WSAGetLastError();
        errno = wsaError == WSAEWOULDBLOCK ? 115 : mapWSAErrorToErrno(wsaError);
    }
    log_info(">>> connect EXIT: returning %d (WSAError=%d)", ret, ret < 0 ? WSAGetLastError() : 0);
    return ret;
}

extern "C" int bridgeBind(SOCKET s, const struct sockaddr *name, int namelen)
{
    log_info(">>> bind called: socket=%lld", (long long)s);
    int ret = bind(s, name, namelen);
    if (ret == SOCKET_ERROR)
    {
        errno = mapWSAErrorToErrno(WSAGetLastError());
    }
    log_info(">>> bind EXIT: returning %d", ret);
    return ret;
}

extern "C" int bridgeListen(SOCKET s, int backlog)
{
    log_info(">>> listen called: socket=%lld, backlog=%d", (long long)s, backlog);
    int ret = listen(s, backlog);
    if (ret == SOCKET_ERROR)
    {
        errno = mapWSAErrorToErrno(WSAGetLastError());
    }
    log_info(">>> listen EXIT: returning %d", ret);
    return ret;
}

extern "C" SOCKET bridgeAccept(SOCKET s, struct sockaddr *addr, int *addrlen)
{
    log_info(">>> accept ENTRY: socket=%lld (THIS MAY BLOCK!)", (long long)s);
    SOCKET ret = accept(s, addr, addrlen);
    if (ret == INVALID_SOCKET)
    {
        errno = mapWSAErrorToErrno(WSAGetLastError());
        return (SOCKET)-1;
    }
    log_info(">>> accept EXIT: returning %lld (WSAError=%d)", (long long)ret, ret == INVALID_SOCKET ? WSAGetLastError() : 0);
    return ret;
}

extern "C" int bridgeRecv(SOCKET s, char *buf, int len, int flags)
{
    log_info(">>> recv ENTRY: socket=%lld, len=%d (THIS MAY BLOCK!)", (long long)s, len);

    if (flags & 0x40)
    {
        flags &= ~0x40;
    }

    int ret = recv(s, buf, len, flags);
    if (ret == SOCKET_ERROR)
    {
        errno = mapWSAErrorToErrno(WSAGetLastError());
    }
    log_info(">>> recv EXIT: returning %d", ret);
    return ret;
}

extern "C" int bridgeSend(SOCKET s, const char *buf, int len, int flags)
{
    log_info(">>> send called: socket=%lld, len=%d", (long long)s, len);

    flags &= ~(0x4000 | 0x40);

    int ret = send(s, buf, len, flags);
    if (ret == SOCKET_ERROR)
    {
        errno = mapWSAErrorToErrno(WSAGetLastError());
    }
    log_info(">>> send EXIT: returning %d", ret);
    return ret;
}

extern "C" int bridgeRecvfrom(SOCKET s, char *buf, int len, int flags, struct sockaddr *from, int *fromlen)
{
    log_info(">>> recvfrom ENTRY: socket=%lld, len=%d (THIS MAY BLOCK!)", (long long)s, len);
    if (flags & 0x40)
        flags &= ~0x40;
    int ret = recvfrom(s, buf, len, flags, from, fromlen);
    if (ret == SOCKET_ERROR)
    {
        errno = mapWSAErrorToErrno(WSAGetLastError());
    }
    log_info(">>> recvfrom EXIT: returning %d", ret);
    return ret;
}

extern "C" int bridgeSendto(SOCKET s, const char *buf, int len, int flags, const struct sockaddr *to, int tolen)
{
    log_info(">>> sendto called: socket=%lld, len=%d", (long long)s, len);
    flags &= ~(0x4000 | 0x40);
    int ret = sendto(s, buf, len, flags, to, tolen);
    if (ret == SOCKET_ERROR)
    {
        errno = mapWSAErrorToErrno(WSAGetLastError());
    }
    log_info(">>> sendto EXIT: returning %d", ret);
    return ret;
}

// Linux i386 struct iovec / struct msghdr
struct LinuxIovec
{
    void *iov_base;
    uint32_t iov_len;
};

struct LinuxMsghdr
{
    void *msg_name;
    uint32_t msg_namelen;
    LinuxIovec *msg_iov;
    uint32_t msg_iovlen;
    void *msg_control;
    uint32_t msg_controllen;
    int msg_flags;
};

static bool msghdrToWsabuf(const LinuxMsghdr *msg, WSABUF *bufs, DWORD maxBufs)
{
    if (!msg || msg->msg_iovlen > maxBufs)
        return false;
    for (uint32_t i = 0; i < msg->msg_iovlen; i++)
    {
        bufs[i].buf = (CHAR *)msg->msg_iov[i].iov_base;
        bufs[i].len = msg->msg_iov[i].iov_len;
    }
    return true;
}

extern "C" int bridgeSendmsg(SOCKET s, const LinuxMsghdr *msg, int flags)
{
    WSABUF bufs[16];
    if (!msghdrToWsabuf(msg, bufs, 16))
    {
        errno = EINVAL;
        return -1;
    }
    DWORD sent = 0;
    if (WSASendTo(s, bufs, msg->msg_iovlen, &sent, flags & ~(0x4000 | 0x40), (const struct sockaddr *)msg->msg_name,
                  msg->msg_name ? (int)msg->msg_namelen : 0, NULL, NULL) == SOCKET_ERROR)
    {
        errno = mapWSAErrorToErrno(WSAGetLastError());
        return -1;
    }
    return (int)sent;
}

extern "C" int bridgeRecvmsg(SOCKET s, LinuxMsghdr *msg, int flags)
{
    WSABUF bufs[16];
    if (!msghdrToWsabuf(msg, bufs, 16))
    {
        errno = EINVAL;
        return -1;
    }
    DWORD received = 0;
    DWORD wsaFlags = flags & ~0x40; // MSG_DONTWAIT has no Winsock equivalent
    INT fromLen = (INT)msg->msg_namelen;
    if (WSARecvFrom(s, bufs, msg->msg_iovlen, &received, &wsaFlags, (struct sockaddr *)msg->msg_name,
                    msg->msg_name ? &fromLen : NULL, NULL, NULL) == SOCKET_ERROR)
    {
        errno = mapWSAErrorToErrno(WSAGetLastError());
        return -1;
    }
    if (msg->msg_name)
        msg->msg_namelen = (uint32_t)fromLen;
    msg->msg_controllen = 0;
    msg->msg_flags = 0;
    return (int)received;
}

extern "C" int bridgeGetsockname(SOCKET s, struct sockaddr *addr, int *addrlen)
{
    if (getsockname(s, addr, addrlen) == SOCKET_ERROR)
    {
        errno = mapWSAErrorToErrno(WSAGetLastError());
        return -1;
    }
    return 0;
}

extern "C" int bridgeSetsockopt(SOCKET s, int level, int optname, const char *optval, int optlen)
{
    log_info(">>> setsockopt called: socket=%lld, level=%d, optname=%d", (long long)s, level, optname);

    if (level == 1)
    {
        level = 0xFFFF;
        if (optname == 2)
            optname = 0x0004;
    }

    int ret = setsockopt(s, level, optname, optval, optlen);
    if (ret == SOCKET_ERROR)
    {
        errno = mapWSAErrorToErrno(WSAGetLastError());
    }
    log_info(">>> setsockopt EXIT: returning %d", ret);
    return ret;
}

extern "C" int bridgeGetsockopt(SOCKET s, int level, int optname, char *optval, int *optlen)
{
    log_info(">>> getsockopt called: socket=%lld, level=%d, optname=%d", (long long)s, level, optname);

    if (level == 1)
    {
        level = 0xFFFF;
        if (optname == 2)
            optname = 0x0004;
    }

    int ret = getsockopt(s, level, optname, optval, optlen);
    if (ret == SOCKET_ERROR)
    {
        errno = mapWSAErrorToErrno(WSAGetLastError());
    }
    log_info(">>> getsockopt EXIT: returning %d", ret);
    return ret;
}

int NetworkBridge::bridgeGethostbyname_r(const char *name, void *ret, char *buf, size_t buflen, void **result, int *h_errnop)
{
    std::lock_guard<std::mutex> lock(g_net_mutex);
    struct hostent *he = gethostbyname(name);
    if (!he)
    {
        if (h_errnop)
            *h_errnop = WSAGetLastError();
        if (result)
            *result = nullptr;
        return -1;
    }
    memcpy(ret, he, sizeof(struct hostent));
    if (result)
        *result = ret;
    return 0;
}

int NetworkBridge::bridgeGethostbyaddr_r(const void *addr, int len, int type, void *ret, char *buf, size_t buflen, void **result,
                                         int *h_errnop)
{
    std::lock_guard<std::mutex> lock(g_net_mutex);
    struct hostent *he = gethostbyaddr((const char *)addr, len, type);
    if (!he)
    {
        if (h_errnop)
            *h_errnop = WSAGetLastError();
        if (result)
            *result = nullptr;
        return -1;
    }
    memcpy(ret, he, sizeof(struct hostent));
    if (result)
        *result = ret;
    return 0;
}

extern "C" int bridgeGethostname(char *name, size_t namelen)
{
    // fallback to localhost if gethostname fails
    if (name == nullptr || namelen == 0)
    {
        return -1;
    }
    strncpy(name, "localhost", namelen - 1);
    name[namelen - 1] = '\0';
    return 0;
}

#endif