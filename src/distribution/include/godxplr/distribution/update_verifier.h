#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <cstdint>

namespace gx::distribution {

struct UpdateManifest {
    std::string version;
    std::string sha256Hex;
    uint64_t packageSizeBytes{0};
    std::string releaseNotes;
};

class UpdateVerifier {
public:
    // Computes fast SHA-256 hex string of a file
    [[nodiscard]] static std::string computeFileSha256(const std::filesystem::path& filePath);

    // Verifies package against manifest
    [[nodiscard]] static bool verifyPackage(
        const std::filesystem::path& packagePath,
        const UpdateManifest& manifest);

    // Stages update into isolated .gxupdate folder
    static bool stageUpdate(
        const std::filesystem::path& packagePath,
        const std::filesystem::path& targetDir);

    // Cleans up .gxupdate staging remnants
    static bool cleanStagedUpdate(const std::filesystem::path& targetDir);
};

} // namespace gx::distribution

