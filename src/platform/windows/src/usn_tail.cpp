#include "godxplr/platform/usn_tail.h"

#ifdef _WIN32

#include <winioctl.h>
#include <chrono>
#include <vector>
#include <shared_mutex>

namespace gx::platform::win32 {

UsnJournalTailer::UsnJournalTailer(NameIndex& index, wchar_t volumeLetter)
    : index_(index), volumeLetter_(volumeLetter) {}

UsnJournalTailer::~UsnJournalTailer() {
    stop();
}

bool UsnJournalTailer::start(UsnJournalCursor cursor) {
    if (isRunning_.load()) {
        return true;
    }

    cursor_ = cursor;
    currentUsn_.store(cursor.nextUsn);
    mutationsApplied_.store(0);
    stopRequested_.store(false);

    HANDLE hEv = ::CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!hEv) {
        return false;
    }
    stopEvent_.reset(hEv);

    workerThread_ = std::thread(&UsnJournalTailer::tailLoop, this);
    return true;
}

void UsnJournalTailer::stop() {
    stopRequested_.store(true);

    if (stopEvent_.isValid()) {
        ::SetEvent(stopEvent_.get());
    }

    if (workerThread_.joinable()) {
        workerThread_.join();
    }

    stopEvent_.reset();
    isRunning_.store(false);
}

void UsnJournalTailer::tailLoop() {
    isRunning_.store(true);

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
        isRunning_.store(false);
        return;
    }

    constexpr size_t kBufferSize = 64 * 1024;
    std::vector<uint8_t> buffer(kBufferSize);

    READ_USN_JOURNAL_DATA_V0 readData{};
    readData.StartUsn = cursor_.nextUsn;
    readData.ReasonMask = 0xFFFFFFFF;
    readData.ReturnOnlyOnClose = 0;
    readData.Timeout = 0;
    readData.BytesToWaitFor = 0;
    readData.UsnJournalID = cursor_.journalId;

    while (!stopRequested_.load()) {
        DWORD bytesReturned = 0;
        BOOL ok = ::DeviceIoControl(
            hVol.get(),
            FSCTL_READ_USN_JOURNAL,
            &readData,
            sizeof(readData),
            buffer.data(),
            static_cast<DWORD>(buffer.size()),
            &bytesReturned,
            nullptr
        );

        if (!ok) {
            DWORD err = ::GetLastError();
            if (err == ERROR_JOURNAL_ENTRY_DELETED ||
                err == ERROR_JOURNAL_NOT_ACTIVE ||
                err == ERROR_INVALID_PARAMETER) {
                // Journal wrapped or inactive
                break;
            }
            if (::WaitForSingleObject(stopEvent_.get(), 100) == WAIT_OBJECT_0) {
                break;
            }
            continue;
        }

        if (bytesReturned < sizeof(USN)) {
            if (::WaitForSingleObject(stopEvent_.get(), 50) == WAIT_OBJECT_0) {
                break;
            }
            continue;
        }

        USN nextUsn = *reinterpret_cast<const USN*>(buffer.data());
        readData.StartUsn = nextUsn;
        cursor_.nextUsn = static_cast<uint64_t>(nextUsn);
        currentUsn_.store(static_cast<uint64_t>(nextUsn));

        if (bytesReturned == sizeof(USN)) {
            // Buffer contained only nextUsn, wait for new records
            if (::WaitForSingleObject(stopEvent_.get(), 50) == WAIT_OBJECT_0) {
                break;
            }
            continue;
        }

        // Apply records under index writer lock
        {
            std::unique_lock<std::shared_mutex> lock(index_.mutex());

            const uint8_t* pByte = buffer.data() + sizeof(USN);
            const uint8_t* pEnd = buffer.data() + bytesReturned;

            while (pByte < pEnd) {
                const auto* rec = reinterpret_cast<const USN_RECORD_V2*>(pByte);
                if (rec->RecordLength == 0 || pByte + rec->RecordLength > pEnd) {
                    break;
                }

                if (rec->MajorVersion == 2) {
                    uint64_t frn = rec->FileReferenceNumber;
                    uint64_t parentFrn = rec->ParentFileReferenceNumber;
                    DWORD reason = rec->Reason;

                    const WCHAR* pNameW = reinterpret_cast<const WCHAR*>(pByte + rec->FileNameOffset);
                    size_t charCount = rec->FileNameLength / sizeof(WCHAR);
                    std::string nameUtf8 = utf16_to_utf8(std::wstring_view(pNameW, charCount));

                    if (reason & USN_REASON_FILE_DELETE) {
                        EntryId id = index_.findByFrn(frn);
                        if (id != kNoEntry) {
                            index_.remove(id);
                            mutationsApplied_++;
                        }
                    } else if (reason & USN_REASON_RENAME_NEW_NAME) {
                        EntryId id = index_.findByFrn(frn);
                        if (id != kNoEntry) {
                            index_.rename(id, nameUtf8);
                            index_.moveByFrn(id, parentFrn);
                        } else {
                            uint8_t flags = 0;
                            if (rec->FileAttributes & FILE_ATTRIBUTE_DIRECTORY) flags |= kDir;
                            if (rec->FileAttributes & FILE_ATTRIBUTE_HIDDEN) flags |= kHidden;
                            if (rec->FileAttributes & FILE_ATTRIBUTE_SYSTEM) flags |= kSystem;
                            index_.add(frn, parentFrn, nameUtf8, flags, 0, rec->TimeStamp.QuadPart);
                        }
                        mutationsApplied_++;
                    } else if (reason & USN_REASON_FILE_CREATE) {
                        EntryId id = index_.findByFrn(frn);
                        if (id == kNoEntry) {
                            uint8_t flags = 0;
                            if (rec->FileAttributes & FILE_ATTRIBUTE_DIRECTORY) flags |= kDir;
                            if (rec->FileAttributes & FILE_ATTRIBUTE_HIDDEN) flags |= kHidden;
                            if (rec->FileAttributes & FILE_ATTRIBUTE_SYSTEM) flags |= kSystem;
                            index_.add(frn, parentFrn, nameUtf8, flags, 0, rec->TimeStamp.QuadPart);
                            mutationsApplied_++;
                        }
                    } else if (reason & (USN_REASON_BASIC_INFO_CHANGE | USN_REASON_DATA_OVERWRITE | USN_REASON_DATA_EXTEND | USN_REASON_DATA_TRUNCATION)) {
                        EntryId id = index_.findByFrn(frn);
                        if (id != kNoEntry) {
                            uint8_t flags = 0;
                            if (rec->FileAttributes & FILE_ATTRIBUTE_DIRECTORY) flags |= kDir;
                            if (rec->FileAttributes & FILE_ATTRIBUTE_HIDDEN) flags |= kHidden;
                            if (rec->FileAttributes & FILE_ATTRIBUTE_SYSTEM) flags |= kSystem;
                            index_.updateMetadata(id, 0, rec->TimeStamp.QuadPart, flags);
                            mutationsApplied_++;
                        }
                    }
                }

                pByte += rec->RecordLength;
            }
        }
    }

    isRunning_.store(false);
}

} // namespace gx::platform::win32

#endif // _WIN32

