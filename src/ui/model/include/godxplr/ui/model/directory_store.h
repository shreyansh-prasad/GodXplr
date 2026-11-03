#pragma once

#include "godxplr/ui/model/directory_entry.h"
#include "godxplr/ui/state/tab_state.h"
#include <vector>
#include <unordered_map>
#include <string_view>
#include <string>
#include <cstdint>
#include <memory>

namespace gx::ui {

class DirectoryStore {
public:
    DirectoryStore();
    ~DirectoryStore() = default;

    // Disallow heavy accidental copies; enable move
    DirectoryStore(const DirectoryStore&) = delete;
    DirectoryStore& operator=(const DirectoryStore&) = delete;
    DirectoryStore(DirectoryStore&&) noexcept = default;
    DirectoryStore& operator=(DirectoryStore&&) noexcept = default;

    // Preallocates memory for N entries and arena
    void reserve(size_t entryCapacity, size_t arenaCapacity = 0);

    // Appends entry into the store and updates identity mapping
    uint32_t addEntry(const EntryKey& key,
                      std::string_view name,
                      uint64_t size = 0,
                      uint64_t mtime = 0,
                      uint32_t attributes = 0,
                      uint32_t flags = 0,
                      MetaState sizeState = MetaState::Known,
                      MetaState timeState = MetaState::Known,
                      uint32_t iconKey = 0);

    // Clears all entries and resets arenas
    void clear();

    // Accessors
    [[nodiscard]] size_t size() const noexcept { return permutation_.size(); }
    [[nodiscard]] bool empty() const noexcept { return permutation_.empty(); }

    [[nodiscard]] const DirectoryEntry& getEntry(uint64_t visibleRow) const;
    [[nodiscard]] std::string_view getName(uint64_t visibleRow) const;

    [[nodiscard]] const DirectoryEntry& getEntryByIndex(uint32_t rawIndex) const;
    [[nodiscard]] std::string_view getNameByIndex(uint32_t rawIndex) const;

    // Stable identity lookups
    [[nodiscard]] uint32_t findIndexByKey(const EntryKey& key) const;
    [[nodiscard]] uint64_t findVisibleRowByKey(const EntryKey& key) const;

    // Generation / Cache control
    [[nodiscard]] uint64_t generation() const noexcept { return generation_; }
    void setGeneration(uint64_t gen) noexcept { generation_ = gen; }

    // In-place sorting of row order permutation
    void sort(const SortSpec& spec);

    // Memory footprint calculation in bytes
    [[nodiscard]] size_t totalMemoryBytes() const noexcept;

private:
    std::vector<char> nameArena_;
    std::vector<DirectoryEntry> entries_;
    std::vector<uint32_t> permutation_;
    std::unordered_map<EntryKey, uint32_t> keyToIndex_;
    uint64_t generation_{0};
};

} // namespace gx::ui
