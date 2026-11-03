#ifdef _WIN32

#include "godxplr/platform/win32_directory_provider.h"
#include <algorithm>
#include <vector>

namespace gx::platform::win32 {

namespace {

constexpr size_t kDirectoryBatchSize = 512;
constexpr DWORD kWatchBufferSize = 64 * 1024; // 64 KB for change notification buffer

uint64_t fnv1a64(std::string_view str, uint64_t seed = 14695981039346656037ULL) noexcept {
    uint64_t hash = seed;
    for (char c : str) {
        // case-insensitive hash for Windows paths
        char lower = (c >= 'A' && c <= 'Z') ? static_cast<char>(c + 32) : c;
        hash ^= static_cast<uint64_t>(static_cast<unsigned char>(lower));
        hash *= 1099511628211ULL;
    }
    return hash;
}

} // namespace

// ============================================================================
// Win32DirectoryWatcher
// ============================================================================

Win32DirectoryWatcher::Win32DirectoryWatcher(std::wstring directoryPath, std::shared_ptr<IWatchListener> listener)
    : path_(std::move(directoryPath))
    , listener_(std::move(listener))
{
    stopEvent_.reset(::CreateEventW(nullptr, TRUE, FALSE, nullptr));
    if (!stopEvent_.isValid()) {
        return;
    }

    HANDLE hDir = ::CreateFileW(
        path_.c_str(),
        FILE_LIST_DIRECTORY,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
        nullptr
    );

    if (hDir == INVALID_HANDLE_VALUE) {
        return;
    }

    dirHandle_.reset(hDir);
    active_.store(true, std::memory_order_release);
    workerThread_ = std::thread(&Win32DirectoryWatcher::watchWorker, this);
}

Win32DirectoryWatcher::~Win32DirectoryWatcher() {
    stop();
}

bool Win32DirectoryWatcher::isActive() const noexcept {
    return active_.load(std::memory_order_acquire);
}

void Win32DirectoryWatcher::stop() {
    if (!active_.exchange(false, std::memory_order_acq_rel)) {
        return;
    }

    if (stopEvent_.isValid()) {
        ::SetEvent(stopEvent_.get());
    }

    if (dirHandle_.isValid()) {
        ::CancelIoEx(dirHandle_.get(), nullptr);
    }

    if (workerThread_.joinable()) {
        workerThread_.join();
    }

    dirHandle_.reset();
}

void Win32DirectoryWatcher::watchWorker() {
    std::vector<BYTE> buffer(kWatchBufferSize);
    const DWORD notifyFilter = FILE_NOTIFY_CHANGE_FILE_NAME |
                               FILE_NOTIFY_CHANGE_DIR_NAME |
                               FILE_NOTIFY_CHANGE_ATTRIBUTES |
                               FILE_NOTIFY_CHANGE_SIZE |
                               FILE_NOTIFY_CHANGE_LAST_WRITE;

    while (active_.load(std::memory_order_acquire)) {
        OVERLAPPED ov{};
        ov.hEvent = ::CreateEventW(nullptr, TRUE, FALSE, nullptr);
        if (!ov.hEvent) {
            break;
        }

        BOOL ok = ::ReadDirectoryChangesW(
            dirHandle_.get(),
            buffer.data(),
            static_cast<DWORD>(buffer.size()),
            FALSE, // shallow watch for active directory
            notifyFilter,
            nullptr,
            &ov,
            nullptr
        );

        if (!ok) {
            DWORD err = ::GetLastError();
            ::CloseHandle(ov.hEvent);
            if (err == ERROR_NOTIFY_ENUM_DIR) {
                // Buffer overflow: force full rescan
                if (listener_) {
                    WatchEvent evt;
                    evt.kind = WatchEvent::Kind::RescanRequired;
                    evt.overflowRequiresRescan = true;
                    listener_->onWatchEvent(evt);
                }
                continue;
            }
            break;
        }

        HANDLE handles[2] = { stopEvent_.get(), ov.hEvent };
        DWORD waitResult = ::WaitForMultipleObjects(2, handles, FALSE, INFINITE);

        if (waitResult == WAIT_OBJECT_0) {
            // Stop requested
            ::CancelIoEx(dirHandle_.get(), &ov);
            ::CloseHandle(ov.hEvent);
            break;
        }

        if (waitResult == WAIT_OBJECT_0 + 1) {
            DWORD bytesReturned = 0;
            BOOL resOk = ::GetOverlappedResult(dirHandle_.get(), &ov, &bytesReturned, FALSE);
            ::CloseHandle(ov.hEvent);

            if (!resOk || bytesReturned == 0) {
                DWORD err = ::GetLastError();
                if (err == ERROR_NOTIFY_ENUM_DIR || bytesReturned == 0) {
                    if (listener_) {
                        WatchEvent evt;
                        evt.kind = WatchEvent::Kind::RescanRequired;
                        evt.overflowRequiresRescan = true;
                        listener_->onWatchEvent(evt);
                    }
                }
                continue;
            }

            // Parse FILE_NOTIFY_INFORMATION records
            const BYTE* ptr = buffer.data();
            while (ptr && active_.load(std::memory_order_acquire)) {
                const auto* info = reinterpret_cast<const FILE_NOTIFY_INFORMATION*>(ptr);
                std::wstring_view nameView(info->FileName, info->FileNameLength / sizeof(WCHAR));
                std::string relPath = utf16_to_utf8(nameView);

                WatchEvent evt;
                evt.relativePath = relPath;

                switch (info->Action) {
                    case FILE_ACTION_ADDED:
                        evt.kind = WatchEvent::Kind::Added;
                        break;
                    case FILE_ACTION_REMOVED:
                        evt.kind = WatchEvent::Kind::Removed;
                        break;
                    case FILE_ACTION_MODIFIED:
                        evt.kind = WatchEvent::Kind::Modified;
                        break;
                    case FILE_ACTION_RENAMED_OLD_NAME:
                        evt.kind = WatchEvent::Kind::RenamedOld;
                        break;
                    case FILE_ACTION_RENAMED_NEW_NAME:
                        evt.kind = WatchEvent::Kind::RenamedNew;
                        break;
                    default:
                        evt.kind = WatchEvent::Kind::Modified;
                        break;
                }

                if (listener_) {
                    listener_->onWatchEvent(evt);
                }

                if (info->NextEntryOffset == 0) {
                    break;
                }
                ptr += info->NextEntryOffset;
            }
        } else {
            ::CloseHandle(ov.hEvent);
            break;
        }
    }
}

// ============================================================================
// Win32DirectoryProvider
// ============================================================================

Win32DirectoryProvider::Win32DirectoryProvider() = default;

bool Win32DirectoryProvider::canHandleUri(std::string_view uri) const noexcept {
    if (uri.starts_with("file://") || uri.starts_with("file:///")) {
        return true;
    }
    // Standard Windows drive paths: e.g. "C:\" or "C:/"
    if (uri.size() >= 2 && uri[1] == ':' &&
        ((uri[0] >= 'A' && uri[0] <= 'Z') || (uri[0] >= 'a' && uri[0] <= 'z'))) {
        return true;
    }
    // UNC paths: "\\server\share"
    if (uri.starts_with("\\\\") || uri.starts_with("//")) {
        return true;
    }
    return false;
}

std::wstring Win32DirectoryProvider::uriToNativePath(std::string_view uri) {
    std::string pathStr(uri);
    if (pathStr.starts_with("file:///")) {
        pathStr = pathStr.substr(8);
    } else if (pathStr.starts_with("file://")) {
        pathStr = pathStr.substr(7);
    }

    // Convert forward slashes to backslashes
    for (char& c : pathStr) {
        if (c == '/') {
            c = '\\';
        }
    }

    // Strip leading slash before drive letter if present (e.g. "\C:\...")
    if (pathStr.size() >= 3 && pathStr[0] == '\\' && pathStr[2] == ':') {
        pathStr = pathStr.substr(1);
    }

    std::wstring native = utf8_to_utf16(pathStr);

    // If path is long and doesn't already have \\?\ prefix, add it
    if (native.size() >= MAX_PATH && !native.starts_with(L"\\\\?\\")) {
        native = L"\\\\?\\" + native;
    }

    return native;
}

uint64_t Win32DirectoryProvider::getVolumeSerial(const std::wstring& nativePath) {
    std::wstring root;
    if (nativePath.starts_with(L"\\\\?\\")) {
        size_t slash = nativePath.find(L'\\', 4);
        if (slash != std::wstring::npos) {
            root = nativePath.substr(0, slash + 1);
        } else {
            root = nativePath + L"\\";
        }
    } else if (nativePath.size() >= 2 && nativePath[1] == L':') {
        root = nativePath.substr(0, 2) + L"\\";
    } else {
        root = L"C:\\";
    }

    DWORD serial = 0;
    if (::GetVolumeInformationW(root.c_str(), nullptr, 0, &serial, nullptr, nullptr, nullptr, 0)) {
        return static_cast<uint64_t>(serial);
    }
    return 1; // Fallback serial
}

ProviderStatus Win32DirectoryProvider::enumerate(
    std::string_view uri,
    uint64_t requestId,
    uint64_t generation,
    std::shared_ptr<CancellationToken> cancelToken,
    std::function<bool(DirectoryBatch&&)> batchCallback)
{
    if (cancelToken && cancelToken->isCancelled()) {
        return ProviderStatus::Cancelled;
    }

    std::wstring nativePath = uriToNativePath(uri);
    // Remove trailing backslash for search pattern unless it's root
    while (nativePath.size() > 3 && nativePath.back() == L'\\') {
        nativePath.pop_back();
    }

    std::wstring searchPattern = nativePath + L"\\*";

    WIN32_FIND_DATAW wfd{};
    HANDLE hFind = ::FindFirstFileExW(
        searchPattern.c_str(),
        FindExInfoBasic,
        &wfd,
        FindExSearchNameMatch,
        nullptr,
        FIND_FIRST_EX_LARGE_FETCH
    );

    if (hFind == INVALID_HANDLE_VALUE) {
        DWORD err = ::GetLastError();
        ProviderStatus status = ProviderStatus::Error;
        std::string errStr;

        if (err == ERROR_FILE_NOT_FOUND || err == ERROR_PATH_NOT_FOUND) {
            status = ProviderStatus::NotFound;
            errStr = "The system cannot find the path specified.";
        } else if (err == ERROR_ACCESS_DENIED) {
            status = ProviderStatus::PermissionDenied;
            errStr = "Access is denied.";
        } else if (err == ERROR_NOT_READY || err == ERROR_DEVICE_NOT_CONNECTED) {
            status = ProviderStatus::DeviceNotReady;
            errStr = "The device is not ready.";
        } else {
            errStr = "Win32 FindFirstFileExW error code: " + std::to_string(err);
        }

        DirectoryBatch finalBatch;
        finalBatch.requestId = requestId;
        finalBatch.generation = generation;
        finalBatch.isFinal = true;
        finalBatch.status = status;
        finalBatch.errorMessage = std::move(errStr);
        batchCallback(std::move(finalBatch));
        return status;
    }

    struct FindCloser {
        HANDLE h;
        ~FindCloser() {
            if (h != INVALID_HANDLE_VALUE) {
                ::FindClose(h);
            }
        }
    } closer{hFind};

    const uint64_t volumeSerial = getVolumeSerial(nativePath);
    uint64_t totalCount = 0;

    DirectoryBatch currentBatch;
    currentBatch.requestId = requestId;
    currentBatch.generation = generation;
    currentBatch.isFinal = false;
    currentBatch.status = ProviderStatus::Loading;
    currentBatch.entries.reserve(kDirectoryBatchSize);

    do {
        if (cancelToken && cancelToken->isCancelled()) {
            return ProviderStatus::Cancelled;
        }

        // Skip "." and ".."
        if (wfd.cFileName[0] == L'.' &&
            (wfd.cFileName[1] == L'\0' || (wfd.cFileName[1] == L'.' && wfd.cFileName[2] == L'\0'))) {
            continue;
        }

        std::string u8Name = utf16_to_utf8(wfd.cFileName);
        const uint32_t nameOffset = static_cast<uint32_t>(currentBatch.nameArena.size());
        const uint32_t nameLen = static_cast<uint32_t>(u8Name.size());
        currentBatch.nameArena.append(u8Name);

        DirectoryEntry entry{};
        entry.name = NameSlice{nameOffset, nameLen};
        entry.size = (static_cast<uint64_t>(wfd.nFileSizeHigh) << 32) | wfd.nFileSizeLow;
        entry.mtime = (static_cast<uint64_t>(wfd.ftLastWriteTime.dwHighDateTime) << 32) | wfd.ftLastWriteTime.dwLowDateTime;
        entry.attributes = wfd.dwFileAttributes;
        entry.sizeState = MetaState::Known;
        entry.timeState = MetaState::Known;

        if (wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            entry.flags |= kEntryDir;
        }
        if (wfd.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) {
            entry.flags |= kEntryHidden;
        }
        if (wfd.dwFileAttributes & FILE_ATTRIBUTE_SYSTEM) {
            entry.flags |= kEntrySystem;
        }
        if (wfd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) {
            entry.flags |= kEntryReparse;
        }
        if (wfd.dwFileAttributes & FILE_ATTRIBUTE_OFFLINE) {
            entry.flags |= kEntryOffline;
        }

        // Generate stable EntryKey using volume serial and hash of filename + directory flag
        uint64_t nameHash = fnv1a64(u8Name, volumeSerial);
        entry.key = EntryKey{
            .provider = ProviderKind::Win32Directory,
            .volumeOrProviderId = volumeSerial,
            .objectId = nameHash,
            .incarnation = 0
        };

        currentBatch.entries.push_back(entry);
        totalCount++;

        if (currentBatch.entries.size() >= kDirectoryBatchSize) {
            bool keepGoing = batchCallback(std::move(currentBatch));
            if (!keepGoing) {
                return ProviderStatus::Cancelled;
            }
            currentBatch = DirectoryBatch{};
            currentBatch.requestId = requestId;
            currentBatch.generation = generation;
            currentBatch.isFinal = false;
            currentBatch.status = ProviderStatus::Loading;
            currentBatch.entries.reserve(kDirectoryBatchSize);
        }

    } while (::FindNextFileW(hFind, &wfd));

    // Emit final batch
    currentBatch.isFinal = true;
    currentBatch.status = (totalCount > 0) ? ProviderStatus::Ready : ProviderStatus::Empty;
    batchCallback(std::move(currentBatch));

    return currentBatch.status;
}

std::unique_ptr<IDirectoryWatcher> Win32DirectoryProvider::watch(
    std::string_view uri,
    std::shared_ptr<IWatchListener> listener)
{
    std::wstring native = uriToNativePath(uri);
    return std::make_unique<Win32DirectoryWatcher>(std::move(native), std::move(listener));
}

} // namespace gx::platform::win32

#endif // _WIN32
