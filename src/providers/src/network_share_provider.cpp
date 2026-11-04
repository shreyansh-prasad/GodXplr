#include "godxplr/providers/network_share_provider.h"
#include <winnetwk.h>

namespace gx::providers {

bool NetworkShareProvider::isUncPath(const std::wstring& path) {
    return path.size() >= 3 && path[0] == L'\\' && path[1] == L'\\' && path[2] != L'?';
}

NetworkShareStatus NetworkShareProvider::checkShareConnectivity(
    const std::wstring& uncPath,
    uint32_t /*timeoutMs*/)
{
    if (!isUncPath(uncPath)) {
        return NetworkShareStatus::NotNetworkPath;
    }

    DWORD attrs = GetFileAttributesW(uncPath.c_str());
    if (attrs != INVALID_FILE_ATTRIBUTES) {
        return NetworkShareStatus::Connected;
    }

    DWORD err = GetLastError();
    if (err == ERROR_ACCESS_DENIED) {
        return NetworkShareStatus::AccessDenied;
    }
    if (err == ERROR_BAD_NETPATH || err == ERROR_BAD_NET_NAME ||
        err == ERROR_NETWORK_UNREACHABLE || err == ERROR_SEM_TIMEOUT ||
        err == ERROR_DEV_NOT_EXIST)
    {
        return NetworkShareStatus::Disconnected;
    }

    return NetworkShareStatus::InvalidPath;
}

} // namespace gx::providers

