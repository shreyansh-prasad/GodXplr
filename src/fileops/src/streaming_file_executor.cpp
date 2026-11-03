#include "godxplr/fileops/streaming_file_executor.h"
#include <vector>
#include <memory>
#include <filesystem>

namespace gx::fileops {

bool StreamingFileExecutor::isSameVolume(const std::wstring& pathA, const std::wstring& pathB) {
    wchar_t volA[MAX_PATH] = { 0 };
    wchar_t volB[MAX_PATH] = { 0 };

    if (!GetVolumePathNameW(pathA.c_str(), volA, MAX_PATH)) {
        return false;
    }
    if (!GetVolumePathNameW(pathB.c_str(), volB, MAX_PATH)) {
        return false;
    }

    return _wcsicmp(volA, volB) == 0;
}

FileOpResult StreamingFileExecutor::atomicSameVolumeMove(
    const std::wstring& sourcePath,
    const std::wstring& destPath,
    bool replaceIfExists)
{
    DWORD flags = MOVEFILE_WRITE_THROUGH;
    if (replaceIfExists) {
        flags |= MOVEFILE_REPLACE_EXISTING;
    }

    if (!MoveFileExW(sourcePath.c_str(), destPath.c_str(), flags)) {
        DWORD err = GetLastError();
        if (err == ERROR_FILE_NOT_FOUND || err == ERROR_PATH_NOT_FOUND) {
            return FileOpResult::SourceNotFound;
        }
        if (err == ERROR_ALREADY_EXISTS || err == ERROR_FILE_EXISTS) {
            return FileOpResult::AlreadyExists;
        }
        if (err == ERROR_ACCESS_DENIED) {
            return FileOpResult::AccessDenied;
        }
        if (err == ERROR_SHARING_VIOLATION) {
            return FileOpResult::SharingViolation;
        }
        return FileOpResult::FailedRename;
    }

    return FileOpResult::Success;
}

bool StreamingFileExecutor::preallocateFile(HANDLE fileHandle, uint64_t targetSizeBytes) {
    if (fileHandle == INVALID_HANDLE_VALUE) {
        return false;
    }

    FILE_ALLOCATION_INFO fai{};
    fai.AllocationSize.QuadPart = static_cast<LONGLONG>(targetSizeBytes);

    return SetFileInformationByHandle(
        fileHandle,
        FileAllocationInfo,
        &fai,
        sizeof(fai)) != FALSE;
}

uint32_t StreamingFileExecutor::computeCrc32(const uint8_t* data, size_t size, uint32_t seed) {
    static const uint32_t kCrcTable[16] = {
        0x00000000, 0x1DB71064, 0x3B6E20C8, 0x26D930AC,
        0x76DC4190, 0x6B6B51F4, 0x4DB26158, 0x5005713C,
        0xEDB88320, 0xF00F9344, 0xD6D6A3E8, 0xCB61B38C,
        0x9B64C2B0, 0x86D3D2D4, 0xA00AE278, 0xBDBDF21C
    };

    uint32_t crc = ~seed;
    for (size_t i = 0; i < size; ++i) {
        uint8_t byte = data[i];
        crc = (crc >> 4) ^ kCrcTable[(crc & 0x0F) ^ (byte & 0x0F)];
        crc = (crc >> 4) ^ kCrcTable[(crc & 0x0F) ^ (byte >> 4)];
    }
    return ~crc;
}

FileOpResult StreamingFileExecutor::streamingCopy(
    const std::wstring& sourcePath,
    const std::wstring& destPath,
    const std::atomic<bool>* cancelToken,
    HANDLE resumeEvent,
    ProgressCallback onProgress,
    uint32_t* outCrc32)
{
    HANDLE hSrc = CreateFileW(
        sourcePath.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        FILE_FLAG_SEQUENTIAL_SCAN,
        nullptr);

    if (hSrc == INVALID_HANDLE_VALUE) {
        DWORD err = GetLastError();
        if (err == ERROR_FILE_NOT_FOUND || err == ERROR_PATH_NOT_FOUND) {
            return FileOpResult::SourceNotFound;
        }
        if (err == ERROR_ACCESS_DENIED) {
            return FileOpResult::AccessDenied;
        }
        if (err == ERROR_SHARING_VIOLATION) {
            return FileOpResult::SharingViolation;
        }
        return FileOpResult::FailedSourceOpen;
    }

    LARGE_INTEGER fileSizeLi{};
    if (!GetFileSizeEx(hSrc, &fileSizeLi)) {
        CloseHandle(hSrc);
        return FileOpResult::FailedRead;
    }
    uint64_t fileSize = static_cast<uint64_t>(fileSizeLi.QuadPart);

    FILETIME ftCreate{}, ftAccess{}, ftWrite{};
    GetFileTime(hSrc, &ftCreate, &ftAccess, &ftWrite);
    DWORD srcAttrs = GetFileAttributesW(sourcePath.c_str());

    // Temporary destination path with .gxpartial suffix
    std::wstring tempDestPath = destPath + L".gxpartial";

    HANDLE hDst = CreateFileW(
        tempDestPath.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);

    if (hDst == INVALID_HANDLE_VALUE) {
        CloseHandle(hSrc);
        DWORD err = GetLastError();
        if (err == ERROR_DISK_FULL) {
            return FileOpResult::DiskFull;
        }
        if (err == ERROR_ACCESS_DENIED) {
            return FileOpResult::AccessDenied;
        }
        return FileOpResult::FailedDestOpen;
    }

    // Spec 04 §2.3: Preallocate disk space
    preallocateFile(hDst, fileSize);

    // Buffer size: 1 MB for files > 1 MB, otherwise 64 KB
    const size_t bufferSize = (fileSize >= 1024 * 1024) ? (1024 * 1024) : (64 * 1024);
    std::vector<uint8_t> buffer(bufferSize);

    uint64_t totalBytesWritten = 0;
    uint32_t runningCrc = 0;

    while (totalBytesWritten < fileSize) {
        if (cancelToken && cancelToken->load(std::memory_order_relaxed)) {
            CloseHandle(hDst);
            CloseHandle(hSrc);
            DeleteFileW(tempDestPath.c_str());
            return FileOpResult::Cancelled;
        }

        if (resumeEvent) {
            WaitForSingleObject(resumeEvent, INFINITE);
        }

        DWORD bytesToRead = static_cast<DWORD>(std::min<uint64_t>(bufferSize, fileSize - totalBytesWritten));
        DWORD bytesRead = 0;

        if (!ReadFile(hSrc, buffer.data(), bytesToRead, &bytesRead, nullptr) || bytesRead == 0) {
            CloseHandle(hDst);
            CloseHandle(hSrc);
            DeleteFileW(tempDestPath.c_str());
            return FileOpResult::FailedRead;
        }

        runningCrc = computeCrc32(buffer.data(), bytesRead, runningCrc);

        DWORD bytesWritten = 0;
        if (!WriteFile(hDst, buffer.data(), bytesRead, &bytesWritten, nullptr) || bytesWritten != bytesRead) {
            CloseHandle(hDst);
            CloseHandle(hSrc);
            DeleteFileW(tempDestPath.c_str());
            return (GetLastError() == ERROR_DISK_FULL) ? FileOpResult::DiskFull : FileOpResult::FailedWrite;
        }

        totalBytesWritten += bytesWritten;
        if (onProgress) {
            onProgress(bytesWritten, totalBytesWritten, fileSize);
        }
    }

    // Flush and replicate metadata
    FlushFileBuffers(hDst);
    SetFileTime(hDst, &ftCreate, &ftAccess, &ftWrite);

    CloseHandle(hDst);
    CloseHandle(hSrc);

    if (srcAttrs != INVALID_FILE_ATTRIBUTES) {
        SetFileAttributesW(tempDestPath.c_str(), srcAttrs);
    }

    // Atomically rename .gxpartial -> target destPath
    if (!MoveFileExW(tempDestPath.c_str(), destPath.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED)) {
        DeleteFileW(tempDestPath.c_str());
        return FileOpResult::FailedRename;
    }

    if (outCrc32) {
        *outCrc32 = runningCrc;
    }

    return FileOpResult::Success;
}

FileOpResult StreamingFileExecutor::crossVolumeMove(
    const std::wstring& sourcePath,
    const std::wstring& destPath,
    const std::atomic<bool>* cancelToken,
    HANDLE resumeEvent,
    ProgressCallback onProgress)
{
    FileOpResult copyRes = streamingCopy(sourcePath, destPath, cancelToken, resumeEvent, onProgress);
    if (copyRes != FileOpResult::Success) {
        return copyRes;
    }

    // Spec 04 §2.2: Delete source only after destination copy and rename are verified
    SetFileAttributesW(sourcePath.c_str(), FILE_ATTRIBUTE_NORMAL);
    if (!DeleteFileW(sourcePath.c_str())) {
        return FileOpResult::FailedDeleteSource;
    }

    return FileOpResult::Success;
}

} // namespace gx::fileops

