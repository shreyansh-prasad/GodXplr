#pragma once

#ifdef _WIN32

#include "godxplr/core/name_index.h"
#include "godxplr/platform/win32_common.h"
#include <cstdint>
#include <string>
#include <functional>

namespace gx::platform::win32 {

struct UsnJournalCursor {
    uint64_t journalId{0};
    uint64_t nextUsn{0};
    uint64_t lowestValidUsn{0};
};

struct UsnEnumerationStats {
    size_t filesCount{0};
    size_t directoriesCount{0};
    size_t rawRecordBytes{0};
    double elapsedMs{0.0};
};

enum class UsnStatus : uint8_t {
    Success,
    AccessDenied,       // Administrative privileges required for raw volume handle
    NotNtfsVolume,      // Volume is FAT, exFAT, ReFS, etc.
    JournalNotActive,   // USN journal absent or disabled
    InvalidHandle,
    IoError
};

class UsnEnumerator {
public:
    explicit UsnEnumerator(wchar_t volumeLetter = L'C');
    ~UsnEnumerator() = default;

    // Checks whether the caller process has administrator / elevation tokens
    [[nodiscard]] static bool isElevated();

    // Queries current USN journal cursor
    UsnStatus queryCursor(UsnJournalCursor& outCursor);

    // Enumerates all MFT records on the volume directly into NameIndex
    // Populates outCursor with the journal state captured BEFORE enumeration
    UsnStatus enumerateVolume(NameIndex& outIndex,
                              UsnJournalCursor& outCursor,
                              UsnEnumerationStats& outStats,
                              const std::function<void(size_t entriesSoFar)>& progressCallback = nullptr);

    [[nodiscard]] wchar_t getVolumeLetter() const noexcept { return volumeLetter_; }

private:
    wchar_t volumeLetter_{L'C'};
};

} // namespace gx::platform::win32

#endif // _WIN32
