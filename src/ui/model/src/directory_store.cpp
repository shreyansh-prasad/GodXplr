#include "godxplr/ui/model/directory_store.h"
#include <algorithm>
#include <stdexcept>

namespace gx::ui {

namespace {

// Natural numeric string comparison (e.g. "file2" < "file10")
int naturalCompare(std::string_view a, std::string_view b) {
    size_t ia = 0, ib = 0;
    while (ia < a.size() && ib < b.size()) {
        char ca = a[ia];
        char cb = b[ib];

        if (ca >= '0' && ca <= '9' && cb >= '0' && cb <= '9') {
            // Extract numeric sequences
            uint64_t numA = 0;
            while (ia < a.size() && a[ia] >= '0' && a[ia] <= '9') {
                numA = numA * 10 + (a[ia] - '0');
                ia++;
            }
            uint64_t numB = 0;
            while (ib < b.size() && b[ib] >= '0' && b[ib] <= '9') {
                numB = numB * 10 + (b[ib] - '0');
                ib++;
            }
            if (numA != numB) {
                return (numA < numB) ? -1 : 1;
            }
        } else {
            // Case-insensitive ASCII character comparison
            char la = (ca >= 'A' && ca <= 'Z') ? static_cast<char>(ca + 32) : ca;
            char lb = (cb >= 'A' && cb <= 'Z') ? static_cast<char>(cb + 32) : cb;
            if (la != lb) {
                return (la < lb) ? -1 : 1;
            }
            ia++;
            ib++;
        }
    }

    if (ia < a.size()) return 1;
    if (ib < b.size()) return -1;
    return 0;
}

} // namespace

DirectoryStore::DirectoryStore() {
    reserve(1024, 32768);
}

void DirectoryStore::reserve(size_t entryCapacity, size_t arenaCapacity) {
    entries_.reserve(entryCapacity);
    permutation_.reserve(entryCapacity);
    keyToIndex_.reserve(entryCapacity);
    if (arenaCapacity == 0) {
        arenaCapacity = entryCapacity * 32;
    }
    nameArena_.reserve(arenaCapacity);
}

uint32_t DirectoryStore::addEntry(const EntryKey& key,
                                  std::string_view name,
                                  uint64_t size,
                                  uint64_t mtime,
                                  uint32_t attributes,
                                  uint32_t flags,
                                  MetaState sizeState,
                                  MetaState timeState,
                                  uint32_t iconKey) {
    uint32_t idx = static_cast<uint32_t>(entries_.size());

    uint32_t nameOff = static_cast<uint32_t>(nameArena_.size());
    uint32_t nameLen = static_cast<uint32_t>(name.size());

    nameArena_.insert(nameArena_.end(), name.begin(), name.end());
    nameArena_.push_back('\0');

    DirectoryEntry entry{};
    entry.key = key;
    entry.name = NameSlice{nameOff, nameLen};
    entry.size = size;
    entry.mtime = mtime;
    entry.attributes = attributes;
    entry.flags = flags;
    entry.sizeState = sizeState;
    entry.timeState = timeState;
    entry.iconKey = iconKey;

    entries_.push_back(entry);
    permutation_.push_back(idx);
    keyToIndex_[key] = idx;

    return idx;
}

void DirectoryStore::clear() {
    entries_.clear();
    permutation_.clear();
    nameArena_.clear();
    keyToIndex_.clear();
    generation_++;
}

const DirectoryEntry& DirectoryStore::getEntry(uint64_t visibleRow) const {
    if (visibleRow >= permutation_.size()) {
        throw std::out_of_range("Visible row index out of range");
    }
    uint32_t rawIndex = permutation_[visibleRow];
    return entries_[rawIndex];
}

std::string_view DirectoryStore::getName(uint64_t visibleRow) const {
    if (visibleRow >= permutation_.size()) {
        return "";
    }
    uint32_t rawIndex = permutation_[visibleRow];
    return getNameByIndex(rawIndex);
}

const DirectoryEntry& DirectoryStore::getEntryByIndex(uint32_t rawIndex) const {
    if (rawIndex >= entries_.size()) {
        throw std::out_of_range("Raw entry index out of range");
    }
    return entries_[rawIndex];
}

std::string_view DirectoryStore::getNameByIndex(uint32_t rawIndex) const {
    if (rawIndex >= entries_.size()) {
        return "";
    }
    const auto& slice = entries_[rawIndex].name;
    return std::string_view(nameArena_.data() + slice.offset, slice.length);
}

uint32_t DirectoryStore::findIndexByKey(const EntryKey& key) const {
    auto it = keyToIndex_.find(key);
    return (it != keyToIndex_.end()) ? it->second : UINT32_MAX;
}

uint64_t DirectoryStore::findVisibleRowByKey(const EntryKey& key) const {
    uint32_t rawIdx = findIndexByKey(key);
    if (rawIdx == UINT32_MAX) {
        return UINT64_MAX;
    }
    for (uint64_t row = 0; row < permutation_.size(); ++row) {
        if (permutation_[row] == rawIdx) {
            return row;
        }
    }
    return UINT64_MAX;
}

void DirectoryStore::sort(const SortSpec& spec) {
    if (permutation_.empty()) {
        return;
    }

    std::sort(permutation_.begin(), permutation_.end(), [&](uint32_t ia, uint32_t ib) {
        const auto& ea = entries_[ia];
        const auto& eb = entries_[ib];

        // 1. Group directories first if requested
        if (spec.directoriesFirst) {
            bool dirA = ea.isDirectory();
            bool dirB = eb.isDirectory();
            if (dirA != dirB) {
                return dirA; // Directories precede files
            }
        }

        int cmp = 0;
        switch (spec.column) {
            case SortColumn::Name: {
                std::string_view na = getNameByIndex(ia);
                std::string_view nb = getNameByIndex(ib);
                cmp = spec.naturalNumeric ? naturalCompare(na, nb) : na.compare(nb);
                break;
            }
            case SortColumn::Size: {
                if (ea.size != eb.size) {
                    cmp = (ea.size < eb.size) ? -1 : 1;
                }
                break;
            }
            case SortColumn::ModifiedTime: {
                if (ea.mtime != eb.mtime) {
                    cmp = (ea.mtime < eb.mtime) ? -1 : 1;
                }
                break;
            }
            case SortColumn::Attributes: {
                if (ea.attributes != eb.attributes) {
                    cmp = (ea.attributes < eb.attributes) ? -1 : 1;
                }
                break;
            }
            default:
                break;
        }

        // Secondary tie-breaker by name
        if (cmp == 0) {
            cmp = naturalCompare(getNameByIndex(ia), getNameByIndex(ib));
        }

        return (spec.order == SortOrder::Ascending) ? (cmp < 0) : (cmp > 0);
    });
}

size_t DirectoryStore::totalMemoryBytes() const noexcept {
    size_t mem = sizeof(DirectoryStore);
    mem += entries_.capacity() * sizeof(DirectoryEntry);
    mem += permutation_.capacity() * sizeof(uint32_t);
    mem += nameArena_.capacity() * sizeof(char);
    // Rough estimate for unordered_map (buckets + nodes)
    mem += keyToIndex_.bucket_count() * sizeof(void*);
    mem += keyToIndex_.size() * (sizeof(EntryKey) + sizeof(uint32_t) + sizeof(void*));
    return mem;
}

} // namespace gx::ui
