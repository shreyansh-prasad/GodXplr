#include "godxplr/ui/model/tree_model.h"

namespace gx::ui {

FolderTreeModel::FolderTreeModel() = default;

uint64_t FolderTreeModel::addRootNode(const EntryKey& key, std::string uri, std::string displayName) {
    uint64_t id = nextNodeId_++;
    TreeNode node;
    node.id = id;
    node.parentId = 0;
    node.key = key;
    node.uri = std::move(uri);
    node.displayName = std::move(displayName);
    node.depth = 0;
    node.childState = TreeChildState::Unloaded;
    node.isExpanded = false;

    nodes_[id] = std::move(node);
    rootIds_.push_back(id);
    return id;
}

bool FolderTreeModel::expandNode(uint64_t nodeId, IDirectoryProvider* provider) {
    auto it = nodes_.find(nodeId);
    if (it == nodes_.end()) {
        return false;
    }

    TreeNode& parent = it->second;

    if (parent.childState == TreeChildState::Unloaded && provider != nullptr) {
        parent.childState = TreeChildState::Loading;
        std::vector<std::pair<EntryKey, std::string>> subfolders;

        auto cancelToken = std::make_shared<CancellationToken>();
        provider->enumerate(
            parent.uri,
            0,
            0,
            cancelToken,
            [&subfolders](DirectoryBatch&& batch) -> bool {
                for (const auto& entry : batch.entries) {
                    if (entry.isDirectory()) {
                        std::string_view name = batch.getName(entry);
                        subfolders.emplace_back(entry.key, std::string(name));
                    }
                }
                return true;
            }
        );

        populateChildren(nodeId, std::move(subfolders));
    }

    parent.isExpanded = true;
    return true;
}

void FolderTreeModel::collapseNode(uint64_t nodeId) {
    auto it = nodes_.find(nodeId);
    if (it != nodes_.end()) {
        it->second.isExpanded = false;
    }
}

void FolderTreeModel::toggleNode(uint64_t nodeId, IDirectoryProvider* provider) {
    auto it = nodes_.find(nodeId);
    if (it != nodes_.end()) {
        if (it->second.isExpanded) {
            collapseNode(nodeId);
        } else {
            expandNode(nodeId, provider);
        }
    }
}

void FolderTreeModel::populateChildren(uint64_t parentId, std::vector<std::pair<EntryKey, std::string>> children) {
    auto it = nodes_.find(parentId);
    if (it == nodes_.end()) {
        return;
    }

    TreeNode& parent = it->second;
    parent.childIds.clear();

    if (children.empty()) {
        parent.childState = TreeChildState::Empty;
        return;
    }

    parent.childIds.reserve(children.size());
    for (auto& [key, name] : children) {
        uint64_t childId = nextNodeId_++;
        TreeNode child;
        child.id = childId;
        child.parentId = parentId;
        child.key = key;
        child.uri = parent.uri + "/" + name;
        child.displayName = std::move(name);
        child.depth = parent.depth + 1;
        child.childState = TreeChildState::Unloaded;
        child.isExpanded = false;

        nodes_[childId] = std::move(child);
        parent.childIds.push_back(childId);
    }

    parent.childState = TreeChildState::Loaded;
}

const TreeNode* FolderTreeModel::getNode(uint64_t nodeId) const noexcept {
    auto it = nodes_.find(nodeId);
    return (it != nodes_.end()) ? &it->second : nullptr;
}

std::vector<uint64_t> FolderTreeModel::visibleRows() const {
    std::vector<uint64_t> rows;
    rows.reserve(nodes_.size());

    for (uint64_t rootId : rootIds_) {
        collectVisible(rootId, rows);
    }

    return rows;
}

void FolderTreeModel::collectVisible(uint64_t nodeId, std::vector<uint64_t>& out) const {
    auto it = nodes_.find(nodeId);
    if (it == nodes_.end()) {
        return;
    }

    const TreeNode& node = it->second;
    out.push_back(nodeId);

    if (node.isExpanded) {
        for (uint64_t childId : node.childIds) {
            collectVisible(childId, out);
        }
    }
}

void FolderTreeModel::clear() {
    nodes_.clear();
    rootIds_.clear();
    nextNodeId_ = 1;
}

} // namespace gx::ui
