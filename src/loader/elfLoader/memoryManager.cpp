#if defined(_WIN32) || defined(__MINGW32__)
#include "memoryManager.hpp"
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <wchar.h>

#include "../log/log.h"
#include <stdio.h>

void *MemoryManager::AllocateExecutable(size_t size, void *requestedAddr)
{
    void* ptr = nullptr;
    if (requestedAddr) {
        ptr = VirtualAlloc(requestedAddr, size, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    }
    if (!ptr) {
        ptr = VirtualAlloc(requestedAddr, size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    }
    if (!ptr && requestedAddr != nullptr)
    {
        uint8_t *scan = static_cast<uint8_t *>(requestedAddr);
        uint8_t *end = scan + size;
        bool canUse = true;

        while (scan < end)
        {
            MEMORY_BASIC_INFORMATION mbi;
            if (!VirtualQuery(scan, &mbi, sizeof(mbi)))
            {
                canUse = false;
                break;
            }

            SIZE_T regionSize = mbi.RegionSize;
            uintptr_t regionEnd = reinterpret_cast<uintptr_t>(mbi.BaseAddress) + regionSize;
            if (regionEnd > reinterpret_cast<uintptr_t>(end))
                regionSize = end - scan;

            if (mbi.State == MEM_COMMIT)
            {
                DWORD oldProtect;
                if (!VirtualProtect(scan, regionSize, PAGE_EXECUTE_READWRITE, &oldProtect))
                {
                    log_error("AllocateExecutable: VirtualProtect failed at %p (%zu bytes). Error: %lu",
                              scan, regionSize, GetLastError());
                    canUse = false;
                    break;
                }
            }
            else if (mbi.State == MEM_FREE)
            {
                if (!VirtualAlloc(scan, regionSize, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE))
                {
                    log_error("AllocateExecutable: VirtualAlloc(FREE) failed at %p (%zu bytes). Error: %lu",
                              scan, regionSize, GetLastError());
                    canUse = false;
                    break;
                }
            }
            else if (mbi.State == MEM_RESERVE)
            {
                if (!VirtualAlloc(scan, regionSize, MEM_COMMIT, PAGE_EXECUTE_READWRITE))
                {
                    log_error("AllocateExecutable: VirtualAlloc(RESERVE) failed at %p (%zu bytes). Error: %lu",
                              scan, regionSize, GetLastError());
                    canUse = false;
                    break;
                }
            }

            scan = reinterpret_cast<uint8_t *>(mbi.BaseAddress) + mbi.RegionSize;
        }

        if (canUse)
        {
            log_warn("AllocateExecutable: Using taken-over memory at %p (%zu bytes)", requestedAddr, size);
            ptr = requestedAddr;
        }
    }
    if (!ptr && requestedAddr != nullptr)
    {
        DWORD err = GetLastError();
        log_error("VirtualAlloc failed for address %p, size %zu. Error: %lu", requestedAddr, size, err);
        
        uint8_t* scan = static_cast<uint8_t*>(requestedAddr);
        uint8_t* end = scan + size;
        
        log_error("Scanning region %p to %p for conflicts...", scan, end);
        while (scan < end)
        {
            MEMORY_BASIC_INFORMATION mbi;
            if (VirtualQuery(scan, &mbi, sizeof(mbi)))
            {
                if (mbi.State != MEM_FREE)
                {
                    log_error("Conflict found at %p - RegionSize: 0x%zx, State: 0x%lx, Type: 0x%lx",
                              mbi.BaseAddress, mbi.RegionSize, mbi.State, mbi.Type);
                    break;
                }
                scan += mbi.RegionSize;
            }
            else
            {
                log_error("VirtualQuery failed at %p", scan);
                break;
            }
        }
    }
    return ptr;
}

#include <psapi.h>

void* MemoryManager::ReserveAddressSpace(size_t size, void* requestedAddr)
{
    void* result = VirtualAlloc(requestedAddr, size, MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (result) {
        log_info("MemoryManager: Pre-reserved address space: %p - %p (%zu bytes)", result, (char *)result + size, size);
        return result;
    }

    if (!requestedAddr)
        return nullptr;

    log_warn("MemoryManager: Pre-reserve failed at %p (%zu bytes). Taking over existing committed memory...",
             requestedAddr, size);

    uintptr_t scanAddr = reinterpret_cast<uintptr_t>(requestedAddr);
    uintptr_t alignedMax = scanAddr + size;
    bool allOk = true;

    while (scanAddr < alignedMax)
    {
        MEMORY_BASIC_INFORMATION mbi;
        if (!VirtualQuery(reinterpret_cast<void *>(scanAddr), &mbi, sizeof(mbi)))
            break;

        if (mbi.State == MEM_FREE)
        {
            SIZE_T freeSize = mbi.RegionSize;
            uintptr_t freeEnd = reinterpret_cast<uintptr_t>(mbi.BaseAddress) + freeSize;
            if (freeEnd > alignedMax)
                freeSize = alignedMax - scanAddr;

            void *allocated = VirtualAlloc(reinterpret_cast<void *>(scanAddr), freeSize, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
            if (allocated)
            {
                log_info("  Claimed free region at %p (%zu bytes)", allocated, freeSize);
            }
            else
            {
                log_error("  Failed to claim free region at %p (%zu bytes). Error: %lu", reinterpret_cast<void *>(scanAddr), freeSize, GetLastError());
                allOk = false;
            }
        }
        else if (mbi.State == MEM_COMMIT)
        {
            char moduleName[MAX_PATH] = {0};
            if (mbi.Type == MEM_IMAGE || mbi.Type == MEM_MAPPED)
                GetMappedFileNameA(GetCurrentProcess(), reinterpret_cast<void *>(scanAddr), moduleName, MAX_PATH);

            log_warn("  Taking over committed region at %p (Size=0x%zx, Type=0x%lx) %s%s", mbi.BaseAddress, mbi.RegionSize, mbi.Type,
                     moduleName[0] ? "Module: " : "", moduleName[0] ? moduleName : "");

            DWORD oldProtect;
            SIZE_T protectSize = mbi.RegionSize;
            uintptr_t regionEnd = reinterpret_cast<uintptr_t>(mbi.BaseAddress) + protectSize;
            if (regionEnd > alignedMax)
                protectSize = alignedMax - scanAddr;

            if (!VirtualProtect(reinterpret_cast<void *>(scanAddr), protectSize, PAGE_EXECUTE_READWRITE, &oldProtect))
            {
                log_error("  VirtualProtect failed at %p (%zu bytes). Error: %lu", reinterpret_cast<void *>(scanAddr), protectSize, GetLastError());
                allOk = false;
            }
        }
        else if (mbi.State == MEM_RESERVE)
        {
            SIZE_T commitSize = mbi.RegionSize;
            uintptr_t regionEnd = reinterpret_cast<uintptr_t>(mbi.BaseAddress) + commitSize;
            if (regionEnd > alignedMax)
                commitSize = alignedMax - scanAddr;

            void *committed = VirtualAlloc(reinterpret_cast<void *>(scanAddr), commitSize, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
            if (committed)
            {
                log_info("  Committed reserved region at %p (%zu bytes)", committed, commitSize);
            }
            else
            {
                log_error("  Failed to commit reserved region at %p (%zu bytes). Error: %lu", reinterpret_cast<void *>(scanAddr), commitSize, GetLastError());
                allOk = false;
            }
        }

        scanAddr = reinterpret_cast<uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;
    }

    return allOk ? requestedAddr : nullptr;
}

bool MemoryManager::CommitSegment(uintptr_t vaddr, uintptr_t memsz)
{
    uintptr_t alignedVAddr = vaddr & ~0xFFF;
    uintptr_t vEnd = vaddr + memsz;
    uintptr_t alignedEnd = (vEnd + 0xFFF) & ~0xFFF;
    uintptr_t alignedSize = alignedEnd - alignedVAddr;

    LPVOID result = VirtualAlloc((LPVOID)alignedVAddr, alignedSize, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (!result) {
        log_error("MemoryManager: VirtualAlloc MEM_COMMIT failed! VAddr: %p, Size: 0x%zx, Error: %lu",
                  (void*)alignedVAddr, alignedSize, GetLastError());
    }
    return result != NULL;
}

void *MemoryManager::customMemmove(void *dest, const void *src, size_t n)
{
    return memmove(dest, src, n);
}

wchar_t *MemoryManager::customWmemmove(wchar_t *dest, const wchar_t *src, size_t n)
{
    return wmemmove(dest, src, n);
}

int MemoryManager::customStrcoll(const char *s1, const char *s2)
{
    return strcoll(s1, s2);
}

size_t MemoryManager::customStrxfrm(char *dest, const char *src, size_t n)
{
    return strxfrm(dest, src, n);
}

// Zeroed, like the memory a Linux process gets fresh from the system: games read fields they have not set yet. Dead
// Heat's clTestMode reads its clock offsets while it is constructed: a year near 50000, and a Boost exception at
// startup, in more than half of the MSVC runs.
// What the game holds through these functions, in bytes (logAddressSpace)
static volatile LONG64 liveBytes = 0;

static void liveAdd(void *ptr)
{
    if (ptr)
        InterlockedExchangeAdd64(&liveBytes, (LONG64)_aligned_msize(ptr, 16, 0));
}

static void liveRemove(void *ptr)
{
    if (ptr)
        InterlockedExchangeAdd64(&liveBytes, -(LONG64)_aligned_msize(ptr, 16, 0));
}

// An allocation that fails, or of more than 256 MB: said once in a while, with who asked (a size read wrong)
static void allocationNote(const char *what, size_t size, const void *ptr, const void *caller)
{
    static volatile long notes = 0;
    // LL_ALLOCS=1: every allocation of 4 MB or more, with its caller
    static const bool allocs = getenv("LL_ALLOCS") != NULL;
    if (allocs && ptr && size >= (4u << 20))
        log_warn("%s(%zu) at %p, called from %p", what, size, ptr, caller);
    if ((!ptr || size >= (256u << 20)) && notes < 20)
    {
        notes++;
        log_warn("%s(%zu) %s, called from %p", what, size, ptr ? "is large" : "failed", caller);
        if (!ptr && notes == 1) // the address space then: what is free, its largest piece, what is reserved or used
        {
            size_t freeTotal = 0, freeLargest = 0, reserved = 0, committed = 0;
            MEMORY_BASIC_INFORMATION mbi;
            for (uintptr_t a = 0x10000; a < 0xFFFF0000u && VirtualQuery((void *)a, &mbi, sizeof(mbi)); a += mbi.RegionSize)
            {
                if (mbi.State == MEM_FREE)
                {
                    freeTotal += mbi.RegionSize;
                    if (mbi.RegionSize > freeLargest)
                        freeLargest = mbi.RegionSize;
                }
                else if (mbi.State == MEM_RESERVE)
                    reserved += mbi.RegionSize;
                else
                    committed += mbi.RegionSize;
                if ((uintptr_t)mbi.BaseAddress + mbi.RegionSize < a)
                    break;
            }
            log_warn("address space: %zu MB free (largest %zu MB), %zu MB reserved, %zu MB committed", freeTotal >> 20,
                     freeLargest >> 20, reserved >> 20, committed >> 20);
        }
    }
}

void MemoryManager::logAddressSpace(void)
{
    struct Block
    {
        uintptr_t base;
        size_t size;
        DWORD type;
    };
    Block biggest[10] = {};
    size_t freeTotal = 0, freeLargest = 0, reserved = 0, committed = 0;
    uintptr_t currentBase = 0;
    size_t currentSize = 0;
    DWORD currentType = 0;
    auto keep = [&](void) {
        for (int i = 0; i < 10; i++)
            if (currentSize > biggest[i].size)
            {
                memmove(&biggest[i + 1], &biggest[i], (9 - i) * sizeof(Block));
                biggest[i] = {currentBase, currentSize, currentType};
                break;
            }
    };
    MEMORY_BASIC_INFORMATION mbi;
    for (uintptr_t a = 0x10000; a < 0xFFFF0000u && VirtualQuery((void *)a, &mbi, sizeof(mbi)); a += mbi.RegionSize)
    {
        if (mbi.State == MEM_FREE)
        {
            freeTotal += mbi.RegionSize;
            if (mbi.RegionSize > freeLargest)
                freeLargest = mbi.RegionSize;
        }
        else
        {
            if (mbi.State == MEM_RESERVE)
                reserved += mbi.RegionSize;
            else
                committed += mbi.RegionSize;
            if ((uintptr_t)mbi.AllocationBase != currentBase)
            {
                keep();
                currentBase = (uintptr_t)mbi.AllocationBase;
                currentSize = 0;
                currentType = mbi.Type;
            }
            currentSize += mbi.RegionSize;
        }
        if ((uintptr_t)mbi.BaseAddress + mbi.RegionSize < a)
            break;
    }
    keep();
    log_warn("address space: %zu MB free (largest %zu MB), %zu MB reserved, %zu MB committed; the game's malloc holds %lld MB",
             freeTotal >> 20, freeLargest >> 20, reserved >> 20, committed >> 20, (long long)(liveBytes >> 20));
    // Committed memory by kind, private by the size of its allocations
    size_t image = 0, mapped = 0, privateSizes[4] = {0, 0, 0, 0}, privateCounts[4] = {0, 0, 0, 0};
    uintptr_t base = 0;
    size_t size = 0;
    DWORD type = 0;
    auto count = [&](void) {
        if (!size)
            return;
        if (type == MEM_IMAGE)
            image += size;
        else if (type == MEM_MAPPED)
            mapped += size;
        else
        {
            int k = size < (1u << 20) ? 0 : size < (4u << 20) ? 1 : size < (16u << 20) ? 2 : 3;
            privateSizes[k] += size;
            privateCounts[k]++;
        }
    };
    for (uintptr_t a = 0x10000; a < 0xFFFF0000u && VirtualQuery((void *)a, &mbi, sizeof(mbi)); a += mbi.RegionSize)
    {
        if (mbi.State != MEM_COMMIT)
            continue;
        if ((uintptr_t)mbi.AllocationBase != base)
        {
            count();
            base = (uintptr_t)mbi.AllocationBase;
            size = 0;
            type = mbi.Type;
        }
        size += mbi.RegionSize;
        if ((uintptr_t)mbi.BaseAddress + mbi.RegionSize < a)
            break;
    }
    count();
    log_warn("address space: committed %zu MB in images, %zu MB mapped, private %zu MB in %zu blocks under 1 MB, %zu MB in %zu "
             "of 1-4 MB, %zu MB in %zu of 4-16 MB, %zu MB in %zu over 16 MB",
             image >> 20, mapped >> 20, privateSizes[0] >> 20, privateCounts[0], privateSizes[1] >> 20, privateCounts[1],
             privateSizes[2] >> 20, privateCounts[2], privateSizes[3] >> 20, privateCounts[3]);
    for (int i = 0; i < 10 && biggest[i].size; i++)
        log_warn("address space: %08X, %zu MB, %s", (unsigned)biggest[i].base, biggest[i].size >> 20,
                 biggest[i].type == MEM_IMAGE ? "image" : biggest[i].type == MEM_MAPPED ? "mapped" : "private");
}

void *MemoryManager::customMalloc(size_t size)
{
    void *ptr = _aligned_malloc(size, 16);
    if (ptr)
        memset(ptr, 0, size);
    liveAdd(ptr);
    allocationNote("malloc", size, ptr, __builtin_return_address(0));
    return ptr;
}

static const DWORD MALLINFO_CACHE_MS = 2000;

struct mallinfo MemoryManager::customMallinfo(void)
{
    // glibc's mallinfo() is a few counter reads; a HeapWalk under HeapLock is not,
    // and some games call it every frame. Walk rarely and reuse the last snapshot.
    static struct mallinfo cached = {};
    static DWORD lastWalk = 0;
    static LONG walking = 0;

    if (lastWalk != 0 && GetTickCount() - lastWalk < MALLINFO_CACHE_MS)
        return cached;

    HANDLE Heap = GetProcessHeap();
    if (!Heap)
        return cached;

    // One walker at a time; latecomers get the previous snapshot.
    if (InterlockedCompareExchange(&walking, 1, 0) != 0)
        return cached;

    struct mallinfo Info = {};

    HeapLock(Heap);

    PROCESS_HEAP_ENTRY Entry = {};
    while (HeapWalk(Heap, &Entry))
    {
        if (Entry.wFlags & PROCESS_HEAP_ENTRY_BUSY)
        {
            Info.uordblks += (int)Entry.cbData;
        }
        else
        {
            Info.fordblks += (int)Entry.cbData;
        }
    }

    HeapUnlock(Heap);

    Info.arena = Info.uordblks + Info.fordblks;

    cached = Info;
    lastWalk = GetTickCount();

    InterlockedExchange(&walking, 0);
    return cached;
}

void MemoryManager::customFree(void *ptr)
{
    liveRemove(ptr);
    _aligned_free(ptr);
}

void *MemoryManager::customCalloc(size_t nmemb, size_t size)
{
    if (nmemb != 0 && size > SIZE_MAX / nmemb)
    {
        return NULL;
    }
    size_t total = nmemb * size;
    void *ptr = _aligned_malloc(total, 16);
    if (ptr)
        memset(ptr, 0, total);
    liveAdd(ptr);
    allocationNote("calloc", total, ptr, __builtin_return_address(0));
    return ptr;
}

void *MemoryManager::customRealloc(void *ptr, size_t size)
{
    liveRemove(ptr);
    void *ret = _aligned_realloc(ptr, size, 16);
    liveAdd(ret ? ret : (size ? ptr : NULL));
    if (size)
        allocationNote("realloc", size, ret, __builtin_return_address(0));
    return ret;
}

void *MemoryManager::customMemalign(size_t alignment, size_t size)
{
    return _aligned_malloc(size, alignment);
} 

int MemoryManager::customPosixMemalign(void **ptr, size_t alignment, size_t size)
{
    if (alignment % sizeof(void*) != 0 || (alignment & (alignment - 1)) != 0)
    {
        return EINVAL;
    }

    *ptr = _aligned_malloc(size, alignment);
    return *ptr ? 0 : ENOMEM;
}

char *MemoryManager::customStrndup(const char *src, size_t size)
{
    size_t len = strnlen(src, size);
    len = len < size ? len : size;
    char *dst = (char *)_aligned_malloc(len + 1, 16);
    if (!dst)
        return NULL;
    memcpy(dst, src, len);
    dst[len] = '\0';
    return dst;
}

char *MemoryManager::customStrdup(const char *s)
{
    if (!s)
        return nullptr;
    size_t len = strlen(s) + 1;
    void *ptr = _aligned_malloc(len, 16);
    if (ptr)
        memcpy(ptr, s, len);
    return (char *)ptr;
}

#endif
