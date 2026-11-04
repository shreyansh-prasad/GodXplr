#pragma once

#include <string>
#include <cstdint>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace gx::providers {

// Constants for Cloud Files API attributes if not in SDK headers
#ifndef FILE_ATTRIBUTE_RECALL_ON_DATA_ACCESS
#define FILE_ATTRIBUTE_RECALL_ON_DATA_ACCESS 0x00400000
#endif

#ifndef FILE_ATTRIBUTE_RECALL_ON_OPEN
#define FILE_ATTRIBUTE_RECALL_ON_OPEN 0x00040000
#endif

enum class CloudSyncState : uint8_t {
    NotCloudFile,
    LocallyAvailable,
    DehydratedPlaceholder,
    Syncing,
    SyncError
};

class CloudStorageProvider {
public:
    // Identifies if file attributes indicate a dehydrated cloud placeholder
    static bool isDehydratedPlaceholder(DWORD fileAttributes);

    // Queries full cloud status for a file path
    static CloudSyncState queryFileCloudState(const std::wstring& filePath);
};

} // namespace gx::providers

