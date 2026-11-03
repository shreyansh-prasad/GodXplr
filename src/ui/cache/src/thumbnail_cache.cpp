#include "godxplr/ui/cache/thumbnail_cache.h"

namespace gx::ui {

ThumbnailCache::ThumbnailCache(size_t maxBytes)
    : maxBytes_(maxBytes > 0 ? maxBytes : 96 * 1024 * 1024)
{
}

std::optional<PixelBuffer> ThumbnailCache::get(const ThumbnailKey& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = map_.find(key);
    if (it == map_.end()) {
        misses_++;
        return std::nullopt;
    }

    hits_++;
    // Move hit node to front of LRU list
    lruList_.splice(lruList_.begin(), lruList_, it->second);
    return it->second->buffer;
}

bool ThumbnailCache::put(const ThumbnailKey& key, PixelBuffer buffer) {
    if (!buffer.isValid()) {
        return false;
    }

    const size_t bytes = buffer.byteCount();
    if (bytes > maxBytes_) {
        return false; // Single image exceeds entire cache capacity
    }

    std::lock_guard<std::mutex> lock(mutex_);

    // If already exists, remove old bytes first
    auto it = map_.find(key);
    if (it != map_.end()) {
        currentBytes_ -= it->second->byteCount;
        it->second->buffer = std::move(buffer);
        it->second->byteCount = bytes;
        currentBytes_ += bytes;
        lruList_.splice(lruList_.begin(), lruList_, it->second);
        return true;
    }

    // Evict least recently used entries until within budget
    while (currentBytes_ + bytes > maxBytes_ && !lruList_.empty()) {
        auto tail = std::prev(lruList_.end());
        currentBytes_ -= tail->byteCount;
        map_.erase(tail->key);
        lruList_.pop_back();
        evictions_++;
    }

    Entry entry;
    entry.key = key;
    entry.buffer = std::move(buffer);
    entry.byteCount = bytes;

    currentBytes_ += bytes;
    lruList_.push_front(std::move(entry));
    map_[key] = lruList_.begin();

    return true;
}

void ThumbnailCache::shrinkToBudget(size_t targetBytes) {
    std::lock_guard<std::mutex> lock(mutex_);
    while (currentBytes_ > targetBytes && !lruList_.empty()) {
        auto tail = std::prev(lruList_.end());
        currentBytes_ -= tail->byteCount;
        map_.erase(tail->key);
        lruList_.pop_back();
        evictions_++;
    }
}

void ThumbnailCache::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    lruList_.clear();
    map_.clear();
    currentBytes_ = 0;
}

size_t ThumbnailCache::currentBytes() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return currentBytes_;
}

size_t ThumbnailCache::count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return map_.size();
}

CacheMetrics ThumbnailCache::metrics() const {
    std::lock_guard<std::mutex> lock(mutex_);
    CacheMetrics m;
    m.hits = hits_;
    m.misses = misses_;
    m.evictions = evictions_;
    m.currentBytes = currentBytes_;
    m.maxBytes = maxBytes_;
    return m;
}

} // namespace gx::ui
