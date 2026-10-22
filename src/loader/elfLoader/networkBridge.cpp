#if defined(_WIN32) || defined(__MINGW32__)
// #include <mutex>
#include <algorithm>
#include <vector>
#include "networkBridge.hpp"
#include "symbolResolver.hpp"
#include "../log/log.h"
#include "../config/config.h"
#include "../redirections/filesystemShared.h"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <mutex>
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <cstring>

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

// getaddrinfo's answer in Linux i386's layout: ai_addr comes before ai_canonname there, after it in Windows', and
// AF_INET6 is 10 (Raw Thrills' Cruis'n Blast, its HASP runtime)
struct LinuxAddrinfo
{
    int flags, family, socktype, protocol;
    uint32_t addrlen;
    sockaddr *addr;
    char *canonname;
    LinuxAddrinfo *next;
};
#define LINUX_AF_INET6 10
#define LINUX_AI_NUMERICSERV 0x400

static int bridgeGetaddrinfo(const char *node, const char *service, const LinuxAddrinfo *hints, LinuxAddrinfo **result)
{
    addrinfo winHints = {}, *winResult = nullptr;
    if (hints)
    {
        winHints.ai_flags = (hints->flags & 0x7) | ((hints->flags & LINUX_AI_NUMERICSERV) ? AI_NUMERICSERV : 0);
        winHints.ai_family = hints->family == LINUX_AF_INET6 ? AF_INET6 : hints->family;
        winHints.ai_socktype = hints->socktype;
        winHints.ai_protocol = hints->protocol;
    }
    int status = getaddrinfo(node, service, hints ? &winHints : nullptr, &winResult);
    if (status != 0)
    {
        *result = nullptr;
        return status == WSAHOST_NOT_FOUND ? -2 : status == WSATRY_AGAIN ? -3 : status == WSAEAFNOSUPPORT ? -6 : -4; // EAI_*
    }
    LinuxAddrinfo *head = nullptr, **tail = &head;
    for (addrinfo *ai = winResult; ai; ai = ai->ai_next)
    {
        LinuxAddrinfo *out = (LinuxAddrinfo *)calloc(1, sizeof(LinuxAddrinfo) + ai->ai_addrlen);
        out->flags = ai->ai_flags;
        out->family = ai->ai_family == AF_INET6 ? LINUX_AF_INET6 : ai->ai_family;
        out->socktype = ai->ai_socktype;
        out->protocol = ai->ai_protocol;
        out->addrlen = (uint32_t)ai->ai_addrlen;
        out->addr = (sockaddr *)(out + 1);
        memcpy(out->addr, ai->ai_addr, ai->ai_addrlen);
        if (ai->ai_family == AF_INET6)
            out->addr->sa_family = LINUX_AF_INET6;
        out->canonname = ai->ai_canonname ? _strdup(ai->ai_canonname) : nullptr;
        *tail = out;
        tail = &out->next;
    }
    freeaddrinfo(winResult);
    *result = head;
    return 0;
}

static void bridgeFreeaddrinfo(LinuxAddrinfo *list)
{
    while (list)
    {
        LinuxAddrinfo *next = list->next;
        free(list->canonname);
        free(list);
        list = next;
    }
}

extern "C" int bridgeShutdown(SOCKET s, int how);

namespace NetworkBridge
{
    void initBridges()
    {
        log_info("Initializing Network Bridges...");

        MAP("getaddrinfo", bridgeGetaddrinfo);
        MAP("freeaddrinfo", bridgeFreeaddrinfo);

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
        MAP("shutdown", bridgeShutdown);
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

// The game's own address, when it sets one (Dead Heat's engine on Maximum Heat 3D: sudo ifconfig eth0 192.168.GROUP.ID,
// filesystemShared.c): this PC answers for it. What comes from this PC (its own multicast, looped back) comes from
// that address, what goes to it goes to 127.0.0.1, and its multicast group is joined; a game that sets none keeps
// the group refused (linuxSockopt). Its boot sees itself so, as a cabinet alone on its link
static uint32_t ownAddress()
{
    const char *address = sharedEth0SetAddress();
    return address ? inet_addr(address) : 0;
}

static bool isThisPc(uint32_t address)
{
    static std::vector<uint32_t> local;
    static std::once_flag once;
    std::call_once(once, [] {
        local.push_back(htonl(INADDR_LOOPBACK));
        char name[256];
        struct addrinfo hints = {}, *list = nullptr;
        hints.ai_family = AF_INET;
        if (gethostname(name, sizeof(name)) == 0 && getaddrinfo(name, nullptr, &hints, &list) == 0)
        {
            for (struct addrinfo *ai = list; ai; ai = ai->ai_next)
                local.push_back(((struct sockaddr_in *)ai->ai_addr)->sin_addr.s_addr);
            freeaddrinfo(list);
        }
    });
    return std::find(local.begin(), local.end(), address) != local.end();
}

static void fromThisPc(void *address)
{
    struct sockaddr_in *in = (struct sockaddr_in *)address;
    uint32_t own = ownAddress();
    if (own && in && in->sin_family == AF_INET && isThisPc(in->sin_addr.s_addr))
        in->sin_addr.s_addr = own;
}

// For sysBridge.cpp's getpeername
extern "C" void networkFromThisPc(void *address)
{
    fromThisPc(address);
}

static const struct sockaddr *toThisPc(const void *address, struct sockaddr_in *copy)
{
    const struct sockaddr_in *in = (const struct sockaddr_in *)address;
    uint32_t own = ownAddress();
    if (!own || !in || in->sin_family != AF_INET || in->sin_addr.s_addr != own)
        return (const struct sockaddr *)address;
    *copy = *in;
    copy->sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    return (const struct sockaddr *)copy;
}

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
    struct sockaddr_in local;
    int ret = connect(s, toThisPc(name, &local), namelen);
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
    struct sockaddr_in any;
    const struct sockaddr_in *in = (const struct sockaddr_in *)name;
    uint32_t own = ownAddress();
    if (own && in && in->sin_family == AF_INET && in->sin_addr.s_addr == own) // not one of Windows' addresses
    {
        any = *in;
        any.sin_addr.s_addr = htonl(INADDR_ANY);
        name = (const struct sockaddr *)&any;
    }
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
    fromThisPc(addr);
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
    else
        fromThisPc(from);
    log_info(">>> recvfrom EXIT: returning %d", ret);
    return ret;
}

extern "C" int bridgeSendto(SOCKET s, const char *buf, int len, int flags, const struct sockaddr *to, int tolen)
{
    log_info(">>> sendto called: socket=%lld, len=%d", (long long)s, len);
    flags &= ~(0x4000 | 0x40);
    struct sockaddr_in local;
    int ret = sendto(s, buf, len, flags, toThisPc(to, &local), tolen);
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
    const struct sockaddr_in *to = (const struct sockaddr_in *)msg->msg_name;
    struct sockaddr_in local;
    if (WSASendTo(s, bufs, msg->msg_iovlen, &sent, flags & ~(0x4000 | 0x40), toThisPc(msg->msg_name, &local),
                  msg->msg_name ? (int)msg->msg_namelen : 0, NULL, NULL) == SOCKET_ERROR)
    {
        int error = WSAGetLastError();
        log_info(">>> sendmsg: socket=%lld to %s:%u failed (%d)", (long long)s, to ? inet_ntoa(to->sin_addr) : "-",
                 to ? ntohs(to->sin_port) : 0, error);
        errno = mapWSAErrorToErrno(error);
        return -1;
    }
    log_info(">>> sendmsg: socket=%lld to %s:%u, %lu bytes", (long long)s, to ? inet_ntoa(to->sin_addr) : "-",
             to ? ntohs(to->sin_port) : 0, (unsigned long)sent);
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
        int error = WSAGetLastError();
        if (error != WSAEWOULDBLOCK)
            log_info(">>> recvmsg: socket=%lld failed (%d)", (long long)s, error);
        errno = mapWSAErrorToErrno(error);
        return -1;
    }
    fromThisPc(msg->msg_name);
    const struct sockaddr_in *from = (const struct sockaddr_in *)msg->msg_name;
    unsigned long room = 0;
    for (uint32_t i = 0; i < msg->msg_iovlen && i < 16; i++)
        room += bufs[i].len;
    log_info(">>> recvmsg: socket=%lld from %s:%u, %lu bytes (room %lu in %u, flags 0x%x)", (long long)s,
             from ? inet_ntoa(from->sin_addr) : "-", from ? ntohs(from->sin_port) : 0, (unsigned long)received, room,
             (unsigned)msg->msg_iovlen, (unsigned)flags);
    if (msg->msg_name)
        msg->msg_namelen = (uint32_t)fromLen;
    msg->msg_controllen = 0;
    msg->msg_flags = 0;
    return (int)received;
}

// Winsock's shutdown is __stdcall, the games call it as cdecl; SHUT_RD, SHUT_WR, SHUT_RDWR are SD_RECEIVE, SD_SEND, SD_BOTH
extern "C" int bridgeShutdown(SOCKET s, int how)
{
    log_info(">>> shutdown: socket=%lld, how=%d", (long long)s, how);
    if (shutdown(s, how) == SOCKET_ERROR)
    {
        errno = mapWSAErrorToErrno(WSAGetLastError());
        return -1;
    }
    return 0;
}

extern "C" int bridgeGetsockname(SOCKET s, struct sockaddr *addr, int *addrlen)
{
    if (getsockname(s, addr, addrlen) == SOCKET_ERROR)
    {
        errno = mapWSAErrorToErrno(WSAGetLastError());
        return -1;
    }
    fromThisPc(addr);
    return 0;
}

// Linux's socket and IP option numbers to Winsock's: SOL_SOCKET 1 is 0xFFFF, IPPROTO_IP keeps its 0 but not its
// options. False for the options refused: joining a multicast group when the game has not set its own address (see
// ownAddress): Dead Heat then plays alone, while in the group its boot waits for the other cabinets of its link and
// ends on "Duplicate PCB-IDs found"
static bool linuxSockopt(int &level, int &optname)
{
    if (level == 1)
    {
        level = SOL_SOCKET;
        switch (optname)
        {
            case 2: optname = SO_REUSEADDR; break;
            case 3: optname = SO_TYPE; break;
            case 4: optname = SO_ERROR; break;
            case 5: optname = SO_DONTROUTE; break;
            case 6: optname = SO_BROADCAST; break;
            case 7: optname = SO_SNDBUF; break;
            case 8: optname = SO_RCVBUF; break;
            case 9: optname = SO_KEEPALIVE; break;
            case 10: optname = SO_OOBINLINE; break;
            case 13: optname = SO_LINGER; break;
            case 20: optname = SO_RCVTIMEO; break;
            case 21: optname = SO_SNDTIMEO; break;
            default: log_warn("Linux socket option %d not translated", optname); break;
        }
    }
    else if (level == 0)
    {
        switch (optname)
        {
            case 1: optname = IP_TOS; break;
            case 2: optname = IP_TTL; break;
            case 32: optname = IP_MULTICAST_IF; break;
            case 33: optname = IP_MULTICAST_TTL; break;
            case 34: optname = IP_MULTICAST_LOOP; break;
            case 35: // IP_ADD_MEMBERSHIP
            case 36: // IP_DROP_MEMBERSHIP
                if (!ownAddress())
                    return false;
                optname = optname == 35 ? IP_ADD_MEMBERSHIP : IP_DROP_MEMBERSHIP;
                break;
            default: log_warn("Linux IP option %d not translated", optname); break;
        }
    }
    return true;
}

// The values that differ: the timeouts (a timeval on Linux, milliseconds here), linger (two ints, two shorts) and
// the multicast loop and TTL (a byte allowed on Linux, a DWORD here)
struct LinuxTimeval
{
    int32_t sec, usec;
};

extern "C" int bridgeSetsockopt(SOCKET s, int level, int optname, const char *optval, int optlen)
{
    log_info(">>> setsockopt called: socket=%lld, level=%d, optname=%d", (long long)s, level, optname);

    if (!linuxSockopt(level, optname))
    {
        errno = mapWSAErrorToErrno(WSAENOPROTOOPT);
        log_info(">>> setsockopt EXIT: refused");
        return -1;
    }
    DWORD dword;
    struct linger linger;
    uint32_t mreq[2] = {0, 0}; // ip_mreq: group, interface; IP_MULTICAST_IF: the interface
    if (level == SOL_SOCKET && (optname == SO_RCVTIMEO || optname == SO_SNDTIMEO) && optval && optlen >= (int)sizeof(LinuxTimeval))
    {
        const LinuxTimeval *tv = (const LinuxTimeval *)optval;
        dword = (DWORD)tv->sec * 1000 + (DWORD)tv->usec / 1000;
        optval = (const char *)&dword;
        optlen = sizeof(dword);
    }
    else if (level == SOL_SOCKET && optname == SO_LINGER && optval && optlen >= 8)
    {
        linger.l_onoff = (u_short)((const int32_t *)optval)[0];
        linger.l_linger = (u_short)((const int32_t *)optval)[1];
        optval = (const char *)&linger;
        optlen = sizeof(linger);
    }
    else if (level == 0 && (optname == IP_ADD_MEMBERSHIP || optname == IP_DROP_MEMBERSHIP || optname == IP_MULTICAST_IF) &&
             optval && optlen >= 4)
    {
        // the game's own address as the interface: any (it is not one of Windows')
        memcpy(mreq, optval, optlen < (int)sizeof(mreq) ? (size_t)optlen : sizeof(mreq));
        int interfaceIndex = optname == IP_MULTICAST_IF ? 0 : 1;
        if (ownAddress() && mreq[interfaceIndex] == ownAddress())
            mreq[interfaceIndex] = htonl(INADDR_ANY);
        optval = (const char *)mreq;
    }
    else if (level == 0 && (optname == IP_MULTICAST_LOOP || optname == IP_MULTICAST_TTL) && optval && optlen == 1)
    {
        dword = (unsigned char)optval[0];
        optval = (const char *)&dword;
        optlen = sizeof(dword);
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

    if (!linuxSockopt(level, optname))
    {
        errno = mapWSAErrorToErrno(WSAENOPROTOOPT);
        return -1;
    }
    if (level == SOL_SOCKET && (optname == SO_RCVTIMEO || optname == SO_SNDTIMEO) && optval && optlen &&
        *optlen >= (int)sizeof(LinuxTimeval))
    {
        DWORD ms = 0;
        int msLength = sizeof(ms);
        int ret = getsockopt(s, level, optname, (char *)&ms, &msLength);
        if (ret == SOCKET_ERROR)
        {
            errno = mapWSAErrorToErrno(WSAGetLastError());
            return ret;
        }
        LinuxTimeval *tv = (LinuxTimeval *)optval;
        tv->sec = (int32_t)(ms / 1000);
        tv->usec = (int32_t)(ms % 1000) * 1000;
        *optlen = sizeof(LinuxTimeval);
        return 0;
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