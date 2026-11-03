#pragma once

#include <filesystem>
#include <string>

namespace gx::distribution {

class PortableMode {
public:
    // Checks if the portable sentinel ("godxplr.portable") exists next to the binary
    [[nodiscard]] static bool isPortable(const std::filesystem::path& appDir);

    // Resolves data directory (local ./data in portable mode, %LOCALAPPDATA%/GodXplr/data in installed mode)
    [[nodiscard]] static std::filesystem::path getDataDirectory(const std::filesystem::path& appDir);

    // Resolves log directory
    [[nodiscard]] static std::filesystem::path getLogDirectory(const std::filesystem::path& appDir);
};

} // namespace gx::distribution
