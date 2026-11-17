// glibc functions the Lindbergh games did not call and Raw Thrills' Cruis'n Blast (and the FMOD libraries it loads)
// does: the fortified _chk variants of its compiler, wide strings with Linux's 32-bit wchar_t, iconv, and a few of the
// system's (uname, sysinfo, mount and login tables). Those the loader already bridges under another name are taken
// from there, so that their file and path handling stays the same.
#if defined(_WIN32) || defined(__MINGW32__)

#include <windows.h>

#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <string>
#include <vector>

#include "libcMoreBridge.hpp"
#include "memoryManager.hpp"
#include "symbolResolver.hpp"
#include "../log/log.h"

#define MAP(name, func) SymbolResolver::GetInstance().RegisterVTable(name, reinterpret_cast<void *>(func))

// Linux's errno values, where Windows' differ
#define LINUX_ENOSYS 38
#define LINUX_ENOTTY 25

typedef uint32_t lwchar; // Linux's wchar_t

namespace
{
// A function another bridge provides, by its name
void *bridged(const char *name)
{
    return SymbolResolver::GetInstance().GetVTable(name);
}

void logOnce(const char *what)
{
    static std::vector<std::string> seen;
    for (const std::string &s : seen)
        if (s == what)
            return;
    seen.push_back(what);
    log_warn("libc: %s", what);
}

// --- UTF-8 and UTF-32

void appendUtf8(std::string &out, uint32_t c)
{
    if (c < 0x80)
        out += (char)c;
    else if (c < 0x800)
    {
        out += (char)(0xc0 | (c >> 6));
        out += (char)(0x80 | (c & 0x3f));
    }
    else if (c < 0x10000)
    {
        out += (char)(0xe0 | (c >> 12));
        out += (char)(0x80 | ((c >> 6) & 0x3f));
        out += (char)(0x80 | (c & 0x3f));
    }
    else
    {
        out += (char)(0xf0 | (c >> 18));
        out += (char)(0x80 | ((c >> 12) & 0x3f));
        out += (char)(0x80 | ((c >> 6) & 0x3f));
        out += (char)(0x80 | (c & 0x3f));
    }
}

// One character of UTF-8 at *s, s moved past it; a byte that starts no sequence is taken as Latin-1
uint32_t nextUtf8(const unsigned char *&s, const unsigned char *end)
{
    unsigned char c = *s++;
    int more = c >= 0xf0 && c < 0xf8 ? 3 : c >= 0xe0 ? 2 : c >= 0xc0 ? 1 : 0;
    if (c < 0x80 || !more || end - s < more)
        return c;
    uint32_t value = c & (0x3f >> more);
    for (int i = 0; i < more; i++)
    {
        if ((s[i] & 0xc0) != 0x80)
            return c;
        value = (value << 6) | (s[i] & 0x3f);
    }
    s += more;
    return value;
}

std::string toUtf8(const lwchar *w)
{
    std::string out;
    for (; w && *w; w++)
        appendUtf8(out, *w);
    return out;
}

void appendWide(std::vector<lwchar> &out, const char *s, size_t n)
{
    const unsigned char *p = (const unsigned char *)s, *end = p + n;
    while (p < end)
        out.push_back(nextUtf8(p, end));
}

// --- the wide printf: the conversions of the format through the narrow printf, %ls and %lc taken as wide (as glibc
// does, %s and %c are then narrow)

int formatWide(std::vector<lwchar> &out, const lwchar *fmt, va_list ap)
{
    while (fmt && *fmt)
    {
        if (*fmt != '%')
        {
            out.push_back(*fmt++);
            continue;
        }
        if (fmt[1] == '%')
        {
            out.push_back('%');
            fmt += 2;
            continue;
        }
        // The spec, in narrow letters: flags, width, precision, length, conversion
        std::string spec = "%";
        const lwchar *p = fmt + 1;
        while (*p && strchr("-+ #0", (int)*p))
            spec += (char)*p++;
        auto number = [&](void) {
            if (*p == '*')
            {
                spec += std::to_string(va_arg(ap, int));
                p++;
            }
            else
                while (*p >= '0' && *p <= '9')
                    spec += (char)*p++;
        };
        number();
        if (*p == '.')
        {
            spec += '.';
            p++;
            number();
        }
        std::string length;
        while (*p && strchr("hlLqjzt", (int)*p))
            length += (char)*p++;
        lwchar conv = *p ? *p++ : 0;
        fmt = p;
        char buffer[512];
        int n = 0;
        bool wide = length == "l";
        switch (conv)
        {
            case 'd':
            case 'i':
            case 'u':
            case 'x':
            case 'X':
            case 'o':
                if (length == "ll" || length == "q" || length == "j")
                    n = snprintf(buffer, sizeof(buffer), (spec + "ll" + (char)conv).c_str(), va_arg(ap, long long));
                else
                    n = snprintf(buffer, sizeof(buffer), (spec + (char)conv).c_str(), va_arg(ap, int));
                break;
            case 'f':
            case 'F':
            case 'e':
            case 'E':
            case 'g':
            case 'G':
            case 'a':
            case 'A':
                if (length == "L")
                    n = snprintf(buffer, sizeof(buffer), (spec + (char)conv).c_str(), (double)va_arg(ap, long double));
                else
                    n = snprintf(buffer, sizeof(buffer), (spec + (char)conv).c_str(), va_arg(ap, double));
                break;
            case 'p':
                n = snprintf(buffer, sizeof(buffer), (spec + "p").c_str(), va_arg(ap, void *));
                break;
            case 'c':
                if (wide)
                {
                    std::string one;
                    appendUtf8(one, (uint32_t)va_arg(ap, int));
                    n = snprintf(buffer, sizeof(buffer), (spec + "s").c_str(), one.c_str());
                }
                else
                    n = snprintf(buffer, sizeof(buffer), (spec + "c").c_str(), va_arg(ap, int));
                break;
            case 's':
            {
                const char *arg;
                std::string text;
                if (wide || length == "S")
                {
                    text = toUtf8(va_arg(ap, const lwchar *));
                    arg = text.c_str();
                }
                else
                    arg = va_arg(ap, const char *);
                std::string formatted(4096, '\0');
                n = snprintf(&formatted[0], formatted.size(), (spec + "s").c_str(), arg ? arg : "(null)");
                if (n > 0)
                    appendWide(out, formatted.c_str(), strlen(formatted.c_str()));
                continue;
            }
            case 'n':
                *va_arg(ap, int *) = (int)out.size();
                continue;
            default:
                logOnce("wide printf: a conversion it does not know");
                continue;
        }
        if (n > 0)
            appendWide(out, buffer, strlen(buffer));
    }
    return (int)out.size();
}

int writeWide(lwchar *s, size_t n, const lwchar *fmt, va_list ap)
{
    std::vector<lwchar> out;
    int count = formatWide(out, fmt, ap);
    if (!s || n == 0)
        return -1;
    size_t copy = out.size() < n ? out.size() : n - 1;
    memcpy(s, out.data(), copy * sizeof(lwchar));
    s[copy] = 0;
    return out.size() < n ? count : -1; // glibc: -1 when the output did not fit
}

int printWide(FILE *stream, const lwchar *fmt, va_list ap)
{
    std::vector<lwchar> out;
    int count = formatWide(out, fmt, ap);
    out.push_back(0);
    std::string text = toUtf8(out.data());
    fputs(text.c_str(), stream);
    return count;
}

// --- iconv: UTF-8, UTF-32 (WCHAR_T, UCS-4), UTF-16 and Latin-1, enough for the games' texts

enum Charset
{
    CS_UNKNOWN,
    CS_UTF8,
    CS_UTF32LE,
    CS_UTF32BE,
    CS_UTF16LE,
    CS_UTF16BE,
    CS_LATIN1,
    CS_ASCII
};

Charset charsetOf(const char *name)
{
    std::string n;
    for (const char *c = name ? name : ""; *c && *c != '/'; c++)
        if (*c != '-' && *c != '_')
            n += (char)toupper((unsigned char)*c);
    if (n == "UTF8")
        return CS_UTF8;
    if (n == "WCHART" || n == "UTF32" || n == "UTF32LE" || n == "UCS4" || n == "UCS4LE" || n == "UCS4INTERNAL")
        return CS_UTF32LE;
    if (n == "UTF32BE" || n == "UCS4BE")
        return CS_UTF32BE;
    if (n == "UTF16" || n == "UTF16LE" || n == "UCS2" || n == "UCS2LE")
        return CS_UTF16LE;
    if (n == "UTF16BE" || n == "UCS2BE")
        return CS_UTF16BE;
    if (n == "ISO88591" || n == "LATIN1" || n == "ISO885915" || n == "CP1252" || n == "WINDOWS1252")
        return CS_LATIN1;
    if (n == "ASCII" || n == "USASCII" || n == "ANSIX3.41968" || n == "" || n == "CHAR")
        return CS_ASCII;
    return CS_UNKNOWN;
}

struct Converter
{
    Charset from, to;
};

int unitSize(Charset cs)
{
    return cs == CS_UTF32LE || cs == CS_UTF32BE ? 4 : cs == CS_UTF16LE || cs == CS_UTF16BE ? 2 : 1;
}

// One character of the input: its bytes used, 0 for an incomplete one, -1 for an invalid one
int decode(Charset cs, const unsigned char *in, size_t left, uint32_t *c)
{
    switch (cs)
    {
        case CS_UTF8:
        {
            const unsigned char *p = in;
            int need = in[0] < 0x80 ? 1 : in[0] >= 0xf0 ? 4 : in[0] >= 0xe0 ? 3 : in[0] >= 0xc0 ? 2 : -1;
            if (need < 0)
                return -1;
            if ((size_t)need > left)
                return 0;
            *c = nextUtf8(p, in + left);
            return (int)(p - in);
        }
        case CS_UTF32LE:
        case CS_UTF32BE:
            if (left < 4)
                return 0;
            *c = cs == CS_UTF32LE ? in[0] | in[1] << 8 | in[2] << 16 | (uint32_t)in[3] << 24
                                  : (uint32_t)in[0] << 24 | in[1] << 16 | in[2] << 8 | in[3];
            return 4;
        case CS_UTF16LE:
        case CS_UTF16BE:
        {
            if (left < 2)
                return 0;
            uint32_t u = cs == CS_UTF16LE ? in[0] | in[1] << 8 : in[0] << 8 | in[1];
            if (u >= 0xd800 && u < 0xdc00)
            {
                if (left < 4)
                    return 0;
                uint32_t v = cs == CS_UTF16LE ? in[2] | in[3] << 8 : in[2] << 8 | in[3];
                *c = 0x10000 + ((u - 0xd800) << 10) + (v - 0xdc00);
                return 4;
            }
            *c = u;
            return 2;
        }
        default:
            *c = in[0];
            return 1;
    }
}

// The character's bytes in out, their count; 0 when it does not fit, -1 when the charset cannot hold it
int encode(Charset cs, uint32_t c, unsigned char *out, size_t left)
{
    unsigned char b[4];
    int n;
    switch (cs)
    {
        case CS_UTF8:
        {
            std::string s;
            appendUtf8(s, c);
            n = (int)s.size();
            memcpy(b, s.data(), n);
            break;
        }
        case CS_UTF32LE:
            b[0] = c & 0xff, b[1] = (c >> 8) & 0xff, b[2] = (c >> 16) & 0xff, b[3] = c >> 24, n = 4;
            break;
        case CS_UTF32BE:
            b[3] = c & 0xff, b[2] = (c >> 8) & 0xff, b[1] = (c >> 16) & 0xff, b[0] = c >> 24, n = 4;
            break;
        case CS_UTF16LE:
        case CS_UTF16BE:
        {
            uint16_t u[2];
            int units = 1;
            if (c >= 0x10000)
            {
                u[0] = (uint16_t)(0xd800 + ((c - 0x10000) >> 10));
                u[1] = (uint16_t)(0xdc00 + ((c - 0x10000) & 0x3ff));
                units = 2;
            }
            else
                u[0] = (uint16_t)c;
            for (int i = 0; i < units; i++)
            {
                b[i * 2] = cs == CS_UTF16LE ? u[i] & 0xff : u[i] >> 8;
                b[i * 2 + 1] = cs == CS_UTF16LE ? u[i] >> 8 : u[i] & 0xff;
            }
            n = units * 2;
            break;
        }
        case CS_ASCII:
            if (c > 0x7f)
                return -1;
            b[0] = (unsigned char)c, n = 1;
            break;
        default: // Latin-1
            if (c > 0xff)
                return -1;
            b[0] = (unsigned char)c, n = 1;
            break;
    }
    if ((size_t)n > left)
        return 0;
    memcpy(out, b, n);
    return n;
}
} // namespace

namespace LibcMoreBridge
{
extern "C"
{
    // --- the fortified variants: the checks left out, the plain function done

    static int bridge__fprintf_chk(FILE *stream, int, const char *format, ...)
    {
        va_list ap;
        va_start(ap, format);
        int ret = vfprintf(stream, format, ap);
        va_end(ap);
        return ret;
    }

    static int bridge__printf_chk(int, const char *format, ...)
    {
        va_list ap;
        va_start(ap, format);
        int ret = vprintf(format, ap);
        va_end(ap);
        return ret;
    }

    static int bridge__sprintf_chk(char *s, int, size_t, const char *format, ...)
    {
        va_list ap;
        va_start(ap, format);
        int ret = vsprintf(s, format, ap);
        va_end(ap);
        return ret;
    }

    static int bridge__snprintf_chk(char *s, size_t maxlen, int, size_t, const char *format, ...)
    {
        va_list ap;
        va_start(ap, format);
        int ret = vsnprintf(s, maxlen, format, ap);
        va_end(ap);
        return ret;
    }

    static int bridge__vsprintf_chk(char *s, int, size_t, const char *format, va_list ap)
    {
        return vsprintf(s, format, ap);
    }

    static int bridge__vsnprintf_chk(char *s, size_t maxlen, int, size_t, const char *format, va_list ap)
    {
        return vsnprintf(s, maxlen, format, ap);
    }

    static int bridge__asprintf_chk(char **out, int, const char *format, ...)
    {
        va_list ap, again;
        va_start(ap, format);
        va_copy(again, ap);
        int n = vsnprintf(NULL, 0, format, ap);
        va_end(ap);
        *out = n >= 0 ? (char *)MemoryManager::customMalloc((size_t)n + 1) : NULL;
        if (*out)
            vsnprintf(*out, (size_t)n + 1, format, again);
        va_end(again);
        return *out ? n : -1;
    }

    static void *bridge__memcpy_chk(void *d, const void *s, size_t n, size_t)
    {
        return memcpy(d, s, n);
    }

    static void *bridge__memset_chk(void *d, int c, size_t n, size_t)
    {
        return memset(d, c, n);
    }

    static char *bridge__strcpy_chk(char *d, const char *s, size_t)
    {
        return strcpy(d, s);
    }

    static char *bridge__strcat_chk(char *d, const char *s, size_t)
    {
        return strcat(d, s);
    }

    static char *bridge__strncpy_chk(char *d, const char *s, size_t n, size_t)
    {
        return strncpy(d, s, n);
    }

    static char *bridge__strncat_chk(char *d, const char *s, size_t n, size_t)
    {
        return strncat(d, s, n);
    }

    static char *bridgeStpcpy(char *d, const char *s)
    {
        size_t n = strlen(s);
        memcpy(d, s, n + 1);
        return d + n;
    }

    static char *bridge__stpcpy_chk(char *d, const char *s, size_t)
    {
        return bridgeStpcpy(d, s);
    }

    static size_t bridge__fread_chk(void *ptr, size_t, size_t size, size_t n, FILE *stream)
    {
        typedef size_t (*Fread)(void *, size_t, size_t, FILE *);
        Fread f = (Fread)bridged("fread");
        return f ? f(ptr, size, n, stream) : fread(ptr, size, n, stream);
    }

    // FD_SET's index check: the word of the descriptor's bit, 32 descriptors a word
    static long bridge__fdelt_chk(long d)
    {
        if (d < 0 || d >= 1024)
            log_warn("libc: descriptor %ld out of an fd_set", d);
        return d / 32;
    }

    static int bridge__open_2(const char *path, int flags)
    {
        typedef int (*Open)(const char *, int, ...);
        Open f = (Open)bridged("open");
        return f ? f(path, flags) : -1;
    }

    static void bridge__stack_chk_fail(void)
    {
        log_fatal("libc: the program's stack was overwritten (__stack_chk_fail)");
        ExitProcess(1);
    }

    // --- glibc's internal names of others

    static int *bridge__h_errno_location(void)
    {
        static int value = 0;
        return &value;
    }

    static int bridge__libc_current_sigrtmin(void)
    {
        return 34;
    }

    static int bridge__finitef(float x)
    {
        return isfinite(x) ? 1 : 0;
    }

    static float bridgeFloorf(float x)
    {
        return floorf(x);
    }

    static long bridgeAtol(const char *s)
    {
        return atol(s);
    }

    static unsigned long long bridgeStrtoull(const char *s, char **end, int base)
    {
        return strtoull(s, end, base);
    }

    // --- time

    struct LinuxTm // i386 glibc's, tm_gmtoff and tm_zone after Windows' fields
    {
        int tm_sec, tm_min, tm_hour, tm_mday, tm_mon, tm_year, tm_wday, tm_yday, tm_isdst;
        long tm_gmtoff;
        const char *tm_zone;
    };

    static char *bridgeAsctime_r(const LinuxTm *t, char *buf)
    {
        static const char days[7][4] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
        static const char months[12][4] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
        snprintf(buf, 26, "%.3s %.3s%3d %.2d:%.2d:%.2d %d\n", days[(unsigned)t->tm_wday % 7], months[(unsigned)t->tm_mon % 12],
                 t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec, 1900 + t->tm_year);
        return buf;
    }

    static char *bridgeAsctime(const LinuxTm *t)
    {
        static char buf[32];
        return bridgeAsctime_r(t, buf);
    }

    struct LinuxTimespec
    {
        int32_t tv_sec, tv_nsec;
    };

    static int bridgeClockGetres(int, LinuxTimespec *res)
    {
        if (res)
            res->tv_sec = 0, res->tv_nsec = 100; // QueryPerformanceCounter's tick, about
        return 0;
    }

    static int bridgeTimerfdCreate(int, int)
    {
        logOnce("timerfd_create: no timer descriptors");
        errno = LINUX_ENOSYS;
        return -1;
    }

    static int bridgeTimerfdSettime(int, int, const void *, void *)
    {
        errno = LINUX_ENOSYS;
        return -1;
    }

    // --- the system

    struct LinuxUtsname
    {
        char sysname[65], nodename[65], release[65], version[65], machine[65], domainname[65];
    };

    static int bridgeUname(LinuxUtsname *u)
    {
        memset(u, 0, sizeof(*u));
        strcpy(u->sysname, "Linux");
        strcpy(u->nodename, "pm");
        strcpy(u->release, "3.2.0");
        strcpy(u->version, "#1 SMP");
        strcpy(u->machine, "i686");
        strcpy(u->domainname, "(none)");
        return 0;
    }

    struct LinuxSysinfo
    {
        int32_t uptime;
        uint32_t loads[3];
        uint32_t totalram, freeram, sharedram, bufferram, totalswap, freeswap;
        uint16_t procs, pad;
        uint32_t totalhigh, freehigh, mem_unit;
        char reserved[8];
    };

    static int bridgeSysinfo(LinuxSysinfo *info)
    {
        memset(info, 0, sizeof(*info));
        MEMORYSTATUSEX mem = {};
        mem.dwLength = sizeof(mem);
        GlobalMemoryStatusEx(&mem);
        info->uptime = (int32_t)(GetTickCount64() / 1000);
        info->mem_unit = 4096;
        info->totalram = (uint32_t)(mem.ullTotalPhys / 4096);
        info->freeram = (uint32_t)(mem.ullAvailPhys / 4096);
        info->totalswap = (uint32_t)(mem.ullTotalPageFile / 4096);
        info->freeswap = (uint32_t)(mem.ullAvailPageFile / 4096);
        info->procs = 100;
        return 0;
    }

    struct LinuxRlimit
    {
        uint32_t cur, max;
    };

    static int bridgeGetrlimit(int resource, LinuxRlimit *limit)
    {
        // RLIMIT_STACK (3): the 8 MB of a Linux shell, the others unlimited
        limit->cur = resource == 3 ? 8u << 20 : 0xffffffffu;
        limit->max = 0xffffffffu;
        return 0;
    }

    static int bridgeMprotect(void *address, size_t length, int prot)
    {
        DWORD protect = (prot & 4) ? ((prot & 2) ? PAGE_EXECUTE_READWRITE : (prot & 1) ? PAGE_EXECUTE_READ : PAGE_EXECUTE)
                                   : (prot & 2) ? PAGE_READWRITE : (prot & 1) ? PAGE_READONLY : PAGE_NOACCESS;
        DWORD old;
        return VirtualProtect(address, length, protect, &old) ? 0 : -1;
    }

    static int bridgeIoperm(unsigned long, unsigned long, int)
    {
        return 0; // the parallel port's wheel board is not there: its writes go nowhere
    }

    static int bridgeExecvp(const char *file, char *const *)
    {
        log_warn("libc: %s not run", file ? file : "(null)");
        errno = ENOENT;
        return -1;
    }

    static int bridgeGetloginR(char *buf, size_t size)
    {
        if (size < 3)
            return ERANGE;
        strcpy(buf, "pm");
        return 0;
    }

    static int bridgeTtynameR(int, char *, size_t)
    {
        return LINUX_ENOTTY;
    }

    static int bridgeZero(void)
    {
        return 0;
    }

    static int bridgeOne(void)
    {
        return 1;
    }

    static void *bridgeNull(void)
    {
        return NULL;
    }

    static void bridgeNothing(void)
    {
    }

    // No login records
    static int bridgeGetutentR(void *, void **result)
    {
        *result = NULL;
        errno = ENOENT;
        return -1;
    }

    static int bridgeBacktrace(void **, int)
    {
        return 0;
    }

    static char **bridgeBacktraceSymbols(void *const *, int size)
    {
        size_t n = size > 0 ? (size_t)size : 0;
        char **out = (char **)MemoryManager::customMalloc(n * sizeof(char *) + 1);
        if (!out)
            return NULL;
        char *empty = (char *)(out + n);
        *empty = '\0';
        for (size_t i = 0; i < n; i++)
            out[i] = empty;
        return out;
    }

    static int bridgeSigdelset(void *set, int signum)
    {
        if (signum < 1 || signum > 64)
            return -1;
        ((uint32_t *)set)[(signum - 1) / 32] &= ~(1u << ((signum - 1) % 32));
        return 0;
    }

    static int bridgePthreadAttrSetinheritsched(void *, int)
    {
        return 0;
    }

    static void *bridgeDlvsym(void *handle, const char *name, const char *)
    {
        typedef void *(*Dlsym)(void *, const char *);
        Dlsym f = (Dlsym)bridged("dlsym");
        return f ? f(handle, name) : NULL;
    }

    // --- wide strings, 32-bit characters

    static lwchar *bridgeWcscat(lwchar *d, const lwchar *s)
    {
        lwchar *p = d;
        while (*p)
            p++;
        while ((*p++ = *s++) != 0)
        {
        }
        return d;
    }

    static lwchar *bridge__wcscpy_chk(lwchar *d, const lwchar *s, size_t)
    {
        lwchar *p = d;
        while ((*p++ = *s++) != 0)
        {
        }
        return d;
    }

    static lwchar *bridgeWcsdup(const lwchar *s)
    {
        size_t n = 0;
        while (s[n])
            n++;
        lwchar *d = (lwchar *)MemoryManager::customMalloc((n + 1) * sizeof(lwchar));
        if (d)
            memcpy(d, s, (n + 1) * sizeof(lwchar));
        return d;
    }

    static lwchar lowerWide(lwchar c)
    {
        return c < 0x10000 ? (lwchar)(uintptr_t)CharLowerW((LPWSTR)(uintptr_t)c) : c;
    }

    static int bridgeWcscasecmp(const lwchar *a, const lwchar *b)
    {
        for (;; a++, b++)
        {
            lwchar x = lowerWide(*a), y = lowerWide(*b);
            if (x != y || !x)
                return (int)x - (int)y;
        }
    }

    static bool inSet(lwchar c, const lwchar *set)
    {
        for (; *set; set++)
            if (*set == c)
                return true;
        return false;
    }

    static lwchar *bridgeWcstok(lwchar *s, const lwchar *delim, lwchar **save)
    {
        if (!s)
            s = *save;
        while (*s && inSet(*s, delim))
            s++;
        if (!*s)
        {
            *save = s;
            return NULL;
        }
        lwchar *token = s;
        while (*s && !inSet(*s, delim))
            s++;
        if (*s)
            *s++ = 0;
        *save = s;
        return token;
    }

    // The digits of a wide number as narrow ones, one byte a character, so that the end found maps back
    static std::string narrowDigits(const lwchar *s)
    {
        std::string n;
        for (; *s; s++)
            n += *s < 0x80 ? (char)*s : '?';
        return n;
    }

    static long long bridgeWcstoll(const lwchar *s, lwchar **end, int base)
    {
        std::string n = narrowDigits(s);
        char *e;
        long long v = strtoll(n.c_str(), &e, base);
        if (end)
            *end = (lwchar *)s + (e - n.c_str());
        return v;
    }

    static unsigned long long bridgeWcstoull(const lwchar *s, lwchar **end, int base)
    {
        std::string n = narrowDigits(s);
        char *e;
        unsigned long long v = strtoull(n.c_str(), &e, base);
        if (end)
            *end = (lwchar *)s + (e - n.c_str());
        return v;
    }

    static int bridgeSwprintfChk(lwchar *s, size_t n, int, size_t, const lwchar *format, ...)
    {
        va_list ap;
        va_start(ap, format);
        int ret = writeWide(s, n, format, ap);
        va_end(ap);
        return ret;
    }

    static int bridgeFwprintf(FILE *stream, const lwchar *format, ...)
    {
        va_list ap;
        va_start(ap, format);
        int ret = printWide(stream, format, ap);
        va_end(ap);
        return ret;
    }

    static int bridge__fwprintf_chk(FILE *stream, int, const lwchar *format, ...)
    {
        va_list ap;
        va_start(ap, format);
        int ret = printWide(stream, format, ap);
        va_end(ap);
        return ret;
    }

    // The narrow scanf on the text and the format made narrow: %s and %c store narrow characters, as glibc's wide
    // scanf does; %ls and %lc are not done
    static int bridgeSwscanf(const lwchar *s, const lwchar *format, ...)
    {
        std::string text = toUtf8(s), f = toUtf8(format);
        if (f.find("%l[") != std::string::npos || f.find("%ls") != std::string::npos || f.find("%lc") != std::string::npos)
            logOnce("swscanf: %ls, %lc and %l[ are not read");
        va_list ap;
        va_start(ap, format);
        int ret = vsscanf(text.c_str(), f.c_str(), ap);
        va_end(ap);
        return ret;
    }

    // --- iconv

    static void *bridgeIconvOpen(const char *to, const char *from)
    {
        Converter c = {charsetOf(from), charsetOf(to)};
        if (c.from == CS_UNKNOWN || c.to == CS_UNKNOWN)
        {
            log_warn("iconv: %s to %s not known", from ? from : "(null)", to ? to : "(null)");
            errno = EINVAL;
            return (void *)(intptr_t)-1;
        }
        Converter *out = new Converter(c);
        return out;
    }

    static int bridgeIconvClose(void *cd)
    {
        delete (Converter *)cd;
        return 0;
    }

    static size_t bridgeIconv(void *cd, char **in, size_t *inLeft, char **out, size_t *outLeft)
    {
        Converter *c = (Converter *)cd;
        if (!in || !*in) // reset: no state kept
            return 0;
        size_t done = 0;
        while (*inLeft > 0)
        {
            uint32_t ch;
            int used = decode(c->from, (const unsigned char *)*in, *inLeft, &ch);
            if (used == 0)
            {
                errno = EINVAL; // an incomplete character at the end
                return (size_t)-1;
            }
            if (used < 0)
            {
                errno = EILSEQ;
                return (size_t)-1;
            }
            int wrote = encode(c->to, ch, (unsigned char *)*out, *outLeft);
            if (wrote == 0)
            {
                errno = E2BIG;
                return (size_t)-1;
            }
            if (wrote < 0)
                wrote = encode(c->to, '?', (unsigned char *)*out, *outLeft), done++;
            if (wrote <= 0)
            {
                errno = E2BIG;
                return (size_t)-1;
            }
            *in += used;
            *inLeft -= used;
            *out += wrote;
            *outLeft -= wrote;
        }
        return done;
    }
}

void initBridges()
{
    MAP("__fprintf_chk", bridge__fprintf_chk);
    MAP("__printf_chk", bridge__printf_chk);
    MAP("__sprintf_chk", bridge__sprintf_chk);
    MAP("__snprintf_chk", bridge__snprintf_chk);
    MAP("__vsprintf_chk", bridge__vsprintf_chk);
    MAP("__vsnprintf_chk", bridge__vsnprintf_chk);
    MAP("__asprintf_chk", bridge__asprintf_chk);
    MAP("__memcpy_chk", bridge__memcpy_chk);
    MAP("__memset_chk", bridge__memset_chk);
    MAP("__strcpy_chk", bridge__strcpy_chk);
    MAP("__strcat_chk", bridge__strcat_chk);
    MAP("__strncpy_chk", bridge__strncpy_chk);
    MAP("__strncat_chk", bridge__strncat_chk);
    MAP("__stpcpy_chk", bridge__stpcpy_chk);
    MAP("stpcpy", bridgeStpcpy);
    MAP("__fread_chk", bridge__fread_chk);
    MAP("__fdelt_chk", bridge__fdelt_chk);
    MAP("__open_2", bridge__open_2);
    MAP("__stack_chk_fail", bridge__stack_chk_fail);

    MAP("__h_errno_location", bridge__h_errno_location);
    MAP("__libc_current_sigrtmin", bridge__libc_current_sigrtmin);
    MAP("__finitef", bridge__finitef);
    MAP("floorf", bridgeFloorf);
    MAP("atol", bridgeAtol);
    MAP("strtoull", bridgeStrtoull);

    MAP("asctime", bridgeAsctime);
    MAP("asctime_r", bridgeAsctime_r);
    MAP("clock_getres", bridgeClockGetres);
    MAP("timerfd_create", bridgeTimerfdCreate);
    MAP("timerfd_settime", bridgeTimerfdSettime);

    MAP("uname", bridgeUname);
    MAP("sysinfo", bridgeSysinfo);
    MAP("getrlimit", bridgeGetrlimit);
    MAP("mprotect", bridgeMprotect);
    MAP("ioperm", bridgeIoperm);
    MAP("execvp", bridgeExecvp);
    MAP("getlogin_r", bridgeGetloginR);
    MAP("ttyname_r", bridgeTtynameR);
    MAP("setmntent", bridgeNull); // no mount table
    MAP("getmntent", bridgeNull);
    MAP("endmntent", bridgeOne);
    MAP("setutent", bridgeNothing); // no login records
    MAP("endutent", bridgeNothing);
    MAP("getutent_r", bridgeGetutentR);
    MAP("backtrace", bridgeBacktrace);
    MAP("backtrace_symbols", bridgeBacktraceSymbols);
    MAP("sigdelset", bridgeSigdelset);
    MAP("pthread_attr_setinheritsched", bridgePthreadAttrSetinheritsched);
    MAP("dlvsym", bridgeDlvsym);
    (void)bridgeZero;

    MAP("wcscat", bridgeWcscat);
    MAP("__wcscpy_chk", bridge__wcscpy_chk);
    MAP("wcsdup", bridgeWcsdup);
    MAP("wcscasecmp", bridgeWcscasecmp);
    MAP("wcstok", bridgeWcstok);
    MAP("wcstoll", bridgeWcstoll);
    MAP("wcstoull", bridgeWcstoull);
    MAP("__swprintf_chk", bridgeSwprintfChk);
    MAP("fwprintf", bridgeFwprintf);
    MAP("__fwprintf_chk", bridge__fwprintf_chk);
    MAP("swscanf", bridgeSwscanf);

    MAP("iconv_open", bridgeIconvOpen);
    MAP("iconv_close", bridgeIconvClose);
    MAP("iconv", bridgeIconv);

    // Others' bridges under glibc's internal names
    static const char *const aliases[][2] = {{"__isoc99_sscanf", "sscanf"},       {"__isoc99_fscanf", "fscanf"},
                                             {"__sysv_signal", "signal"},          {"__strtok_r", "strtok_r"},
                                             {"__pthread_key_create", "pthread_key_create"}};
    for (const auto &alias : aliases)
    {
        void *target = bridged(alias[1]);
        if (target)
            MAP(alias[0], target);
        else
            log_warn("libc: no %s for %s", alias[1], alias[0]);
    }
}
} // namespace LibcMoreBridge
#endif
