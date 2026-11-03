#pragma once

#include "godxplr/ui/cache/cache_keys.h"
#include "godxplr/ui/cache/pixel_buffer.h"
#include "godxplr/ui/cache/icon_cache.h"
#include <list>
#include <unordered_map>
#include <mutex>
#include <cstdint>
#include <optional>

namespace gx::ui {

class ThumbnailCache {
public:
    explicit ThumbnailCache(size_t maxBytes = 96 * 1024 * 1024); // 96 MB default budget
    ~ThumbnailCache() = default;

    ThumbnailCache(const ThumbnailCache&) = delete;
    ThumbnailCache& operator=(const ThumbnailCache&) = delete;

    std::optional<PixelBuffer> get(const ThumbnailKey& key);
    bool put(const ThumbnailKey& key, PixelBuffer buffer);
    void shrinkToBudget(size_t targetBytes);
    void clear();

    [[nodiscard]] size_t currentBytes() const;
    [[nodiscard]] size_t maxBytes() const noexcept { return maxBytes_; }
    [[nodiscard]] size_t count() const;
    [[nodiscard]] CacheMetrics metrics() const;

private:
    struct Entry {
        ThumbnailKey key;
        PixelBuffer buffer;
        size_t byteCount{0};
    };

    using ListType = std::list<Entry>;

    const size_t maxBytes_;
    mutable std::mutex mutex_;
    size_t currentBytes_{0};
    ListType lruList_;
    std::unordered_map<ThumbnailKey, ListType::iterator> map_;

    uint64_t hits_{0};
    uint64_t misses_{0};
    uint64_t evictions_{0};
};

} // namespace gx::ui

