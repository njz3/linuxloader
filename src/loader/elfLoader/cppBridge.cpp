#if defined(_WIN32) || defined(__MINGW32__)
#include "cppBridge.hpp"
#include "symbolResolver.hpp"
#include "../log/log.h"
#include <cstdio>

#define MAP(name, func) SymbolResolver::GetInstance().RegisterVTable(name, reinterpret_cast<void *>(func))

namespace CppBridge
{

    void initBridges()
    {
        log_info("Initializing C++ Bridges...");

        MAP("_ZdlPv", bridgeOperatorDelete);
        MAP("_Znwj", bridgeOperatorNew);
        MAP("_Znaj", bridgeOperatorNewArray);
        MAP("_ZdaPv", bridgeOperatorDelete); 
    }

    // Zeroed, as MemoryManager::customMalloc: objects whose constructors read members not set yet (Dead Heat's
    // clTestMode) find zeros, as on Linux
    void *bridgeOperatorNew(size_t size)
    {
        log_trace("Intercepted operator new, size: %zu", size);
        return calloc(1, size ? size : 1);
    }

    void *bridgeOperatorNewArray(size_t size)
    {
        log_trace("Intercepted operator new[], size: %zu", size);
        return calloc(1, size ? size : 1);
    }

    void bridgeOperatorDelete(void *ptr)
    {
        log_trace("Intercepted operator delete, ptr: %p", ptr);
        free(ptr);
    }

} // namespace CppBridge

#endif