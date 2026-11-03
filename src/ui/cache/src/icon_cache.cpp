#include "godxplr/ui/cache/icon_cache.h"

namespace gx::ui {

IconCache::IconCache(size_t maxEntries)
    : maxEntries_(maxEntries > 0 ? maxEntries : 2048)
{
}

std::optional<PixelBuffer> IconCache::get(const IconKey& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = map_.find(key);
    if (it == map_.end()) {
        misses_++;
        return std::nullopt;
    }

    hits_++;
    // Move to front of LRU list
    lruList_.splice(lruList_.begin(), lruList_, it->second);
    return it->second->second;
}

void IconCache::put(const IconKey& key, PixelBuffer buffer) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = map_.find(key);
    if (it != map_.end()) {
        it->second->second = std::move(buffer);
        lruList_.splice(lruList_.begin(), lruList_, it->second);
        return;
    }

    if (map_.size() >= maxEntries_) {
        // Evict least recently used (tail)
        auto tail = std::prev(lruList_.end());
        map_.erase(tail->first);
        lruList_.pop_back();
        evictions_++;
    }

    lruList_.emplace_front(key, std::move(buffer));
    map_[key] = lruList_.begin();
}

void IconCache::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    lruList_.clear();
    map_.clear();
}

size_t IconCache::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return map_.size();
}

CacheMetrics IconCache::metrics() const {
    std::lock_guard<std::mutex> lock(mutex_);
    CacheMetrics m;
    m.hits = hits_;
    m.misses = misses_;
    m.evictions = evictions_;
    m.currentBytes = 0;
    m.maxBytes = maxEntries_;
    return m;
}

} // namespace gx::ui

