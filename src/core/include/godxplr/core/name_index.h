#pragma once

#include "godxplr/core/search_types.h"
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <span>

namespace gx {

struct OffsetIdPair {
    uint32_t offset{0};
    EntryId id{kNoEntry};

    auto operator<=>(const OffsetIdPair&) const = default;
};

class NameIndex {
public:
    NameIndex();
    ~NameIndex() = default;

    // Disallow accidental heavy copies; enable move semantics
    NameIndex(const NameIndex&) = delete;
    NameIndex& operator=(const NameIndex&) = delete;
    NameIndex(NameIndex&&) noexcept = default;
    NameIndex& operator=(NameIndex&&) noexcept = default;

    // Resizes and preallocates capacity for N entries
    void reserve(size_t capacity, size_t arenaCapacity = 0);

    // Adds a file or directory entry to the index
    EntryId add(uint64_t frn, uint64_t parentFrn, std::string_view name,
                uint8_t flags, uint64_t size = 0, uint64_t mtime = 0);

    // Removes an entry by ID (marks tombstone, poisons arena, recycles slot)
    void remove(EntryId id);

    // Renames an existing entry
    void rename(EntryId id, std::string_view newName);

    // Moves an entry to a new parent
    void move(EntryId id, EntryId newParentId);

    // Resolves entry ID by FRN in O(1)
    [[nodiscard]] EntryId findByFrn(uint64_t frn) const;

    // Resolves hit offset in arena to EntryId in O(log N)
    [[nodiscard]] EntryId findByOffset(uint32_t offset) const;

    // Reconstructs the full path by traversing parent pointers
    [[nodiscard]] std::string buildPath(EntryId id) const;

    // Accessors
    [[nodiscard]] size_t size() const noexcept { return entryCount_; }
    [[nodiscard]] size_t capacity() const noexcept { return nameOff_.size(); }
    [[nodiscard]] std::string_view getName(EntryId id) const;
    [[nodiscard]] std::string_view getFoldedName(EntryId id) const;
    [[nodiscard]] EntryId getParent(EntryId id) const { return parent_[id]; }
    [[nodiscard]] uint64_t getFrn(EntryId id) const { return frn_[id]; }
    [[nodiscard]] uint64_t getFileSize(EntryId id) const { return size_[id]; }
    [[nodiscard]] uint64_t getMtime(EntryId id) const { return mtime_[id]; }
    [[nodiscard]] uint8_t getFlags(EntryId id) const { return flags_[id]; }
    [[nodiscard]] uint32_t getLetters(EntryId id) const { return letters_[id]; }
    [[nodiscard]] bool isDeleted(EntryId id) const { return (flags_[id] & kDeleted) != 0; }
    [[nodiscard]] bool isDirectory(EntryId id) const { return (flags_[id] & kDir) != 0; }

    // Direct arena spans for SIMD scanning
    [[nodiscard]] std::span<const char> getOriginalArena() const noexcept { return arenaOrig_; }
    [[nodiscard]] std::span<const char> getFoldedArena() const noexcept { return arenaFold_; }

    // Computes total resident memory footprint in bytes
    [[nodiscard]] size_t totalMemoryBytes() const noexcept;

    // Helper: Folds a UTF-8 string in-place (length-preserving)
    static std::string foldCase(std::string_view input);

    // Helper: Computes letter presence bitmask (a-z: 0..25, 0-9: 26..35)
    static uint32_t computeLetterMask(std::string_view folded);

private:
    size_t entryCount_{0};
    size_t activeCount_{0};

    // Parallel Structure of Arrays (SoA) Columns
    std::vector<uint32_t> nameOff_;
    std::vector<uint16_t> nameLen_;
    std::vector<EntryId> parent_;
    std::vector<uint64_t> frn_;
    std::vector<uint64_t> size_;
    std::vector<uint64_t> mtime_;
    std::vector<uint8_t> flags_;
    std::vector<uint32_t> letters_;

    // Monotonic Arenas (NUL-delimited strings)
    std::vector<char> arenaOrig_;
    std::vector<char> arenaFold_;

    // Offset-to-ID Reverse Map (Sorted by offset for binary search)
    std::vector<OffsetIdPair> offsetMap_;

    // Fast O(1) FRN to EntryId map
    std::unordered_map<uint64_t, EntryId> frnToIdMap_;

    // Free list for recycled entry IDs
    std::vector<EntryId> freeList_;
};

} // namespace gx
