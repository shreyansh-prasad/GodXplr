#include "godxplr/distribution/portable_mode.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shlobj.h>

namespace gx::distribution {

bool PortableMode::isPortable(const std::filesystem::path& appDir) {
    std::error_code ec;
    return std::filesystem::exists(appDir / "godxplr.portable", ec);
}

std::filesystem::path PortableMode::getDataDirectory(const std::filesystem::path& appDir) {
    if (isPortable(appDir)) {
        return appDir / "data";
    }

    // Windows installed mode: %LOCALAPPDATA%\GodXplr\data
    wchar_t localAppData[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, 0, localAppData))) {
        return std::filesystem::path(localAppData) / "GodXplr" / "data";
    }

    return appDir / "data";
}

std::filesystem::path PortableMode::getLogDirectory(const std::filesystem::path& appDir) {
    if (isPortable(appDir)) {
        return appDir / "logs";
    }

    wchar_t localAppData[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, 0, localAppData))) {
        return std::filesystem::path(localAppData) / "GodXplr" / "logs";
    }

    return appDir / "logs";
}

} // namespace gx::distribution
