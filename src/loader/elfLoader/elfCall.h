#pragma once

// Calls from host code into the game's Linux i386 code. The Linux ABI (and GCC/MinGW code) has esp % 16 == 0 at every
// call instruction, and the games' compilers rely on it for aligned SSE accesses to the stack (movaps, movdqa). MSVC
// only keeps 4-byte alignment, so MSVC-built host code calls the game through these, which realign the stack first.
// With GCC/MinGW the call sites call the game directly, as before (#ifdef _MSC_VER at each of them).
//
// elfCallN(function, arguments...): up to 4 arguments of 32 bits, the result in eax (int or pointer).

#ifdef _MSC_VER
#include <stdint.h>

static __inline uintptr_t elfCall0(const void *function)
{
    uintptr_t result;
    __asm {
        mov eax, function
        mov esi, esp
        and esp, 0xFFFFFFF0
        call eax
        mov esp, esi
        mov result, eax
    }
    return result;
}

static __inline uintptr_t elfCall1(const void *function, uintptr_t a1)
{
    uintptr_t result;
    __asm {
        mov eax, function
        mov esi, esp
        and esp, 0xFFFFFFF0
        sub esp, 12
        push a1
        call eax
        mov esp, esi
        mov result, eax
    }
    return result;
}

static __inline uintptr_t elfCall2(const void *function, uintptr_t a1, uintptr_t a2)
{
    uintptr_t result;
    __asm {
        mov eax, function
        mov esi, esp
        and esp, 0xFFFFFFF0
        sub esp, 8
        push a2
        push a1
        call eax
        mov esp, esi
        mov result, eax
    }
    return result;
}

static __inline uintptr_t elfCall3(const void *function, uintptr_t a1, uintptr_t a2, uintptr_t a3)
{
    uintptr_t result;
    __asm {
        mov eax, function
        mov esi, esp
        and esp, 0xFFFFFFF0
        sub esp, 4
        push a3
        push a2
        push a1
        call eax
        mov esp, esi
        mov result, eax
    }
    return result;
}

static __inline uintptr_t elfCall4(const void *function, uintptr_t a1, uintptr_t a2, uintptr_t a3, uintptr_t a4)
{
    uintptr_t result;
    __asm {
        mov eax, function
        mov esi, esp
        and esp, 0xFFFFFFF0
        push a4
        push a3
        push a2
        push a1
        call eax
        mov esp, esi
        mov result, eax
    }
    return result;
}
#endif
