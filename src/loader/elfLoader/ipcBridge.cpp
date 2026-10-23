#if defined(_WIN32) || defined(__MINGW32__)
#include "ipcBridge.hpp"
#include "../log/log.h"
#include "symbolResolver.hpp"

#include <condition_variable>
#include <errno.h>
#include <mutex>
#include <stdarg.h>
#include <vector>

#define MAP(name, func) SymbolResolver::GetInstance().RegisterVTable(name, reinterpret_cast<void *>(func))

// System V semaphores within the game's own process (Maximum Tune 3 guards its shared state with them)
struct SemSet
{
    int key;
    std::vector<int> values; // empty once removed
};

struct LinuxSembuf
{
    unsigned short num;
    short op;
    short flags;
};

static std::mutex semLock;
static std::condition_variable semChanged;
static std::vector<SemSet> semSets;

static int bridgeSemget(int key, int nsems, int flags)
{
    std::lock_guard<std::mutex> lock(semLock);
    if (key != 0) // IPC_PRIVATE
        for (size_t i = 0; i < semSets.size(); i++)
            if (semSets[i].key == key && !semSets[i].values.empty())
            {
                if ((flags & 01000) && (flags & 02000)) // IPC_CREAT | IPC_EXCL
                {
                    errno = EEXIST;
                    return -1;
                }
                return (int)i;
            }
    if (key != 0 && !(flags & 01000))
    {
        errno = ENOENT;
        return -1;
    }
    semSets.push_back({key, std::vector<int>(nsems > 0 ? nsems : 1, 0)});
    log_debug("semget(key=0x%X, %d) = %d", key, nsems, (int)semSets.size() - 1);
    return (int)semSets.size() - 1;
}

static bool semCanApply(const SemSet &set, const LinuxSembuf *ops, unsigned count)
{
    for (unsigned i = 0; i < count; i++)
    {
        int value = set.values[ops[i].num];
        if ((ops[i].op < 0 && value < -ops[i].op) || (ops[i].op == 0 && value != 0))
            return false;
    }
    return true;
}

static int bridgeSemop(int id, const LinuxSembuf *ops, unsigned count)
{
    std::unique_lock<std::mutex> lock(semLock);
    if (id < 0 || id >= (int)semSets.size() || semSets[id].values.empty())
    {
        errno = EINVAL;
        return -1;
    }
    bool noWait = false;
    for (unsigned i = 0; i < count; i++)
    {
        if (ops[i].num >= semSets[id].values.size())
        {
            errno = EFBIG;
            return -1;
        }
        noWait |= (ops[i].flags & 04000) != 0; // IPC_NOWAIT
    }
    while (!semCanApply(semSets[id], ops, count))
    {
        if (noWait)
        {
            errno = EAGAIN;
            return -1;
        }
        semChanged.wait(lock);
        if (semSets[id].values.empty())
        {
            errno = EIDRM;
            return -1;
        }
    }
    for (unsigned i = 0; i < count; i++)
        semSets[id].values[ops[i].num] += ops[i].op;
    semChanged.notify_all();
    return 0;
}

static int bridgeSemctl(int id, int num, int cmd, ...)
{
    va_list args;
    va_start(args, cmd);
    void *arg = va_arg(args, void *); // union semun
    va_end(args);
    std::lock_guard<std::mutex> lock(semLock);
    if (id < 0 || id >= (int)semSets.size() || semSets[id].values.empty())
    {
        errno = EINVAL;
        return -1;
    }
    std::vector<int> &values = semSets[id].values;
    switch (cmd & 0xFF) // without IPC_64
    {
    case 0: // IPC_RMID
        values.clear();
        semSets[id].key = -1;
        semChanged.notify_all();
        return 0;
    case 2: // IPC_STAT: struct semid_ds, sem_nsems after the 36-byte ipc_perm and two times
        memset(arg, 0, 64);
        ((uint32_t *)arg)[13] = (uint32_t)values.size();
        return 0;
    case 12: // GETVAL
        return num >= 0 && num < (int)values.size() ? values[num] : -1;
    case 13: // GETALL
        for (size_t i = 0; i < values.size(); i++)
            ((unsigned short *)arg)[i] = (unsigned short)values[i];
        return 0;
    case 16: // SETVAL
        if (num >= 0 && num < (int)values.size())
            values[num] = (int)(intptr_t)arg;
        semChanged.notify_all();
        return 0;
    case 17: // SETALL
        for (size_t i = 0; i < values.size(); i++)
            values[i] = ((unsigned short *)arg)[i];
        semChanged.notify_all();
        return 0;
    default:
        return 0;
    }
}

// The key from the path's name: one process, so only the same path and id must give the same key
static int bridgeFtok(const char *path, int id)
{
    uint32_t hash = 2166136261u;
    for (const char *c = path ? path : ""; *c; c++)
        hash = (hash ^ (uint8_t)*c) * 16777619u;
    return (int)(((uint32_t)(id & 0xFF) << 24) | (hash & 0xFFFFFF));
}

namespace IpcBridge
{

    void initBridges()
    {
        log_info("Initializing IPC Bridges...");

        MAP("shmget", bridgeShmget);
        MAP("shmat", bridgeShmat);
        MAP("shmctl", bridgeShmctl);
        MAP("shmdt", bridgeShmdt);
        MAP("semget", bridgeSemget);
        MAP("semop", bridgeSemop);
        MAP("semctl", bridgeSemctl);
        MAP("ftok", bridgeFtok);
    }
}
extern "C"
{
    int bridgeShmget(int key, size_t size, int shmflg)
    {
        log_debug("shmget(key=%d, size=%zu, flags=0x%X)", key, size, shmflg);

        if (g_shmMap.find(key) != g_shmMap.end())
        {
            log_trace("shmget: Returning existing key %d", key);
            return key;
        }

        char mapName[64];
        sprintf(mapName, "Global\\Windy_SHM_%d", key);

        HANDLE hMap = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, (DWORD)size, mapName);

        if (!hMap)
        {
            log_error("shmget failed: CreateFileMapping error %lu", GetLastError());
            return -1;
        }

        if (GetLastError() == ERROR_ALREADY_EXISTS)
        {
            log_trace("shmget: Opened existing mapping for key %d", key);
        }

        ShmInfo info;
        info.hMap = hMap;
        info.pMem = NULL;
        info.size = size;
        info.key = key;

        g_shmMap[key] = info;

        log_debug("shmget: Created mapping for key %d, size %zu", key, size);
        return key;
    }

    void *bridgeShmat(int shmid, const void *shmaddr, int shmflg)
    {
        log_trace("shmat(id=%d, addr=%p, flags=0x%X)", shmid, shmaddr, shmflg);

        if (g_shmMap.find(shmid) == g_shmMap.end())
        {
            log_error("shmat failed: Invalid shmid %d", shmid);
            return (void *)-1;
        }

        ShmInfo &info = g_shmMap[shmid];

        if (info.pMem)
        {
            return info.pMem;
        }

        info.pMem = MapViewOfFile(info.hMap, FILE_MAP_ALL_ACCESS, 0, 0, 0);

        if (!info.pMem)
        {
            log_error("shmat failed: MapViewOfFile error %lu", GetLastError());
            return (void *)-1;
        }

        log_debug("shmat: Attached id=%d at %p", shmid, info.pMem);
        return info.pMem;
    }

    int bridgeShmctl(int shmid, int cmd, void *buf)
    {
        if (cmd == 0)
        { // IPC_RMID
            log_debug("shmctl: Removing id=%d", shmid);

            if (g_shmMap.find(shmid) != g_shmMap.end())
            {
                ShmInfo &info = g_shmMap[shmid];

                if (info.pMem)
                {
                    UnmapViewOfFile(info.pMem);
                }

                if (info.hMap)
                {
                    CloseHandle(info.hMap);
                }

                g_shmMap.erase(shmid);
            }
        }

        return 0;
    }

    int bridgeShmdt(const void *shmaddr)
    {
        log_trace("shmdt(%p)", shmaddr);
        return 0;
    }
}   

#endif