#pragma once

#include <string>
#include <vector>
#include <optional>
#include <filesystem>

namespace gx::distribution {

struct RegistryBackup {
    std::wstring directoryCommand;
    std::wstring driveCommand;
    std::wstring folderCommand;
    bool hasBackup{false};
};

class RegistryManager {
public:
    RegistryManager() = default;

    // Checks if GodXplr is currently configured as the default folder opener
    [[nodiscard]] static bool isDefaultOpener(const std::wstring& godxplrExePath);

    // Sets GodXplr as the default folder opener under HKCU (backing up existing values first)
    static bool setDefaultOpener(
        const std::wstring& godxplrExePath,
        const std::filesystem::path& backupFilePath);

    // Restores original Explorer handlers from backup file or Windows standard defaults
    static bool restoreDefaultExplorer(const std::filesystem::path& backupFilePath);

    // Read a string value from HKCU
    static std::optional<std::wstring> readHkcuString(
        const std::wstring& subKey,
        const std::wstring& valueName);

    // Write a string value to HKCU
    static bool writeHkcuString(
        const std::wstring& subKey,
        const std::wstring& valueName,
        const std::wstring& data);

    // Delete a key under HKCU
    static bool deleteHkcuKey(const std::wstring& subKey);
};

} // namespace gx::distribution
