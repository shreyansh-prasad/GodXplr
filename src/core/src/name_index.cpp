#include "godxplr/core/name_index.h"
#include <algorithm>
#include <cassert>
#include <cstring>

namespace gx {

NameIndex::NameIndex() {
    // Reserve minimal baseline capacity
    reserve(1024, 32768);
}

NameIndex::NameIndex(NameIndex&& other) noexcept {
    std::unique_lock<std::shared_mutex> lock(other.mutex_);
    entryCount_ = other.entryCount_;
    activeCount_ = other.activeCount_;
    nameOff_ = std::move(other.nameOff_);
    nameLen_ = std::move(other.nameLen_);
    parent_ = std::move(other.parent_);
    frn_ = std::move(other.frn_);
    size_ = std::move(other.size_);
    mtime_ = std::move(other.mtime_);
    flags_ = std::move(other.flags_);
    letters_ = std::move(other.letters_);
    arenaOrig_ = std::move(other.arenaOrig_);
    arenaFold_ = std::move(other.arenaFold_);
    offsetMap_ = std::move(other.offsetMap_);
    frnToIdMap_ = std::move(other.frnToIdMap_);
    freeList_ = std::move(other.freeList_);
    pendingParents_ = std::move(other.pendingParents_);
    other.entryCount_ = 0;
    other.activeCount_ = 0;
}

NameIndex& NameIndex::operator=(NameIndex&& other) noexcept {
    if (this != &other) {
        std::unique_lock<std::shared_mutex> lockThis(mutex_, std::defer_lock);
        std::unique_lock<std::shared_mutex> lockOther(other.mutex_, std::defer_lock);
        std::lock(lockThis, lockOther);

        entryCount_ = other.entryCount_;
        activeCount_ = other.activeCount_;
        nameOff_ = std::move(other.nameOff_);
        nameLen_ = std::move(other.nameLen_);
        parent_ = std::move(other.parent_);
        frn_ = std::move(other.frn_);
        size_ = std::move(other.size_);
        mtime_ = std::move(other.mtime_);
        flags_ = std::move(other.flags_);
        letters_ = std::move(other.letters_);
        arenaOrig_ = std::move(other.arenaOrig_);
        arenaFold_ = std::move(other.arenaFold_);
        offsetMap_ = std::move(other.offsetMap_);
        frnToIdMap_ = std::move(other.frnToIdMap_);
        freeList_ = std::move(other.freeList_);
        pendingParents_ = std::move(other.pendingParents_);
        other.entryCount_ = 0;
        other.activeCount_ = 0;
    }
    return *this;
}

void NameIndex::reserve(size_t capacity, size_t arenaCapacity) {
    nameOff_.reserve(capacity);
    nameLen_.reserve(capacity);
    parent_.reserve(capacity);
    frn_.reserve(capacity);
    size_.reserve(capacity);
    mtime_.reserve(capacity);
    flags_.reserve(capacity);
    letters_.reserve(capacity);
    offsetMap_.reserve(capacity);
    frnToIdMap_.reserve(capacity);

    if (arenaCapacity == 0) {
        arenaCapacity = capacity * 32; // Average 32 bytes per name
    }
    arenaOrig_.reserve(arenaCapacity);
    arenaFold_.reserve(arenaCapacity);
}

std::string NameIndex::foldCase(std::string_view input) {
    std::string folded(input);
    for (char& c : folded) {
        if (c >= 'A' && c <= 'Z') {
            c = static_cast<char>(c + 32); // Simple ASCII lowercase
        }
    }
    return folded;
}

uint32_t NameIndex::computeLetterMask(std::string_view folded) {
    uint32_t mask = 0;
    for (char c : folded) {
        if (c >= 'a' && c <= 'z') {
            mask |= (1u << (c - 'a'));
        } else if (c >= '0' && c <= '9') {
            mask |= (1u << (26 + (c - '0')));
        }
    }
    return mask;
}

EntryId NameIndex::add(uint64_t frn, uint64_t parentFrn, std::string_view name,
                       uint8_t flags, uint64_t size, uint64_t mtime) {
    EntryId id = kNoEntry;
    if (!freeList_.empty()) {
        id = freeList_.back();
        freeList_.pop_back();
    } else {
        id = static_cast<EntryId>(nameOff_.size());
        nameOff_.push_back(0);
        nameLen_.push_back(0);
        parent_.push_back(kNoEntry);
        frn_.push_back(0);
        size_.push_back(0);
        mtime_.push_back(0);
        flags_.push_back(0);
        letters_.push_back(0);
    }

    uint32_t off = static_cast<uint32_t>(arenaOrig_.size());
    uint16_t len = static_cast<uint16_t>(name.size());

    // Append original name with NUL terminator
    arenaOrig_.insert(arenaOrig_.end(), name.begin(), name.end());
    arenaOrig_.push_back('\0');

    // Append folded name with NUL terminator
    std::string folded = foldCase(name);
    arenaFold_.insert(arenaFold_.end(), folded.begin(), folded.end());
    arenaFold_.push_back('\0');

    // Populate SoA columns
    nameOff_[id] = off;
    nameLen_[id] = len;
    frn_[id] = frn;
    size_[id] = size;
    mtime_[id] = mtime;
    flags_[id] = flags;
    letters_[id] = computeLetterMask(folded);

    // Resolve parent ID if parentFrn exists in map, or schedule for post-resolution
    if (parentFrn != 0) {
        auto it = frnToIdMap_.find(parentFrn);
        if (it != frnToIdMap_.end()) {
            parent_[id] = it->second;
        } else {
            parent_[id] = kNoEntry;
            pendingParents_.emplace_back(id, parentFrn);
        }
    } else {
        parent_[id] = kNoEntry;
    }

    // Record offset mapping for reverse lookup
    offsetMap_.push_back(OffsetIdPair{off, id});
    frnToIdMap_[frn] = id;

    entryCount_ = std::max(entryCount_, static_cast<size_t>(id + 1));
    activeCount_++;
    return id;
}

void NameIndex::remove(EntryId id) {
    if (id >= entryCount_ || isDeleted(id)) {
        return;
    }

    // Mark deleted
    flags_[id] |= kDeleted;

    // Poison arena slice with byte 0x01 so no search matches it
    uint32_t off = nameOff_[id];
    uint16_t len = nameLen_[id];
    if (off + len <= arenaFold_.size()) {
        std::memset(arenaFold_.data() + off, 0x01, len);
        std::memset(arenaOrig_.data() + off, 0x01, len);
    }

    // Remove from FRN lookup
    frnToIdMap_.erase(frn_[id]);

    freeList_.push_back(id);
    if (activeCount_ > 0) {
        activeCount_--;
    }
}

void NameIndex::rename(EntryId id, std::string_view newName) {
    if (id >= entryCount_ || isDeleted(id)) {
        return;
    }

    // Poison old arena slice
    uint32_t oldOff = nameOff_[id];
    uint16_t oldLen = nameLen_[id];
    if (oldOff + oldLen <= arenaFold_.size()) {
        std::memset(arenaFold_.data() + oldOff, 0x01, oldLen);
        std::memset(arenaOrig_.data() + oldOff, 0x01, oldLen);
    }

    // Append new name at tail of arena
    uint32_t newOff = static_cast<uint32_t>(arenaOrig_.size());
    uint16_t newLen = static_cast<uint16_t>(newName.size());

    arenaOrig_.insert(arenaOrig_.end(), newName.begin(), newName.end());
    arenaOrig_.push_back('\0');

    std::string folded = foldCase(newName);
    arenaFold_.insert(arenaFold_.end(), folded.begin(), folded.end());
    arenaFold_.push_back('\0');

    nameOff_[id] = newOff;
    nameLen_[id] = newLen;
    letters_[id] = computeLetterMask(folded);

    offsetMap_.push_back(OffsetIdPair{newOff, id});
}

void NameIndex::move(EntryId id, EntryId newParentId) {
    if (id >= entryCount_ || isDeleted(id)) {
        return;
    }
    parent_[id] = newParentId;
}

void NameIndex::moveByFrn(EntryId id, uint64_t newParentFrn) {
    if (id >= entryCount_ || isDeleted(id)) {
        return;
    }
    auto it = frnToIdMap_.find(newParentFrn);
    if (it != frnToIdMap_.end()) {
        parent_[id] = it->second;
    } else {
        parent_[id] = kNoEntry;
        pendingParents_.emplace_back(id, newParentFrn);
    }
}

void NameIndex::updateMetadata(EntryId id, uint64_t size, uint64_t mtime, uint8_t flags) {
    if (id >= entryCount_ || isDeleted(id)) {
        return;
    }
    if (size != 0) {
        size_[id] = size;
    }
    if (mtime != 0) {
        mtime_[id] = mtime;
    }
    uint8_t wasDeleted = flags_[id] & kDeleted;
    flags_[id] = (flags & ~kDeleted) | wasDeleted;
}

EntryId NameIndex::findByFrn(uint64_t frn) const {
    auto it = frnToIdMap_.find(frn);
    if (it != frnToIdMap_.end()) {
        EntryId id = it->second;
        if (!isDeleted(id)) {
            return id;
        }
    }
    return kNoEntry;
}

EntryId NameIndex::findByOffset(uint32_t offset) const {
    if (offsetMap_.empty()) {
        return kNoEntry;
    }

    // Binary search for offset in offsetMap_
    OffsetIdPair target{offset, 0};
    auto it = std::upper_bound(
        offsetMap_.begin(),
        offsetMap_.end(),
        target,
        [](const OffsetIdPair& a, const OffsetIdPair& b) {
            return a.offset < b.offset;
        }
    );

    if (it == offsetMap_.begin()) {
        return kNoEntry;
    }
    --it;

    EntryId id = it->id;
    if (id < entryCount_ && !isDeleted(id)) {
        // Verify hit offset falls strictly within [nameOff, nameOff + nameLen)
        uint32_t start = nameOff_[id];
        uint32_t end = start + nameLen_[id];
        if (offset >= start && offset < end) {
            return id;
        }
    }
    return kNoEntry;
}

std::string NameIndex::buildPath(EntryId id) const {
    if (id >= entryCount_ || isDeleted(id)) {
        return {};
    }

    std::vector<std::string_view> segments;
    EntryId curr = id;
    int depth = 0;

    while (curr != kNoEntry && depth++ < 256) {
        segments.push_back(getName(curr));
        EntryId p = parent_[curr];
        if (p == curr) {
            break; // Root points to itself
        }
        curr = p;
    }

    std::string path;
    for (auto it = segments.rbegin(); it != segments.rend(); ++it) {
        if (!path.empty() && path.back() != '\\' && path.back() != '/') {
            path.push_back('\\');
        }
        path.append(*it);
    }
    return path;
}

void NameIndex::resolveParents() {
    for (const auto& [childId, parentFrn] : pendingParents_) {
        if (childId < entryCount_ && !isDeleted(childId) && parent_[childId] == kNoEntry) {
            auto it = frnToIdMap_.find(parentFrn);
            if (it != frnToIdMap_.end()) {
                parent_[childId] = it->second;
            }
        }
    }
    pendingParents_.clear();
    pendingParents_.shrink_to_fit();
}

std::string_view NameIndex::getName(EntryId id) const {
    if (id >= entryCount_ || isDeleted(id)) {
        return {};
    }
    return std::string_view(arenaOrig_.data() + nameOff_[id], nameLen_[id]);
}

std::string_view NameIndex::getFoldedName(EntryId id) const {
    if (id >= entryCount_ || isDeleted(id)) {
        return {};
    }
    return std::string_view(arenaFold_.data() + nameOff_[id], nameLen_[id]);
}

size_t NameIndex::totalMemoryBytes() const noexcept {
    size_t bytes = 0;
    bytes += nameOff_.capacity() * sizeof(uint32_t);
    bytes += nameLen_.capacity() * sizeof(uint16_t);
    bytes += parent_.capacity() * sizeof(EntryId);
    bytes += frn_.capacity() * sizeof(uint64_t);
    bytes += size_.capacity() * sizeof(uint64_t);
    bytes += mtime_.capacity() * sizeof(uint64_t);
    bytes += flags_.capacity() * sizeof(uint8_t);
    bytes += letters_.capacity() * sizeof(uint32_t);
    bytes += arenaOrig_.capacity() * sizeof(char);
    bytes += arenaFold_.capacity() * sizeof(char);
    bytes += offsetMap_.capacity() * sizeof(OffsetIdPair);
    bytes += frnToIdMap_.size() * (sizeof(uint64_t) + sizeof(EntryId) + sizeof(void*) * 3);
    bytes += freeList_.capacity() * sizeof(EntryId);
    return bytes;
}

} // namespace gx

