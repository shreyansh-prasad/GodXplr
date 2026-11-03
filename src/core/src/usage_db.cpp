#include "godxplr/core/usage_db.h"
#include <chrono>

namespace gx::search {

UsageTracker::UsageTracker(size_t maxLruEntries)
    : maxEntries_(maxLruEntries > 0 ? maxLruEntries : 16384)
{
}

uint64_t UsageTracker::hashPath(std::string_view fullPath) noexcept {
    // 64-bit FNV-1a case-insensitive hash
    uint64_t hash = 14695981039346656037ULL;
    for (char c : fullPath) {
        char lower = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        // Normalize slash direction
        if (lower == '/') lower = '\\';
        hash ^= static_cast<uint64_t>(static_cast<unsigned char>(lower));
        hash *= 1099511628211ULL;
    }
    return hash;
}

void UsageTracker::recordOpen(uint64_t pathHash, std::string_view queryPrefix) {
    if (!enabled_ || pathHash == 0) {
        return;
    }

    uint64_t now = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count()
    );

    std::lock_guard<std::mutex> lock(mutex_);
    auto it = map_.find(pathHash);
    if (it != map_.end()) {
        it->second->openCount++;
        it->second->lastOpenTime = now;
        if (!queryPrefix.empty()) {
            it->second->lastQueryPrefix = std::string(queryPrefix);
        }
        lruList_.splice(lruList_.begin(), lruList_, it->second);
        return;
    }

    // Evict least recently used if capacity reached
    if (lruList_.size() >= maxEntries_) {
        uint64_t evictHash = lruList_.back().pathHash;
        map_.erase(evictHash);
        lruList_.pop_back();
    }

    UsageEntry entry;
    entry.pathHash = pathHash;
    entry.openCount = 1;
    entry.lastOpenTime = now;
    entry.lastQueryPrefix = std::string(queryPrefix);

    lruList_.push_front(std::move(entry));
    map_[pathHash] = lruList_.begin();
}

std::optional<UsageEntry> UsageTracker::getUsage(uint64_t pathHash) {
    if (!enabled_ || pathHash == 0) {
        return std::nullopt;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    auto it = map_.find(pathHash);
    if (it == map_.end()) {
        return std::nullopt;
    }

    // Move to front of LRU
    lruList_.splice(lruList_.begin(), lruList_, it->second);
    return *(it->second);
}

void UsageTracker::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    lruList_.clear();
    map_.clear();
}

size_t UsageTracker::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return map_.size();
}

} // namespace gx::search
