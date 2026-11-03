#pragma once

#include <string>
#include <cstdint>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace gx::providers {

enum class NetworkShareStatus : uint8_t {
    NotNetworkPath,
    Connected,
    Disconnected,
    AccessDenied,
    InvalidPath
};

class NetworkShareProvider {
public:
    // Identifies whether a path is an SMB/UNC network share
    static bool isUncPath(const std::wstring& path);

    // Checks connectivity of a network share with bounded timeout
    static NetworkShareStatus checkShareConnectivity(
        const std::wstring& uncPath,
        uint32_t timeoutMs = 500);
};

} // namespace gx::providers
