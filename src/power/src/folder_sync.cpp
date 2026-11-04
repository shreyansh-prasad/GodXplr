#include "godxplr/power/folder_sync.h"
#include <filesystem>
#include <unordered_map>
#include <unordered_set>

namespace gx::power {

struct LocalFileInfo {
    uint64_t size{0};
    uint64_t mtime{0};
};

static void scanDirectory(
    const std::filesystem::path& root,
    std::unordered_map<std::wstring, LocalFileInfo>& outMap)
{
    if (!std::filesystem::exists(root) || !std::filesystem::is_directory(root)) {
        return;
    }

    std::error_code ec;
    for (auto it = std::filesystem::recursive_directory_iterator(root, ec);
         it != std::filesystem::recursive_directory_iterator();
         it.increment(ec))
    {
        if (ec) continue;
        if (it->is_regular_file(ec)) {
            auto rel = std::filesystem::relative(it->path(), root, ec).wstring();
            uint64_t size = static_cast<uint64_t>(it->file_size(ec));
            auto ftime = it->last_write_time(ec);
            uint64_t mtime = static_cast<uint64_t>(ftime.time_since_epoch().count());
            outMap[rel] = LocalFileInfo{ size, mtime };
        }
    }
}

SyncPlan FolderSync::compareFolders(
    const std::wstring& leftRoot,
    const std::wstring& rightRoot,
    SyncDirection direction)
{
    SyncPlan plan;
    plan.leftRoot = leftRoot;
    plan.rightRoot = rightRoot;

    std::unordered_map<std::wstring, LocalFileInfo> leftFiles;
    std::unordered_map<std::wstring, LocalFileInfo> rightFiles;

    scanDirectory(leftRoot, leftFiles);
    scanDirectory(rightRoot, rightFiles);

    std::unordered_set<std::wstring> allRelPaths;
    for (const auto& [rel, _] : leftFiles) allRelPaths.insert(rel);
    for (const auto& [rel, _] : rightFiles) allRelPaths.insert(rel);

    std::filesystem::path leftPath(leftRoot);
    std::filesystem::path rightPath(rightRoot);

    for (const auto& rel : allRelPaths) {
        auto itLeft = leftFiles.find(rel);
        auto itRight = rightFiles.find(rel);

        SyncFileItem item;
        item.relativePath = rel;

        if (itLeft != leftFiles.end() && itRight != rightFiles.end()) {
            item.leftSize = itLeft->second.size;
            item.leftMtime = itLeft->second.mtime;
            item.rightSize = itRight->second.size;
            item.rightMtime = itRight->second.mtime;

            if (item.leftSize == item.rightSize && item.leftMtime == item.rightMtime) {
                item.state = SyncItemState::Identical;
            } else {
                item.state = SyncItemState::Different;
            }
        } else if (itLeft != leftFiles.end()) {
            item.leftSize = itLeft->second.size;
            item.leftMtime = itLeft->second.mtime;
            item.state = SyncItemState::LeftOnly;
        } else {
            item.rightSize = itRight->second.size;
            item.rightMtime = itRight->second.mtime;
            item.state = SyncItemState::RightOnly;
        }

        // Action calculation
        SyncActionItem act;
        act.relativePath = rel;
        act.sourceFullPath = (leftPath / rel).wstring();
        act.destFullPath = (rightPath / rel).wstring();

        if (direction == SyncDirection::LeftToRight) {
            if (item.state == SyncItemState::LeftOnly || item.state == SyncItemState::Different) {
                act.actionType = SyncActionType::CopyLeftToRight;
                act.transferBytes = item.leftSize;
                plan.totalBytesToTransfer += item.leftSize;
                plan.plannedActions.push_back(std::move(act));
            }
        } else if (direction == SyncDirection::RightToLeft) {
            if (item.state == SyncItemState::RightOnly || item.state == SyncItemState::Different) {
                act.actionType = SyncActionType::CopyRightToLeft;
                act.sourceFullPath = (rightPath / rel).wstring();
                act.destFullPath = (leftPath / rel).wstring();
                act.transferBytes = item.rightSize;
                plan.totalBytesToTransfer += item.rightSize;
                plan.plannedActions.push_back(std::move(act));
            }
        } else if (direction == SyncDirection::TwoWay) {
            if (item.state == SyncItemState::LeftOnly) {
                act.actionType = SyncActionType::CopyLeftToRight;
                act.transferBytes = item.leftSize;
                plan.totalBytesToTransfer += item.leftSize;
                plan.plannedActions.push_back(std::move(act));
            } else if (item.state == SyncItemState::RightOnly) {
                act.actionType = SyncActionType::CopyRightToLeft;
                act.sourceFullPath = (rightPath / rel).wstring();
                act.destFullPath = (leftPath / rel).wstring();
                act.transferBytes = item.rightSize;
                plan.totalBytesToTransfer += item.rightSize;
                plan.plannedActions.push_back(std::move(act));
            } else if (item.state == SyncItemState::Different) {
                // Copy newer over older
                if (item.leftMtime >= item.rightMtime) {
                    act.actionType = SyncActionType::CopyLeftToRight;
                    act.transferBytes = item.leftSize;
                    plan.totalBytesToTransfer += item.leftSize;
                } else {
                    act.actionType = SyncActionType::CopyRightToLeft;
                    act.sourceFullPath = (rightPath / rel).wstring();
                    act.destFullPath = (leftPath / rel).wstring();
                    act.transferBytes = item.rightSize;
                    plan.totalBytesToTransfer += item.rightSize;
                }
                plan.plannedActions.push_back(std::move(act));
            }
        }

        plan.items.push_back(std::move(item));
    }

    return plan;
}

} // namespace gx::power

