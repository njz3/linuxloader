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
#include <ctime>

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
                    cp = sharedSrtvAddress();
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
    // The link on one PC: the game's address is this copy's (eth0Address), nothing to answer for
    if (sharedLocalLinkAddress())
        return 0;
    const char *address = sharedEth0SetAddress();
    return address ? inet_addr(address) : 0;
}

static const std::vector<uint32_t> &thisPcAddresses()
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
    return local;
}

static bool isThisPc(uint32_t address)
{
    const std::vector<uint32_t> &local = thisPcAddresses();
    return std::find(local.begin(), local.end(), address) != local.end();
}

// This PC's address in the /24 of the game's own (Namco's 192.168.GROUP.x): the cabinets' network. 0 for none
static uint32_t lanAddressOf(uint32_t gameAddress)
{
    for (uint32_t address : thisPcAddresses())
        if (address != htonl(INADDR_LOOPBACK) && ((address ^ gameAddress) & htonl(0xFFFFFF00)) == 0)
            return address;
    return 0;
}

// Dead Heat Riders takes another cabinet's packets only from a source whose last number has its two last digits at 4
// or less (clNet::isResionCheck: .4, .41, .42 pass, .5 not): its cabinets' own addresses, 192.168.G.(10 * TOTAL + PCB
// ID), always do, the PCs' not (racecab2, 192.168.1.5). An other cabinet is shown to it under its game address, learnt
// from its status packets (36 bytes, type 0x1903 at +0x14, its PCB ID - 1 at +0x1a and TOTAL at +0x1b), and that
// address taken back to its PC's for what the game sends it or connects to
#define LINK_PEERS 8
static struct
{
    uint32_t game, pc; // network order
} linkPeers[LINK_PEERS];
static int linkPeerCount = 0;
static SRWLOCK linkPeerLock = SRWLOCK_INIT;

static void linkPeerLearn(const struct sockaddr_in *from, const WSABUF *bufs, DWORD count, DWORD received)
{
    uint32_t own = ownAddress();
    if (getConfig()->crc32 != DEAD_HEAT_RIDERS || !own || !from || from->sin_family != AF_INET || received != 36 ||
        isThisPc(from->sin_addr.s_addr))
        return;
    unsigned char packet[36];
    DWORD copied = 0;
    for (DWORD i = 0; i < count && copied < sizeof(packet); i++)
    {
        DWORD part = bufs[i].len < sizeof(packet) - copied ? bufs[i].len : sizeof(packet) - copied;
        memcpy(packet + copied, bufs[i].buf, part);
        copied += part;
    }
    uint32_t type;
    memcpy(&type, packet + 0x14, 4);
    if (copied < sizeof(packet) || type != 0x1903)
        return;
    uint32_t game = htonl((ntohl(own) & 0xFFFFFF00) | ((packet[0x1a] + 10u * packet[0x1b] + 1u) & 0xFF));
    AcquireSRWLockExclusive(&linkPeerLock);
    int found = 0;
    for (int i = 0; i < linkPeerCount && !found; i++)
        if (linkPeers[i].pc == from->sin_addr.s_addr)
        {
            found = 1;
            linkPeers[i].game = game;
        }
    if (!found && linkPeerCount < LINK_PEERS)
    {
        linkPeers[linkPeerCount].game = game;
        linkPeers[linkPeerCount++].pc = from->sin_addr.s_addr;
        char a[16], b[16];
        InetNtopA(AF_INET, &game, a, sizeof(a));
        InetNtopA(AF_INET, (void *)&from->sin_addr, b, sizeof(b));
        log_warn("Link: the cabinet at %s shown to the game as %s", b, a);
    }
    ReleaseSRWLockExclusive(&linkPeerLock);
}

// An other cabinet's address, its PC's to the game's (toGame) or back
static uint32_t linkPeerSwap(uint32_t address, bool toGame)
{
    uint32_t swapped = address;
    AcquireSRWLockShared(&linkPeerLock);
    for (int i = 0; i < linkPeerCount; i++)
        if ((toGame ? linkPeers[i].pc : linkPeers[i].game) == address)
            swapped = toGame ? linkPeers[i].game : linkPeers[i].pc;
    ReleaseSRWLockShared(&linkPeerLock);
    return swapped;
}

static void fromThisPc(void *address)
{
    struct sockaddr_in *in = (struct sockaddr_in *)address;
    uint32_t own = ownAddress();
    if (own && in && in->sin_family == AF_INET && isThisPc(in->sin_addr.s_addr))
        in->sin_addr.s_addr = own;
    else if (own && in && in->sin_family == AF_INET && linkPeerCount)
        in->sin_addr.s_addr = linkPeerSwap(in->sin_addr.s_addr, true);
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
    if (!own || !in || in->sin_family != AF_INET)
        return (const struct sockaddr *)address;
    uint32_t pc = in->sin_addr.s_addr == own ? htonl(INADDR_LOOPBACK)
                  : linkPeerCount             ? linkPeerSwap(in->sin_addr.s_addr, false)
                                              : in->sin_addr.s_addr;
    if (pc == in->sin_addr.s_addr)
        return (const struct sockaddr *)address;
    *copy = *in;
    copy->sin_addr.s_addr = pc;
    return (const struct sockaddr *)copy;
}

// The link on one PC (sharedLocalLinkAddress): each copy of the loader has its own loopback address, 127.0.0.2,
// 127.0.0.3... Its sockets are bound to it, those the game binds to any address and those it sends from unbound (Windows
// would send them from 127.0.0.1), so that the copies share their ports and know each other's packets. Windows'
// loopback carries no broadcast: one goes to each of the LOCAL_LINK_COPIES addresses from 127.0.0.2, the sender's too,
// as a broadcast comes back on Linux. A multicast the same (Namco's 225.0.0.1), its group joined without Winsock
#define LOCAL_LINK_COPIES 8

static uint32_t localLinkAddress()
{
    const char *address = sharedLocalLinkAddress();
    return address ? inet_addr(address) : 0;
}

static bool isLocalLinkBroadcast(const struct sockaddr *address)
{
    const struct sockaddr_in *in = (const struct sockaddr_in *)address;
    return in && in->sin_family == AF_INET && localLinkAddress() &&
           (in->sin_addr.s_addr == htonl(INADDR_BROADCAST) || (ntohl(in->sin_addr.s_addr) & 0xFF) == 0xFF ||
            (ntohl(in->sin_addr.s_addr) >> 28) == 0xE);
}

// A socket not bound yet, before it sends: bound to this copy's address
static void bindToLocalLink(SOCKET s)
{
    uint32_t own = localLinkAddress();
    struct sockaddr_in bound = {};
    int length = sizeof(bound);
    if (!own || getsockname(s, (struct sockaddr *)&bound, &length) == 0 || WSAGetLastError() != WSAEINVAL)
        return;
    bound.sin_family = AF_INET;
    bound.sin_addr.s_addr = own;
    bound.sin_port = 0;
    if (bind(s, (const struct sockaddr *)&bound, sizeof(bound)) == SOCKET_ERROR)
        log_warn("Local link: socket not bound to its copy's address (%d)", WSAGetLastError());
}

// Gaelco's link broadcasts to the .255 of its Base IP, 90.0.0.255 as the cabinets came (Ring Riders always to that
// one), which is no network of this PC's: Windows would send it to its gateway. Between cabinets it goes to the
// broadcast of this PC's network
static const char *addressText(const void *address, char *text, size_t size);

static const struct sockaddr *toLanBroadcast(const struct sockaddr *address, struct sockaddr_in *copy)
{
    const struct sockaddr_in *in = (const struct sockaddr_in *)address;
    uint32_t lan, mask;
    if (getConfig()->gameGroup != GROUP_GAELCO || !in || in->sin_family != AF_INET ||
        (ntohl(in->sin_addr.s_addr) & 0xFF) != 0xFF || lanAddressOf(in->sin_addr.s_addr) || !sharedLanAddress(&lan, &mask))
        return address;
    *copy = *in;
    copy->sin_addr.s_addr = lan | ~mask;
    static bool told = false;
    if (!told)
    {
        char from[32], to[32];
        log_warn("Link: the broadcasts to %s go to %s, this PC's network", addressText(address, from, sizeof(from)),
                 addressText(copy, to, sizeof(to)));
        told = true;
    }
    return (const struct sockaddr *)copy;
}

// A broadcast on the link on one PC, sent to each copy: the length sent when one copy at least got it
static int sendToLocalLink(SOCKET s, WSABUF *bufs, DWORD count, int flags, const struct sockaddr *to)
{
    struct sockaddr_in copy = *(const struct sockaddr_in *)to;
    int sent = SOCKET_ERROR, error = 0;
    for (int n = 2; n < 2 + LOCAL_LINK_COPIES; n++)
    {
        copy.sin_addr.s_addr = htonl((INADDR_LOOPBACK & 0xFFFFFF00) | n);
        DWORD bytes = 0;
        if (WSASendTo(s, bufs, count, &bytes, flags, (const struct sockaddr *)&copy, sizeof(copy), NULL, NULL) == 0)
            sent = (int)bytes;
        else
            error = WSAGetLastError();
    }
    if (sent == SOCKET_ERROR)
        WSASetLastError(error);
    return sent;
}

// Initial D 4 Japan's in-shop server box, which the loader plays (cStoreServerMonitor: UDP 13346, 0x210-byte "IDBP"
// packets, the command at +4). A seat's search (0x0e, a broadcast to 192.168.37.255) is answered as the server (0x0f,
// seat 0xff), which the seat then finds at the answer's source, 127.0.0.1; its alives to the server (0x10) are answered,
// its pairing request (4) goes on to the other seat. The seats then talk to each other: their alives, the match server
// on the odd seat (UDP 12222), the race (UDP 13349). No in-shop ranking (the box's TCP 9876)
#define ID4_STORE_PORT 13346
#define ID4_STORE_PACKET 0x210

static void id4ServerAnswer(const char *request, uint32_t command)
{
    static SOCKET server = INVALID_SOCKET;
    if (server == INVALID_SOCKET)
    {
        server = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        struct sockaddr_in local = {};
        local.sin_family = AF_INET;
        local.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        bind(server, (const struct sockaddr *)&local, sizeof(local));
    }
    char answer[ID4_STORE_PACKET];
    memcpy(answer, request, sizeof(answer));
    uint32_t value = command;
    memcpy(answer + 0x4, &value, 4);
    value = (uint32_t)time(NULL);
    memcpy(answer + 0xc, &value, 4);
    memcpy(&value, answer + 0x20, 4);
    value |= 1; // a packet without it is ignored
    memcpy(answer + 0x20, &value, 4);
    value = INADDR_LOOPBACK; // the sender's address, host order
    memcpy(answer + 0x24, &value, 4);
    answer[0x2a] = (char)0xff; // the server's seat
    answer[0x2b] = 'X';
    value = 0;
    memcpy(answer + 0x2c, &value, 4);
    // To the seat's socket, bound to any address on a network, to this copy's on one PC
    struct sockaddr_in seat = {};
    seat.sin_family = AF_INET;
    seat.sin_port = htons(ID4_STORE_PORT);
    seat.sin_addr.s_addr = sharedLocalLinkAddress() ? inet_addr(sharedLocalLinkAddress()) : htonl(INADDR_LOOPBACK);
    sendto(server, answer, sizeof(answer), 0, (const struct sockaddr *)&seat, sizeof(seat));
}

// True when the packet is the server box's business, *sent its length
static bool id4ServerBox(SOCKET s, const char *buf, int len, const struct sockaddr *to, int *sent)
{
    const struct sockaddr_in *in = (const struct sockaddr_in *)to;
    if (getConfig()->gameGroup != GROUP_ID4_JAP || len != ID4_STORE_PACKET || memcmp(buf, "IDBP", 4) != 0 || !in ||
        in->sin_family != AF_INET || ntohs(in->sin_port) != ID4_STORE_PORT)
        return false;
    uint32_t command;
    memcpy(&command, buf + 4, 4);
    bool toServer = in->sin_addr.s_addr == htonl(INADDR_LOOPBACK);
    if (command != 0x0e && !toServer)
        return false; // to the other seat
    *sent = len;
    if (command == 0x0e)
        id4ServerAnswer(buf, 0x0f);
    else if (command == 0x10)
        id4ServerAnswer(buf, 0x10);
    else if (command == 4)
    {
        // The pairing request, on to the other seat: the copies on one PC, or ID_IP_SEAT_1 or 2, the one not this PC's
        struct sockaddr_in other = *in;
        if (sharedLocalLinkAddress())
        {
            WSABUF buffer = {(ULONG)len, (CHAR *)buf};
            sendToLocalLink(s, &buffer, 1, 0, (const struct sockaddr *)&other);
        }
        else
        {
            const char *seats[2] = {getConfig()->idIpSeat1, getConfig()->idIpSeat2};
            for (const char *seat : seats)
            {
                other.sin_addr.s_addr = inet_addr(seat);
                if (seat[0] && other.sin_addr.s_addr != INADDR_NONE && !isThisPc(other.sin_addr.s_addr))
                    sendto(s, buf, len, 0, (const struct sockaddr *)&other, sizeof(other));
            }
        }
    }
    log_info(">>> sendto: Initial D's server box, command 0x%x", command);
    return true;
}

// Windows makes a UDP socket's next recvfrom fail with WSAECONNRESET when one of its datagrams found no one listening
// (an ICMP port unreachable), Linux does not: a link cabinet sending before the other listens would take it for an
// error
#ifndef SIO_UDP_CONNRESET
#define SIO_UDP_CONNRESET _WSAIOW(IOC_VENDOR, 12)
#endif

// "a.b.c.d:port" of an IPv4 address, for the log (the link between cabinets: who talks to whom)
static const char *addressText(const void *address, char *text, size_t size)
{
    const struct sockaddr_in *in = (const struct sockaddr_in *)address;
    char ip[16] = "";
    if (!in || in->sin_family != AF_INET || !InetNtopA(AF_INET, (void *)&in->sin_addr, ip, sizeof(ip)))
        snprintf(text, size, "-");
    else
        snprintf(text, size, "%s:%u", ip, ntohs(in->sin_port));
    return text;
}

// The Winsock socket behind one of the game's descriptors (filesystemShared.c): INVALID_SOCKET for another number,
// which Winsock then refuses (ENOTSOCK)
static SOCKET gameSocket(SOCKET fd)
{
    return sharedSocketOf((int)fd);
}

extern "C" SOCKET bridgeSocket(int af, int type, int protocol)
{
    log_info(">>> socket called: af=%d, type=%d, protocol=%d", af, type, protocol);
    // SOCK_PACKET, Linux's raw Ethernet socket: Gaelco's games ask eth0's MAC with it (Ring Riders numbers its cabinets
    // with it) and set eth0's address, ioctls only (sharedIoctl). A UDP socket stands for it
    if (af == AF_INET && type == 10)
    {
        log_info(">>> socket: SOCK_PACKET, a UDP socket for its ioctls");
        type = SOCK_DGRAM;
        protocol = 0;
    }
    SOCKET s = socket(af, type, protocol);
    if (s == INVALID_SOCKET)
    {
        errno = mapWSAErrorToErrno(WSAGetLastError());
        return (SOCKET)-1;
    }
    if ((type & 0xF) == SOCK_DGRAM)
    {
        BOOL report = FALSE;
        DWORD returned = 0;
        WSAIoctl(s, SIO_UDP_CONNRESET, &report, sizeof(report), NULL, 0, &returned, NULL, NULL);
    }
    int fd = sharedSocketFdNew(s);
    if (fd < 0)
    {
        closesocket(s);
        errno = 24; // EMFILE
        return (SOCKET)-1;
    }
    log_info(">>> socket EXIT: returning %d", fd);
    return (SOCKET)fd;
}

extern "C" int bridgeConnect(SOCKET fd, const struct sockaddr *name, int namelen)
{
    SOCKET s = gameSocket(fd);
    char text[32];
    log_info(">>> connect called: socket=%lld to %s", (long long)fd, addressText(name, text, sizeof(text)));
    struct sockaddr_in local;
    bindToLocalLink(s);
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

extern "C" int bridgeBind(SOCKET fd, const struct sockaddr *name, int namelen)
{
    SOCKET s = gameSocket(fd);
    char text[32];
    log_info(">>> bind called: socket=%lld on %s", (long long)fd, addressText(name, text, sizeof(text)));
    struct sockaddr_in any;
    const struct sockaddr_in *in = (const struct sockaddr_in *)name;
    uint32_t own = ownAddress();
    if (own && in && in->sin_family == AF_INET && in->sin_addr.s_addr == own) // not one of Windows' addresses
    {
        any = *in;
        any.sin_addr.s_addr = htonl(INADDR_ANY);
        name = (const struct sockaddr *)&any;
    }
    // The link on one PC: any address, or a broadcast one, is this copy's; another copy's is not this PC's, as on a
    // network (Hummer binds to each cabinet's address, and keeps the one it gets)
    uint32_t localLink = localLinkAddress();
    if (localLink && in && in->sin_family == AF_INET && (ntohl(in->sin_addr.s_addr) >> 24) == 127 &&
        in->sin_addr.s_addr != localLink && in->sin_addr.s_addr != htonl(INADDR_LOOPBACK) && !isLocalLinkBroadcast(name))
    {
        log_info(">>> bind EXIT: %s is another copy's address, the link on one PC", text);
        errno = 99; // EADDRNOTAVAIL
        return -1;
    }
    if (localLink && in && in->sin_family == AF_INET &&
        (in->sin_addr.s_addr == htonl(INADDR_ANY) || isLocalLinkBroadcast(name)))
    {
        any = *in;
        any.sin_addr.s_addr = localLink;
        name = (const struct sockaddr *)&any;
        log_info(">>> bind: on this copy's address, the link on one PC");
    }
    int ret = bind(s, name, namelen);
    // A UDP socket bound to a broadcast address (255.255.255.255, or a.b.c.255 as SIOCGIFBRDADDR gives it) receives
    // that address's broadcasts on Linux; Windows refuses the address, and a socket bound to any address receives
    // them: SEGA Race TV's main cabinet listens so for its sub cabinets' search
    int type = 0, typeLength = sizeof(type);
    if (ret == SOCKET_ERROR && WSAGetLastError() == WSAEADDRNOTAVAIL && in && in->sin_family == AF_INET &&
        (in->sin_addr.s_addr == htonl(INADDR_BROADCAST) || (ntohl(in->sin_addr.s_addr) & 0xFF) == 0xFF) &&
        getsockopt(s, SOL_SOCKET, SO_TYPE, (char *)&type, &typeLength) == 0 && type == SOCK_DGRAM)
    {
        any = *in;
        any.sin_addr.s_addr = htonl(INADDR_ANY);
        ret = bind(s, (const struct sockaddr *)&any, namelen);
        log_info(">>> bind: %s is a broadcast address, bound to any address on its port instead", text);
    }
    if (ret == SOCKET_ERROR)
    {
        errno = mapWSAErrorToErrno(WSAGetLastError());
    }
    log_info(">>> bind EXIT: returning %d", ret);
    return ret;
}

extern "C" int bridgeListen(SOCKET fd, int backlog)
{
    SOCKET s = gameSocket(fd);
    log_info(">>> listen called: socket=%lld, backlog=%d", (long long)fd, backlog);
    int ret = listen(s, backlog);
    if (ret == SOCKET_ERROR)
    {
        errno = mapWSAErrorToErrno(WSAGetLastError());
    }
    log_info(">>> listen EXIT: returning %d", ret);
    return ret;
}

extern "C" SOCKET bridgeAccept(SOCKET fd, struct sockaddr *addr, int *addrlen)
{
    SOCKET s = gameSocket(fd);
    log_info(">>> accept ENTRY: socket=%lld (THIS MAY BLOCK!)", (long long)fd);
    SOCKET accepted = accept(s, addr, addrlen);
    if (accepted == INVALID_SOCKET)
    {
        errno = mapWSAErrorToErrno(WSAGetLastError());
        return (SOCKET)-1;
    }
    int ret = sharedSocketFdNew(accepted);
    if (ret < 0)
    {
        closesocket(accepted);
        errno = 24; // EMFILE
        return (SOCKET)-1;
    }
    fromThisPc(addr);
    log_info(">>> accept EXIT: returning %d", ret);
    return (SOCKET)ret;
}

extern "C" int bridgeRecv(SOCKET fd, char *buf, int len, int flags)
{
    SOCKET s = gameSocket(fd);
    log_info(">>> recv ENTRY: socket=%lld, len=%d (THIS MAY BLOCK!)", (long long)fd, len);

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

extern "C" int bridgeSend(SOCKET fd, const char *buf, int len, int flags)
{
    SOCKET s = gameSocket(fd);
    log_info(">>> send called: socket=%lld, len=%d", (long long)fd, len);

    flags &= ~(0x4000 | 0x40);

    int ret = send(s, buf, len, flags);
    if (ret == SOCKET_ERROR)
    {
        errno = mapWSAErrorToErrno(WSAGetLastError());
    }
    log_info(">>> send EXIT: returning %d", ret);
    return ret;
}

extern "C" int bridgeRecvfrom(SOCKET fd, char *buf, int len, int flags, struct sockaddr *from, int *fromlen)
{
    SOCKET s = gameSocket(fd);
    log_info(">>> recvfrom ENTRY: socket=%lld, len=%d (THIS MAY BLOCK!)", (long long)fd, len);
    sharedSigioPoll();
    if (flags & 0x40)
        flags &= ~0x40;
    int ret = recvfrom(s, buf, len, flags, from, fromlen);
    char text[32];
    if (ret == SOCKET_ERROR)
    {
        int wsaError = WSAGetLastError();
        errno = mapWSAErrorToErrno(wsaError);
        log_info(">>> recvfrom EXIT: returning %d (WSAError=%d)", ret, wsaError);
        return ret;
    }
    fromThisPc(from);
    log_info(">>> recvfrom EXIT: returning %d from %s", ret, addressText(from, text, sizeof(text)));
    return ret;
}

extern "C" int bridgeSendto(SOCKET fd, const char *buf, int len, int flags, const struct sockaddr *to, int tolen)
{
    SOCKET s = gameSocket(fd);
    log_info(">>> sendto called: socket=%lld, len=%d", (long long)fd, len);
    sharedSigioPoll();
    flags &= ~(0x4000 | 0x40);
    struct sockaddr_in local;
    bindToLocalLink(s);
    int ret;
    if (id4ServerBox(s, buf, len, to, &ret))
        ;
    else if (isLocalLinkBroadcast(to))
    {
        WSABUF buffer = {(ULONG)len, (CHAR *)buf};
        ret = sendToLocalLink(s, &buffer, 1, flags, to);
    }
    else
    {
        struct sockaddr_in broadcast;
        ret = sendto(s, buf, len, flags, toThisPc(toLanBroadcast(to, &broadcast), &local), tolen);
    }
    char text[32];
    if (ret == SOCKET_ERROR)
    {
        int wsaError = WSAGetLastError();
        errno = mapWSAErrorToErrno(wsaError);
        log_info(">>> sendto EXIT: returning %d to %s (WSAError=%d)", ret, addressText(to, text, sizeof(text)), wsaError);
        return ret;
    }
    log_info(">>> sendto EXIT: returning %d to %s", ret, addressText(to, text, sizeof(text)));
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

extern "C" int bridgeSendmsg(SOCKET fd, const LinuxMsghdr *msg, int flags)
{
    SOCKET s = gameSocket(fd);
    WSABUF bufs[16];
    if (!msghdrToWsabuf(msg, bufs, 16))
    {
        errno = EINVAL;
        return -1;
    }
    DWORD sent = 0;
    const struct sockaddr_in *to = (const struct sockaddr_in *)msg->msg_name;
    struct sockaddr_in local;
    bindToLocalLink(s);
    int ret;
    if (isLocalLinkBroadcast((const struct sockaddr *)to))
        ret = sendToLocalLink(s, bufs, msg->msg_iovlen, flags & ~(0x4000 | 0x40), (const struct sockaddr *)to);
    else
        ret = WSASendTo(s, bufs, msg->msg_iovlen, &sent, flags & ~(0x4000 | 0x40), toThisPc(msg->msg_name, &local),
                        msg->msg_name ? (int)msg->msg_namelen : 0, NULL, NULL) == SOCKET_ERROR
                  ? SOCKET_ERROR
                  : (int)sent;
    if (ret == SOCKET_ERROR)
    {
        int error = WSAGetLastError();
        log_info(">>> sendmsg: socket=%lld to %s:%u failed (%d)", (long long)fd, to ? inet_ntoa(to->sin_addr) : "-",
                 to ? ntohs(to->sin_port) : 0, error);
        errno = mapWSAErrorToErrno(error);
        return -1;
    }
    log_info(">>> sendmsg: socket=%lld to %s:%u, %lu bytes", (long long)fd, to ? inet_ntoa(to->sin_addr) : "-",
             to ? ntohs(to->sin_port) : 0, (unsigned long)ret);
    return ret;
}

extern "C" int bridgeRecvmsg(SOCKET fd, LinuxMsghdr *msg, int flags)
{
    SOCKET s = gameSocket(fd);
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
            log_info(">>> recvmsg: socket=%lld failed (%d)", (long long)fd, error);
        errno = mapWSAErrorToErrno(error);
        return -1;
    }
    linkPeerLearn((const struct sockaddr_in *)msg->msg_name, bufs, msg->msg_iovlen, received);
    fromThisPc(msg->msg_name);
    const struct sockaddr_in *from = (const struct sockaddr_in *)msg->msg_name;
    unsigned long room = 0;
    for (uint32_t i = 0; i < msg->msg_iovlen && i < 16; i++)
        room += bufs[i].len;
    log_info(">>> recvmsg: socket=%lld from %s:%u, %lu bytes (room %lu in %u, flags 0x%x)", (long long)fd,
             from ? inet_ntoa(from->sin_addr) : "-", from ? ntohs(from->sin_port) : 0, (unsigned long)received, room,
             (unsigned)msg->msg_iovlen, (unsigned)flags);
    if (msg->msg_name)
        msg->msg_namelen = (uint32_t)fromLen;
    msg->msg_controllen = 0;
    msg->msg_flags = 0;
    return (int)received;
}

// Winsock's shutdown is __stdcall, the games call it as cdecl; SHUT_RD, SHUT_WR, SHUT_RDWR are SD_RECEIVE, SD_SEND, SD_BOTH
extern "C" int bridgeShutdown(SOCKET fd, int how)
{
    SOCKET s = gameSocket(fd);
    log_info(">>> shutdown: socket=%lld, how=%d", (long long)fd, how);
    if (shutdown(s, how) == SOCKET_ERROR)
    {
        errno = mapWSAErrorToErrno(WSAGetLastError());
        return -1;
    }
    return 0;
}

extern "C" int bridgeGetsockname(SOCKET fd, struct sockaddr *addr, int *addrlen)
{
    SOCKET s = gameSocket(fd);
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
                if (!ownAddress() && !sharedLocalLinkAddress())
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

extern "C" int bridgeSetsockopt(SOCKET fd, int level, int optname, const char *optval, int optlen)
{
    SOCKET s = gameSocket(fd);
    log_info(">>> setsockopt called: socket=%lld, level=%d, optname=%d", (long long)fd, level, optname);

    // IP_MTU_DISCOVER (Gaelco's link): Winsock has no such option (its 10 is IP_MULTICAST_TTL)
    if (level == 0 && optname == 10)
    {
        log_info(">>> setsockopt EXIT: IP_MTU_DISCOVER taken");
        return 0;
    }
    if (!linuxSockopt(level, optname))
    {
        errno = mapWSAErrorToErrno(WSAENOPROTOOPT);
        log_info(">>> setsockopt EXIT: refused");
        return -1;
    }
    // The link on one PC: a group is joined by the copies' fan-out (sendToLocalLink), Winsock's loopback has none
    if (sharedLocalLinkAddress() && level == 0 && (optname == IP_ADD_MEMBERSHIP || optname == IP_DROP_MEMBERSHIP))
    {
        log_info(">>> setsockopt EXIT: group taken, the link on one PC");
        return 0;
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
        // A link between cabinets: the group on the PC's address in the game's /24, and its sends leaving there, not
        // on whichever adapter Windows picks (a VPN's)
        uint32_t lan = ownAddress() ? lanAddressOf(ownAddress()) : 0;
        if (lan && mreq[interfaceIndex] == htonl(INADDR_ANY))
        {
            mreq[interfaceIndex] = lan;
            if (optname == IP_ADD_MEMBERSHIP)
                setsockopt(s, IPPROTO_IP, IP_MULTICAST_IF, (const char *)&lan, sizeof(lan));
            char text[32];
            struct sockaddr_in in = {};
            in.sin_family = AF_INET;
            in.sin_addr.s_addr = lan;
            log_warn("Link: multicast on %s, this PC's address in the game's network", addressText(&in, text, sizeof(text)));
        }
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

extern "C" int bridgeGetsockopt(SOCKET fd, int level, int optname, char *optval, int *optlen)
{
    SOCKET s = gameSocket(fd);
    log_info(">>> getsockopt called: socket=%lld, level=%d, optname=%d", (long long)fd, level, optname);

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