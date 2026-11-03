#include "godxplr/power/duplicate_finder.h"
#include "godxplr/fileops/streaming_file_executor.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <algorithm>

namespace gx::power {

bool DuplicateFinder::computeHeadTailHash(
    const std::wstring& filePath,
    uint64_t fileSize,
    uint32_t& outHash)
{
    HANDLE hFile = CreateFileW(
        filePath.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        FILE_FLAG_SEQUENTIAL_SCAN,
        nullptr);

    if (hFile == INVALID_HANDLE_VALUE) {
        return false;
    }

    if (fileSize <= 8192) {
        std::vector<uint8_t> buf(static_cast<size_t>(fileSize));
        DWORD bytesRead = 0;
        if (!ReadFile(hFile, buf.data(), static_cast<DWORD>(fileSize), &bytesRead, nullptr) || bytesRead != fileSize) {
            CloseHandle(hFile);
            return false;
        }
        CloseHandle(hFile);
        outHash = fileops::StreamingFileExecutor::computeCrc32(buf.data(), bytesRead);
        return true;
    }

    // Read first 4KB
    std::vector<uint8_t> head(4096);
    DWORD bytesRead = 0;
    if (!ReadFile(hFile, head.data(), 4096, &bytesRead, nullptr) || bytesRead != 4096) {
        CloseHandle(hFile);
        return false;
    }

    // Seek and read last 4KB
    LARGE_INTEGER li{};
    li.QuadPart = static_cast<LONGLONG>(fileSize - 4096);
    if (!SetFilePointerEx(hFile, li, nullptr, FILE_BEGIN)) {
        CloseHandle(hFile);
        return false;
    }

    std::vector<uint8_t> tail(4096);
    if (!ReadFile(hFile, tail.data(), 4096, &bytesRead, nullptr) || bytesRead != 4096) {
        CloseHandle(hFile);
        return false;
    }

    CloseHandle(hFile);

    uint32_t hCrc = fileops::StreamingFileExecutor::computeCrc32(head.data(), head.size());
    outHash = fileops::StreamingFileExecutor::computeCrc32(tail.data(), tail.size(), hCrc);
    return true;
}

bool DuplicateFinder::computeFullHash(
    const std::wstring& filePath,
    uint32_t& outHash,
    const std::atomic<bool>* cancelToken)
{
    HANDLE hFile = CreateFileW(
        filePath.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        FILE_FLAG_SEQUENTIAL_SCAN,
        nullptr);

    if (hFile == INVALID_HANDLE_VALUE) {
        return false;
    }

    const size_t kChunkSize = 64 * 1024;
    std::vector<uint8_t> buf(kChunkSize);
    uint32_t runningCrc = 0;
    DWORD bytesRead = 0;

    while (ReadFile(hFile, buf.data(), static_cast<DWORD>(buf.size()), &bytesRead, nullptr) && bytesRead > 0) {
        if (cancelToken && cancelToken->load(std::memory_order_relaxed)) {
            CloseHandle(hFile);
            return false;
        }
        runningCrc = fileops::StreamingFileExecutor::computeCrc32(buf.data(), bytesRead, runningCrc);
    }

    CloseHandle(hFile);
    outHash = runningCrc;
    return true;
}

std::vector<DuplicateGroup> DuplicateFinder::findDuplicates(
    const std::vector<DuplicateFileEntry>& candidates,
    const std::atomic<bool>* cancelToken)
{
    if (candidates.size() < 2) {
        return {};
    }

    // Tier 1: Group by file size
    std::unordered_map<uint64_t, std::vector<const DuplicateFileEntry*>> sizeMap;
    for (const auto& entry : candidates) {
        if (entry.fileSize > 0) { // skip empty files
            sizeMap[entry.fileSize].push_back(&entry);
        }
    }

    std::vector<DuplicateGroup> result;

    for (const auto& [size, sizeGroup] : sizeMap) {
        if (cancelToken && cancelToken->load(std::memory_order_relaxed)) {
            break;
        }

        if (sizeGroup.size() < 2) {
            continue; // Unique file size, cannot be a duplicate
        }

        // Tier 2: Group by partial (head+tail) hash
        std::unordered_map<uint32_t, std::vector<const DuplicateFileEntry*>> partialMap;
        for (const auto* file : sizeGroup) {
            uint32_t pHash = 0;
            if (computeHeadTailHash(file->filePath, file->fileSize, pHash)) {
                partialMap[pHash].push_back(file);
            }
        }

        // Tier 3: Group by full hash
        for (const auto& [_, partialGroup] : partialMap) {
            if (partialGroup.size() < 2) {
                continue;
            }

            std::unordered_map<uint32_t, std::vector<std::wstring>> fullMap;
            for (const auto* file : partialGroup) {
                uint32_t fHash = 0;
                if (computeFullHash(file->filePath, fHash, cancelToken)) {
                    fullMap[fHash].push_back(file->filePath);
                }
            }

            for (const auto& [fullCrc, matchedPaths] : fullMap) {
                if (matchedPaths.size() >= 2) {
                    DuplicateGroup grp;
                    grp.fileSize = size;
                    grp.fullCrc32 = fullCrc;
                    grp.filePaths = matchedPaths;
                    result.push_back(std::move(grp));
                }
            }
        }
    }

    return result;
}

} // namespace gx::power
