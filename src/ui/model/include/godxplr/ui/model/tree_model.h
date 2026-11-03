#pragma once

#include "godxplr/core/entry_key.h"
#include "godxplr/core/provider_contract.h"
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <functional>

namespace gx::ui {

enum class TreeChildState : uint8_t {
    Unloaded,
    Loading,
    Loaded,
    Empty,
    Failed
};

struct TreeNode {
    uint64_t id{0};
    uint64_t parentId{0};
    EntryKey key{};
    std::string uri;
    std::string displayName;
    uint32_t depth{0};
    TreeChildState childState{TreeChildState::Unloaded};
    bool isExpanded{false};
    std::vector<uint64_t> childIds;

    [[nodiscard]] bool hasChildren() const noexcept {
        return childState != TreeChildState::Empty && (!childIds.empty() || childState == TreeChildState::Unloaded);
    }
};

class FolderTreeModel {
public:
    FolderTreeModel();
    ~FolderTreeModel() = default;

    FolderTreeModel(const FolderTreeModel&) = delete;
    FolderTreeModel& operator=(const FolderTreeModel&) = delete;
    FolderTreeModel(FolderTreeModel&&) noexcept = default;
    FolderTreeModel& operator=(FolderTreeModel&&) noexcept = default;

    // Adds a top-level root node (e.g. drive or Quick Access folder)
    uint64_t addRootNode(const EntryKey& key, std::string uri, std::string displayName);

    // Expands a node lazily. If children are not loaded, enumerates subfolders via provider.
    bool expandNode(uint64_t nodeId, IDirectoryProvider* provider = nullptr);

    // Collapses an expanded node. Descendants remain in memory but are hidden from visible rows.
    void collapseNode(uint64_t nodeId);

    // Toggles expansion state
    void toggleNode(uint64_t nodeId, IDirectoryProvider* provider = nullptr);

    // Sets child nodes directly (e.g. from background batch or test fixture)
    void populateChildren(uint64_t parentId, std::vector<std::pair<EntryKey, std::string>> children);

    // Lookups
    [[nodiscard]] const TreeNode* getNode(uint64_t nodeId) const noexcept;
    [[nodiscard]] const std::vector<uint64_t>& rootNodeIds() const noexcept { return rootIds_; }

    // Computes the flattened list of visible node IDs based on active expansion states
    [[nodiscard]] std::vector<uint64_t> visibleRows() const;

    [[nodiscard]] size_t totalNodeCount() const noexcept { return nodes_.size(); }
    void clear();

private:
    void collectVisible(uint64_t nodeId, std::vector<uint64_t>& out) const;

    uint64_t nextNodeId_{1};
    std::vector<uint64_t> rootIds_;
    std::unordered_map<uint64_t, TreeNode> nodes_;
};

} // namespace gx::ui
