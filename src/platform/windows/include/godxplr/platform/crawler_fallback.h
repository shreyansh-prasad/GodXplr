#pragma once

#include "godxplr/core/name_index.h"
#include <string>
#include <functional>
#include <cstdint>

namespace gx::platform::win32 {

struct CrawlerStats {
    size_t filesCount{0};
    size_t directoriesCount{0};
    size_t totalBytes{0};
    double elapsedMs{0.0};
};

class CrawlerFallback {
public:
    CrawlerFallback() = default;
    ~CrawlerFallback() = default;

    // Recursively crawls a directory, FAT/exFAT/USB volume, or network share using FindFirstFileExW
    static bool crawl(
        const std::wstring& rootPath,
        NameIndex& outIndex,
        CrawlerStats& outStats,
        const std::function<void(size_t entriesSoFar)>& progressCallback = nullptr);

private:
    static void crawlInternal(
        const std::wstring& currentPath,
        uint64_t parentFrn,
        uint32_t currentDepth,
        uint64_t& nextFrnCounter,
        NameIndex& outIndex,
        CrawlerStats& outStats,
        const std::function<void(size_t)>& progressCallback);
};

} // namespace gx::platform::win32
