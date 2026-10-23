#include <string.h>
#if defined(_WIN32) || defined(__MINGW32__)

#include "symbolResolver.hpp"
#include "mathBridge.hpp"

#include <math.h>
#include <cmath>
#include <stdint.h>
#include <stdlib.h>

#define MAP(name, func) SymbolResolver::GetInstance().RegisterVTable(name, reinterpret_cast<void *>(func))

namespace MathBridge
{
    void initBridges()
    {
        MAP("atan", bridge_atan);
        MAP("atan2", bridge_atan2);
        MAP("atanf", bridge_atanf);
        MAP("atan2f", bridge_atan2f);
        MAP("tan", bridge_tan);
        MAP("log", bridge_log);
        MAP("logf", bridge_logf);
        MAP("log10", bridge_log10);
        MAP("log10f", bridge_log10f);
        MAP("exp", bridge_exp);
        MAP("expf", bridge_expf);
        MAP("asin", bridge_asin);
        MAP("asinf", bridge_asinf);
        MAP("sin", bridge_sin);
        MAP("sinh", bridge_sinh);
        MAP("sinf", bridge_sinf);
        MAP("sinhf", bridge_sinhf);
        MAP("acos", bridge_acos);
        MAP("acosf", bridge_acosf);
        MAP("cos", bridge_cos);
        MAP("cosh", bridge_cosh);
        MAP("cosf", bridge_cosf);
        MAP("coshf", bridge_coshf);
        MAP("tan", bridge_tan);
        MAP("tanh", bridge_tanh);
        MAP("tanf", bridge_tanf);
        MAP("tanhf", bridge_tanhf);
        MAP("pow", bridge_pow);
        MAP("powf", bridge_powf);
        MAP("hypot", bridge_hypot);
        MAP("hypotf", bridge_hypotf);
        MAP("fmod", bridge_fmod);
        MAP("fmodf", bridge_fmodf);
        MAP("modf", bridge_modf);
        MAP("modff", bridge_modff);
        MAP("sqrt", bridge_sqrt);
        MAP("sqrtf", bridge_sqrtf);
        MAP("div", bridge_div);
        MAP("ldiv", bridge_div); // long is int on i386
        MAP("ldexp", bridge_ldexp);
        MAP("roundf", bridge_roundf);
        MAP("difftime", bridge_difftime);
        MAP("finitef", bridge_finitef);
        MAP("finite", bridge_finite);
        MAP("frexp", bridge_frexp);
        MAP("frexpf", bridge_frexpf);
        MAP("ldexpf", bridge_ldexpf);
        MAP("fabsf", bridge_fabsf);
        MAP("ceil", bridge_ceil);
        MAP("floor", bridge_floor);
        MAP("log2f", bridge_log2f);
        MAP("log2", bridge_log2);
        MAP("ceilf", bridge_ceilf);
        MAP("lrint", bridge_lrint);
        MAP("sincos", bridge_sincos);
        MAP("sincosf", bridge_sincosf);
        MAP("strtof", bridge_strtof);
        MAP("strtold", bridge_strtold);
        MAP("strtoul", bridge_strtoul);
        MAP("frexpl", bridge_frexpl);
    }
}   

extern "C" double bridge_log(double x)
{
    return ::log(x);
}
extern "C" float bridge_logf(float x)
{
    return ::logf(x);
}
extern "C" double bridge_log10(double x)
{
    return ::log10(x);
}
extern "C" float bridge_log10f(float x)
{
    return ::log10f(x);
}
extern "C" double bridge_exp(double x)
{
    return ::exp(x);
}
extern "C" float bridge_expf(float x)
{
    return ::expf(x);
}
extern "C" double bridge_asin(double x)
{
    return ::asin(x);
}
extern "C" float bridge_asinf(float x)
{
    return ::asinf(x);
}
extern "C" double bridge_sin(double x)
{
    return ::sin(x);
}
extern "C" double bridge_sinh(double x)
{
    return ::sinh(x);
}
extern "C" float bridge_sinf(float x)
{
    return ::sinf(x);
}
extern "C" float bridge_sinhf(float x)
{
    return ::sinhf(x);
}
extern "C" double bridge_acos(double x)
{
    return ::acos(x);
}
extern "C" float bridge_acosf(float x)
{
    return ::acosf(x);
}
extern "C" double bridge_cos(double x)
{
    return ::cos(x);
}
extern "C" double bridge_cosh(double x)
{
    return ::cosh(x);
}
extern "C" float bridge_cosf(float x)
{
    return ::cosf(x);
}
extern "C" float bridge_coshf(float x)
{
    return ::coshf(x);
}
extern "C" double bridge_atan(double x)
{
    return ::atan(x);
}
extern "C" double bridge_atan2(double y, double x)
{
    return ::atan2(y, x);
}
extern "C" float bridge_atanf(float x)
{
    return ::atanf(x);
}
extern "C" float bridge_atan2f(float y, float x)
{
    return ::atan2f(y, x);
}
extern "C" double bridge_tan(double x)
{
    return ::tan(x);
}
extern "C" double bridge_tanh(double x)
{
    return ::tanh(x);
}
extern "C" float bridge_tanf(float x)
{
    return ::tanf(x);
}
extern "C" float bridge_tanhf(float x)
{
    return ::tanhf(x);
}
extern "C" double bridge_pow(double x, double y)
{
    return ::pow(x, y);
}
extern "C" float bridge_powf(float x, float y)
{
    return ::powf(x, y);
}
extern "C" double bridge_hypot(double x, double y)
{
    return ::hypot(x, y);
}
extern "C" float bridge_hypotf(float x, float y)
{
    return ::hypotf(x, y);
}
extern "C" double bridge_fmod(double x, double y)
{
    return ::fmod(x, y);
}
extern "C" float bridge_fmodf(float x, float y)
{
    return ::fmodf(x, y);
}
extern "C" double bridge_modf(double x, double *iptr)
{
    return ::modf(x, iptr);
}
extern "C" float bridge_modff(float x, float *iptr)
{
    return ::modff(x, iptr);
}
extern "C" double bridge_sqrt(double x)
{
    return ::sqrt(x);
}
extern "C" float bridge_sqrtf(float x)
{
    return ::sqrtf(x);
}
// div / ldiv return a struct. The i386 Linux ABI returns it through a hidden pointer, passed before the arguments,
// that the callee pops (ret $4), and leaves that pointer in eax; MSVC and MinGW would return it in edx:eax.
// Stack on entry: return address, result pointer, numerator, denominator.
#ifdef _MSC_VER
extern "C" __declspec(naked) void bridge_div(void)
{
    __asm {
        mov ecx, [esp + 4]
        mov eax, [esp + 8]
        cdq
        idiv dword ptr [esp + 12]
        mov [ecx], eax
        mov [ecx + 4], edx
        mov eax, ecx
        ret 4
    }
}
#else
extern "C" void __attribute__((naked)) bridge_div(void)
{
    asm volatile("mov 4(%esp), %ecx\n\t"
                 "mov 8(%esp), %eax\n\t"
                 "cltd\n\t"
                 "idivl 12(%esp)\n\t"
                 "mov %eax, (%ecx)\n\t"
                 "mov %edx, 4(%ecx)\n\t"
                 "mov %ecx, %eax\n\t"
                 "ret $4\n\t");
}
#endif
extern "C" int bridge_finite(double x)
{
    return std::isfinite(x) ? 1 : 0;
}
extern "C" int bridge_finitef(float x)
{
    return std::isfinite(x) ? 1 : 0;
}
extern "C" double bridge_frexp(double x, int *exp)
{
    return ::frexp(x, exp);
}
extern "C" float bridge_frexpf(float x, int *exp)
{
    return ::frexpf(x, exp);
}
extern "C" float bridge_ldexpf(float x, int exp)
{
    return ::ldexpf(x, exp);
}
extern "C" double bridge_ldexp(double x, int exp)
{
    return ::ldexp(x, exp);
}
extern "C" float bridge_roundf(float x)
{
    return ::roundf(x);
}
// time_t is 32 bits in the games
extern "C" double bridge_difftime(int end, int start)
{
    return (double)end - (double)start;
}

extern "C" float bridge_fabsf(float x)
{
    return ::fabsf(x);
}

extern "C" double bridge_ceil(double x)
{
    return ::ceil(x);
}

extern "C" double bridge_floor(double x)
{
    return ::floor(x);
}

extern "C" float bridge_log2f(float x)
{
    return ::log2f(x);
}

extern "C" double bridge_log2(double x)
{
    return ::log2(x);
}

extern "C" float bridge_ceilf(float x)
{
    return ::ceilf(x);
}

extern "C" long bridge_lrint(double x)
{
    return ::lrint(x);
}

// glibc's, not in Windows' C library
extern "C" void bridge_sincos(double x, double *s, double *c)
{
    *s = ::sin(x);
    *c = ::cos(x);
}

extern "C" void bridge_sincosf(float x, float *s, float *c)
{
    *s = ::sinf(x);
    *c = ::cosf(x);
}

extern "C" float bridge_strtof(const char *text, char **end)
{
    return ::strtof(text, end);
}

// Linux returns a long double in st(0), where a double comes back too: the caller reads the same register
extern "C" double bridge_strtold(const char *text, char **end)
{
    return ::strtod(text, end);
}

extern "C" unsigned long bridge_strtoul(const char *text, char **end, int base)
{
    return ::strtoul(text, end, base);
}

// The x87 extended value read by hand (MSVC's long double is a double), then split as frexp does
extern "C" double bridge_frexpl(MathBridge::LinuxLongDouble x, int *exponent)
{
    uint64_t mantissa;
    uint16_t signExponent;
    memcpy(&mantissa, x.bytes, 8);
    memcpy(&signExponent, x.bytes + 8, 2);
    int biased = signExponent & 0x7FFF;
    double value = 0.0;
    if (biased != 0 || mantissa != 0)
        value = ::ldexp((double)mantissa, biased - 16383 - 63);
    if (signExponent & 0x8000)
        value = -value;
    return ::frexp(value, exponent);
}

#endif
