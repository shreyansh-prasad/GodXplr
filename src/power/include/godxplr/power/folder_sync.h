#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace gx::power {

enum class SyncItemState : uint8_t {
    Identical,
    Different,
    LeftOnly,
    RightOnly
};

enum class SyncActionType : uint8_t {
    CopyLeftToRight,
    CopyRightToLeft,
    DeleteLeft,
    DeleteRight,
    Skip
};

enum class SyncDirection : uint8_t {
    LeftToRight,
    RightToLeft,
    TwoWay
};

struct SyncFileItem {
    std::wstring relativePath;
    uint64_t leftSize{0};
    uint64_t rightSize{0};
    uint64_t leftMtime{0};
    uint64_t rightMtime{0};
    SyncItemState state{SyncItemState::Identical};
};

struct SyncActionItem {
    std::wstring relativePath;
    std::wstring sourceFullPath;
    std::wstring destFullPath;
    SyncActionType actionType{SyncActionType::Skip};
    uint64_t transferBytes{0};
};

struct SyncPlan {
    std::wstring leftRoot;
    std::wstring rightRoot;
    std::vector<SyncFileItem> items;
    std::vector<SyncActionItem> plannedActions;
    uint64_t totalBytesToTransfer{0};
};

class FolderSync {
public:
    FolderSync() = default;
    ~FolderSync() = default;

    // Scans left and right folders and compares contents
    static SyncPlan compareFolders(
        const std::wstring& leftRoot,
        const std::wstring& rightRoot,
        SyncDirection direction = SyncDirection::LeftToRight);
};

} // namespace gx::power
