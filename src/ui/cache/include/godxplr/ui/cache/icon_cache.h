#pragma once

#include "godxplr/ui/cache/cache_keys.h"
#include "godxplr/ui/cache/pixel_buffer.h"
#include <list>
#include <unordered_map>
#include <mutex>
#include <cstdint>
#include <optional>

namespace gx::ui {

struct CacheMetrics {
    uint64_t hits{0};
    uint64_t misses{0};
    uint64_t evictions{0};
    size_t currentBytes{0};
    size_t maxBytes{0};
};

class IconCache {
public:
    explicit IconCache(size_t maxEntries = 2048);
    ~IconCache() = default;

    IconCache(const IconCache&) = delete;
    IconCache& operator=(const IconCache&) = delete;

    std::optional<PixelBuffer> get(const IconKey& key);
    void put(const IconKey& key, PixelBuffer buffer);
    void clear();

    [[nodiscard]] size_t size() const;
    [[nodiscard]] size_t maxEntries() const noexcept { return maxEntries_; }
    [[nodiscard]] CacheMetrics metrics() const;

private:
    using ListType = std::list<std::pair<IconKey, PixelBuffer>>;

    const size_t maxEntries_;
    mutable std::mutex mutex_;
    ListType lruList_;
    std::unordered_map<IconKey, ListType::iterator> map_;

    uint64_t hits_{0};
    uint64_t misses_{0};
    uint64_t evictions_{0};
};

} // namespace gx::ui

