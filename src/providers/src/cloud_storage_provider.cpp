#include "godxplr/providers/cloud_storage_provider.h"

namespace gx::providers {

bool CloudStorageProvider::isDehydratedPlaceholder(DWORD fileAttributes) {
    if (fileAttributes == INVALID_FILE_ATTRIBUTES) {
        return false;
    }

    const DWORD kDehydratedFlags = FILE_ATTRIBUTE_RECALL_ON_DATA_ACCESS |
                                   FILE_ATTRIBUTE_RECALL_ON_OPEN |
                                   FILE_ATTRIBUTE_OFFLINE;

    return (fileAttributes & kDehydratedFlags) != 0;
}

CloudSyncState CloudStorageProvider::queryFileCloudState(const std::wstring& filePath) {
    WIN32_FIND_DATAW ffd{};
    HANDLE hFind = FindFirstFileW(filePath.c_str(), &ffd);
    if (hFind == INVALID_HANDLE_VALUE) {
        return CloudSyncState::NotCloudFile;
    }
    FindClose(hFind);

    if (isDehydratedPlaceholder(ffd.dwFileAttributes)) {
        return CloudSyncState::DehydratedPlaceholder;
    }

    if (ffd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) {
        // Cloud Files Reparse Tags (OneDrive: 0x9000001A, 0x9000101A)
        DWORD tag = ffd.dwReserved0;
        if (tag == 0x9000001A || tag == 0x9000101A || tag == 0x9000201A) {
            return CloudSyncState::LocallyAvailable;
        }
    }

    return CloudSyncState::NotCloudFile;
}

} // namespace gx::providers

