#pragma once

#include <string>
#include <cstdint>
#include <atomic>
#include <functional>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace gx::fileops {

enum class FileOpResult : uint8_t {
    Success,
    Cancelled,
    FailedSourceOpen,
    FailedDestOpen,
    FailedPreallocation,
    FailedRead,
    FailedWrite,
    FailedVerification,
    FailedRename,
    FailedDeleteSource,
    SourceNotFound,
    AlreadyExists,
    AccessDenied,
    SharingViolation,
    DiskFull
};

using ProgressCallback = std::function<void(uint64_t bytesJustWritten, uint64_t totalBytesWritten, uint64_t fileSize)>;

class StreamingFileExecutor {
public:
    StreamingFileExecutor() = default;
    ~StreamingFileExecutor() = default;

    StreamingFileExecutor(const StreamingFileExecutor&) = delete;
    StreamingFileExecutor& operator=(const StreamingFileExecutor&) = delete;

    // Checks whether two paths reside on the exact same Windows volume
    static bool isSameVolume(const std::wstring& pathA, const std::wstring& pathB);

    // Performs an atomic same-volume rename/move via SetFileInformationByHandle(FileRenameInfo)
    static FileOpResult atomicSameVolumeMove(
        const std::wstring& sourcePath,
        const std::wstring& destPath,
        bool replaceIfExists = false);

    // Executes a streaming copy with preallocation, double buffering, and metadata replication
    static FileOpResult streamingCopy(
        const std::wstring& sourcePath,
        const std::wstring& destPath,
        const std::atomic<bool>* cancelToken = nullptr,
        HANDLE resumeEvent = nullptr,
        ProgressCallback onProgress = nullptr,
        uint32_t* outCrc32 = nullptr);

    // Executes a cross-volume move: copy -> verify -> rename .gxpartial -> delete source
    static FileOpResult crossVolumeMove(
        const std::wstring& sourcePath,
        const std::wstring& destPath,
        const std::atomic<bool>* cancelToken = nullptr,
        HANDLE resumeEvent = nullptr,
        ProgressCallback onProgress = nullptr);

    // Preallocates physical disk clusters using SetFileInformationByHandle(FileAllocationInfo)
    static bool preallocateFile(HANDLE fileHandle, uint64_t targetSizeBytes);

    // Fast CRC32 computation on byte buffer
    static uint32_t computeCrc32(const uint8_t* data, size_t size, uint32_t seed = 0);
};

} // namespace gx::fileops

