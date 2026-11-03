#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include <atomic>

namespace gx::power {

struct DuplicateFileEntry {
    std::wstring filePath;
    uint64_t fileSize{0};
    uint64_t mtime{0};
    uint64_t frn{0}; // File Reference Number for hard-link awareness
};

struct DuplicateGroup {
    uint64_t fileSize{0};
    uint32_t fullCrc32{0};
    std::vector<std::wstring> filePaths;
};

class DuplicateFinder {
public:
    DuplicateFinder() = default;
    ~DuplicateFinder() = default;

    // Executes the 3-tier cascade duplicate detection (Size -> Head/Tail -> Full Hash)
    static std::vector<DuplicateGroup> findDuplicates(
        const std::vector<DuplicateFileEntry>& candidates,
        const std::atomic<bool>* cancelToken = nullptr);

    // Fast partial hash of file head (first 4KB) and tail (last 4KB)
    static bool computeHeadTailHash(
        const std::wstring& filePath,
        uint64_t fileSize,
        uint32_t& outHash);

    // Full hash of file contents
    static bool computeFullHash(
        const std::wstring& filePath,
        uint32_t& outHash,
        const std::atomic<bool>* cancelToken = nullptr);
};

} // namespace gx::power
