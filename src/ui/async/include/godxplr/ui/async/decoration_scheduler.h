#pragma once

#include "godxplr/core/pixel_buffer.h"
#include "godxplr/ui/cache/cache_keys.h"
#include "godxplr/ui/cache/icon_cache.h"
#include "godxplr/ui/cache/thumbnail_cache.h"
#include "godxplr/platform/com_apartment.h"
#include "godxplr/platform/shell_thumbnail_provider.h"

#include <string>
#include <vector>
#include <queue>
#include <unordered_set>
#include <mutex>
#include <functional>
#include <atomic>
#include <memory>
#include <optional>

namespace gx::ui {

enum class DecorationPriority : uint8_t {
    VisibleNow = 0,
    NearViewport = 1,
    BackgroundTab = 2
};

struct IconRequest {
    IconKey key;
    std::wstring filePath;
    uint64_t generation{0};
    uint32_t rowIndex{0};
    DecorationPriority priority{DecorationPriority::VisibleNow};
};

struct ThumbnailRequest {
    ThumbnailKey key;
    std::wstring filePath;
    uint64_t generation{0};
    uint32_t rowIndex{0};
    DecorationPriority priority{DecorationPriority::VisibleNow};
};

using IconReadyCallback = std::function<void(const IconKey& key, const core::PixelBuffer& buffer, uint64_t generation)>;
using ThumbnailReadyCallback = std::function<void(const ThumbnailKey& key, const core::PixelBuffer& buffer, uint64_t generation)>;

class DecorationScheduler {
public:
    DecorationScheduler(
        size_t iconCacheEntries = 2048,
        size_t thumbnailCacheBytes = 96 * 1024 * 1024);
    ~DecorationScheduler();

    DecorationScheduler(const DecorationScheduler&) = delete;
    DecorationScheduler& operator=(const DecorationScheduler&) = delete;

    void setIconReadyCallback(IconReadyCallback callback);
    void setThumbnailReadyCallback(ThumbnailReadyCallback callback);

    // Updates active generation; invalidates/cancels any queued or returning work from older generations
    void setGeneration(uint64_t generation);
    [[nodiscard]] uint64_t generation() const noexcept { return currentGeneration_.load(std::memory_order_relaxed); }

    // Synchronous memory-cache check. If cache miss, enqueues request with priority.
    std::optional<core::PixelBuffer> requestIcon(
        const IconKey& key,
        const std::wstring& filePath,
        uint64_t generation,
        uint32_t rowIndex,
        DecorationPriority priority = DecorationPriority::VisibleNow);

    // Synchronous memory-cache check. If cache miss, enqueues request with priority.
    std::optional<core::PixelBuffer> requestThumbnail(
        const ThumbnailKey& key,
        const std::wstring& filePath,
        uint64_t generation,
        uint32_t rowIndex,
        DecorationPriority priority = DecorationPriority::VisibleNow);

    // Prunes queued requests that have scrolled out of view
    void updateVisibleRange(uint64_t generation, uint32_t startRow, uint32_t endRow, int scrollDelta = 0);

    // Cancels all pending queued requests
    void cancelAll();

    // Cache access and memory management
    [[nodiscard]] IconCache& iconCache() noexcept { return iconCache_; }
    [[nodiscard]] ThumbnailCache& thumbnailCache() noexcept { return thumbnailCache_; }
    void shrinkMemoryToBudget(size_t maxBytes);

    [[nodiscard]] size_t pendingRequestCount() const;

private:
    struct CompareThumbnailPriority {
        bool operator()(const ThumbnailRequest& a, const ThumbnailRequest& b) const {
            return static_cast<uint8_t>(a.priority) > static_cast<uint8_t>(b.priority);
        }
    };

    struct CompareIconPriority {
        bool operator()(const IconRequest& a, const IconRequest& b) const {
            return static_cast<uint8_t>(a.priority) > static_cast<uint8_t>(b.priority);
        }
    };

    void dispatchNextIcon();
    void dispatchNextThumbnail();

    IconCache iconCache_;
    ThumbnailCache thumbnailCache_;

    std::atomic<uint64_t> currentGeneration_{0};
    mutable std::mutex mutex_;

    IconReadyCallback iconCallback_;
    ThumbnailReadyCallback thumbnailCallback_;

    std::vector<IconRequest> pendingIcons_;
    std::vector<ThumbnailRequest> pendingThumbnails_;
    std::unordered_set<IconKey> activeIconKeys_;
    std::unordered_set<ThumbnailKey> activeThumbnailKeys_;

    std::unique_ptr<platform::win32::StaWorkerThread> workerThread_;
};

} // namespace gx::ui

