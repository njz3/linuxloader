#pragma once

// glibc functions the Lindbergh games did not call and Raw Thrills' Cruis'n Blast does: the fortified _chk variants
// of its compiler, wide strings with Linux's 32-bit wchar_t, the system's and the time's. libcMoreBridge.cpp
namespace LibcMoreBridge
{
    void initBridges();
}
