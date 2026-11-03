#include "godxplr/platform/usn_enumerator.h"

#ifdef _WIN32

#include <winioctl.h>
#include <chrono>
#include <vector>

namespace gx::platform::win32 {

UsnEnumerator::UsnEnumerator(wchar_t volumeLetter)
    : volumeLetter_(volumeLetter) {}

bool UsnEnumerator::isElevated() {
    HANDLE hToken = nullptr;
    if (!::OpenProcessToken(::GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
        return false;
    }
    ScopedHandle tokenHandle(hToken);

    TOKEN_ELEVATION elevation{};
    DWORD retLen = 0;
    if (::GetTokenInformation(tokenHandle.get(), TokenElevation, &elevation, sizeof(elevation), &retLen)) {
        return elevation.TokenIsElevated != 0;
    }
    return false;
}

UsnStatus UsnEnumerator::queryCursor(UsnJournalCursor& outCursor) {
    std::wstring volPath = L"\\\\.\\";
    volPath.push_back(volumeLetter_);
    volPath.push_back(L':');

    ScopedHandle hVol(::CreateFileW(
        volPath.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS,
        nullptr
    ));

    if (!hVol.isValid()) {
        DWORD err = ::GetLastError();
        if (err == ERROR_ACCESS_DENIED) {
            return UsnStatus::AccessDenied;
        }
        return UsnStatus::InvalidHandle;
    }

    USN_JOURNAL_DATA_V0 journalData{};
    DWORD bytesReturned = 0;
    BOOL ok = ::DeviceIoControl(
        hVol.get(),
        FSCTL_QUERY_USN_JOURNAL,
        nullptr,
        0,
        &journalData,
        sizeof(journalData),
        &bytesReturned,
        nullptr
    );

    if (!ok) {
        DWORD err = ::GetLastError();
        if (err == ERROR_JOURNAL_NOT_ACTIVE) {
            return UsnStatus::JournalNotActive;
        }
        if (err == ERROR_INVALID_FUNCTION) {
            return UsnStatus::NotNtfsVolume;
        }
        return UsnStatus::IoError;
    }

    outCursor.journalId = journalData.UsnJournalID;
    outCursor.nextUsn = journalData.NextUsn;
    outCursor.lowestValidUsn = journalData.LowestValidUsn;
    return UsnStatus::Success;
}

UsnStatus UsnEnumerator::enumerateVolume(NameIndex& outIndex,
                                        UsnJournalCursor& outCursor,
                                        UsnEnumerationStats& outStats,
                                        const std::function<void(size_t)>& progressCallback) {
    auto tStart = std::chrono::high_resolution_clock::now();

    std::wstring volPath = L"\\\\.\\";
    volPath.push_back(volumeLetter_);
    volPath.push_back(L':');

    ScopedHandle hVol(::CreateFileW(
        volPath.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS,
        nullptr
    ));

    if (!hVol.isValid()) {
        DWORD err = ::GetLastError();
        if (err == ERROR_ACCESS_DENIED) {
            return UsnStatus::AccessDenied;
        }
        return UsnStatus::InvalidHandle;
    }

    // Step 1: Query journal cursor BEFORE enumeration begins
    USN_JOURNAL_DATA_V0 journalData{};
    DWORD bytesReturned = 0;
    BOOL ok = ::DeviceIoControl(
        hVol.get(),
        FSCTL_QUERY_USN_JOURNAL,
        nullptr,
        0,
        &journalData,
        sizeof(journalData),
        &bytesReturned,
        nullptr
    );

    if (!ok) {
        DWORD err = ::GetLastError();
        if (err == ERROR_JOURNAL_NOT_ACTIVE) {
            return UsnStatus::JournalNotActive;
        }
        if (err == ERROR_INVALID_FUNCTION) {
            return UsnStatus::NotNtfsVolume;
        }
        return UsnStatus::IoError;
    }

    outCursor.journalId = journalData.UsnJournalID;
    outCursor.nextUsn = journalData.NextUsn;
    outCursor.lowestValidUsn = journalData.LowestValidUsn;

    // Step 2: Configure MFT enumeration parameters
    MFT_ENUM_DATA_V1 enumData{};
    enumData.StartFileReferenceNumber = 0;
    enumData.LowUsn = 0;
    enumData.HighUsn = journalData.NextUsn;
    enumData.MinMajorVersion = 2;
    enumData.MaxMajorVersion = 2;

    // Allocate 1 MB aligned buffer for DeviceIoControl
    constexpr size_t kBufferSize = 1024 * 1024;
    std::vector<uint8_t> buffer(kBufferSize);

    // Pre-reserve index capacity
    outIndex.reserve(250000, 250000 * 32);

    while (true) {
        DWORD retBytes = 0;
        BOOL enumOk = ::DeviceIoControl(
            hVol.get(),
            FSCTL_ENUM_USN_DATA,
            &enumData,
            sizeof(enumData),
            buffer.data(),
            static_cast<DWORD>(buffer.size()),
            &retBytes,
            nullptr
        );

        if (!enumOk) {
            DWORD err = ::GetLastError();
            if (err == ERROR_HANDLE_EOF) {
                break; // Enumeration completed cleanly
            }
            return UsnStatus::IoError;
        }

        if (retBytes < sizeof(USN)) {
            break;
        }

        // First 8 bytes of the output buffer is the next start FRN
        enumData.StartFileReferenceNumber = *reinterpret_cast<const DWORDLONG*>(buffer.data());

        const uint8_t* pByte = buffer.data() + sizeof(USN);
        const uint8_t* pEnd = buffer.data() + retBytes;

        while (pByte < pEnd) {
            const auto* rec = reinterpret_cast<const USN_RECORD_V2*>(pByte);
            if (rec->RecordLength == 0 || pByte + rec->RecordLength > pEnd) {
                break;
            }

            if (rec->MajorVersion == 2) {
                const WCHAR* pNameW = reinterpret_cast<const WCHAR*>(
                    pByte + rec->FileNameOffset
                );
                size_t charCount = rec->FileNameLength / sizeof(WCHAR);
                std::wstring_view nameW(pNameW, charCount);
                std::string nameUtf8 = utf16_to_utf8(nameW);

                uint8_t flags = 0;
                if (rec->FileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                    flags |= kDir;
                    outStats.directoriesCount++;
                } else {
                    outStats.filesCount++;
                }
                if (rec->FileAttributes & FILE_ATTRIBUTE_HIDDEN) {
                    flags |= kHidden;
                }
                if (rec->FileAttributes & FILE_ATTRIBUTE_SYSTEM) {
                    flags |= kSystem;
                }
                if (rec->FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) {
                    flags |= kReparse;
                }

                uint64_t frn = rec->FileReferenceNumber;
                uint64_t parentFrn = rec->ParentFileReferenceNumber;
                uint64_t mtime = static_cast<uint64_t>(rec->TimeStamp.QuadPart);

                outIndex.add(frn, parentFrn, nameUtf8, flags, 0, mtime);
                outStats.rawRecordBytes += rec->RecordLength;
            }

            pByte += rec->RecordLength;
        }

        size_t totalSoFar = outStats.filesCount + outStats.directoriesCount;
        if (progressCallback && (totalSoFar % 16384 == 0)) {
            progressCallback(totalSoFar);
        }
    }

    // Step 3: Resolve all parents in one fast post-enumeration pass
    outIndex.resolveParents();

    auto tEnd = std::chrono::high_resolution_clock::now();
    outStats.elapsedMs = std::chrono::duration<double, std::milli>(tEnd - tStart).count();

    return UsnStatus::Success;
}

} // namespace gx::platform::win32

#endif // _WIN32

