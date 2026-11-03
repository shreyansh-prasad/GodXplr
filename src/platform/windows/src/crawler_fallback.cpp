#include "godxplr/platform/crawler_fallback.h"
#include "godxplr/platform/win32_common.h"
#include <chrono>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

namespace gx::platform::win32 {

constexpr uint32_t kMaxCrawlDepth = 32;

bool CrawlerFallback::crawl(
    const std::wstring& rootPath,
    NameIndex& outIndex,
    CrawlerStats& outStats,
    const std::function<void(size_t)>& progressCallback)
{
#ifdef _WIN32
    auto tStart = std::chrono::high_resolution_clock::now();

    uint64_t frnCounter = 1;
    uint64_t rootFrn = frnCounter++;

    outIndex.reserve(10000, 10000 * 32);

    crawlInternal(rootPath, rootFrn, 0, frnCounter, outIndex, outStats, progressCallback);

    outIndex.resolveParents();

    auto tEnd = std::chrono::high_resolution_clock::now();
    outStats.elapsedMs = std::chrono::duration<double, std::milli>(tEnd - tStart).count();
    return true;
#else
    return false;
#endif
}

void CrawlerFallback::crawlInternal(
    const std::wstring& currentPath,
    uint64_t parentFrn,
    uint32_t currentDepth,
    uint64_t& nextFrnCounter,
    NameIndex& outIndex,
    CrawlerStats& outStats,
    const std::function<void(size_t)>& progressCallback)
{
#ifdef _WIN32
    if (currentDepth > kMaxCrawlDepth) {
        return;
    }

    std::wstring searchPattern = currentPath;
    if (!searchPattern.empty() && searchPattern.back() != L'\\' && searchPattern.back() != L'/') {
        searchPattern += L'\\';
    }
    searchPattern += L'*';

    WIN32_FIND_DATAW findData{};
    HANDLE hFind = ::FindFirstFileExW(
        searchPattern.c_str(),
        FindExInfoBasic,
        &findData,
        FindExSearchNameMatch,
        nullptr,
        FIND_FIRST_EX_LARGE_FETCH
    );

    if (hFind == INVALID_HANDLE_VALUE) {
        return;
    }

    std::vector<std::pair<std::wstring, uint64_t>> subdirs;

    do {
        if (findData.cFileName[0] == L'.' &&
            (findData.cFileName[1] == L'\0' || (findData.cFileName[1] == L'.' && findData.cFileName[2] == L'\0'))) {
            continue;
        }

        uint64_t frn = nextFrnCounter++;
        std::wstring nameW(findData.cFileName);
        std::string nameUtf8 = utf16_to_utf8(nameW);

        bool isDir = (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        uint8_t flags = isDir ? kDir : 0;
        flags |= kMetaKnown;

        if (findData.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) flags |= kHidden;
        if (findData.dwFileAttributes & FILE_ATTRIBUTE_SYSTEM) flags |= kSystem;
        if (findData.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) flags |= kReparse;

        ULARGE_INTEGER ulSize{};
        ulSize.LowPart = findData.nFileSizeLow;
        ulSize.HighPart = findData.nFileSizeHigh;
        uint64_t fileSize = isDir ? 0 : ulSize.QuadPart;

        ULARGE_INTEGER ulTime{};
        ulTime.LowPart = findData.ftLastWriteTime.dwLowDateTime;
        ulTime.HighPart = findData.ftLastWriteTime.dwHighDateTime;
        uint64_t mtime = ulTime.QuadPart;

        if (isDir) {
            outStats.directoriesCount++;
            std::wstring subPath = currentPath;
            if (!subPath.empty() && subPath.back() != L'\\' && subPath.back() != L'/') {
                subPath += L'\\';
            }
            subPath += nameW;
            subdirs.emplace_back(std::move(subPath), frn);
        } else {
            outStats.filesCount++;
            outStats.totalBytes += fileSize;
        }

        outIndex.add(frn, parentFrn, nameUtf8, flags, fileSize, mtime);

        size_t totalSoFar = outStats.filesCount + outStats.directoriesCount;
        if (progressCallback && (totalSoFar % 8192 == 0)) {
            progressCallback(totalSoFar);
        }

    } while (::FindNextFileW(hFind, &findData));

    ::FindClose(hFind);

    for (const auto& [subPath, dirFrn] : subdirs) {
        crawlInternal(subPath, dirFrn, currentDepth + 1, nextFrnCounter, outIndex, outStats, progressCallback);
    }
#endif
}

} // namespace gx::platform::win32

