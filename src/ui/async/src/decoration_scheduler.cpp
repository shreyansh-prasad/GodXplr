#include "godxplr/ui/async/decoration_scheduler.h"
#include <algorithm>

namespace gx::ui {

DecorationScheduler::DecorationScheduler(
    size_t iconCacheEntries,
    size_t thumbnailCacheBytes)
    : iconCache_(iconCacheEntries)
    , thumbnailCache_(thumbnailCacheBytes)
{
#ifdef _WIN32
    workerThread_ = std::make_unique<platform::win32::StaWorkerThread>();
#endif
}

DecorationScheduler::~DecorationScheduler() {
    cancelAll();
    if (workerThread_) {
        workerThread_->stop();
    }
}

void DecorationScheduler::setIconReadyCallback(IconReadyCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    iconCallback_ = std::move(callback);
}

void DecorationScheduler::setThumbnailReadyCallback(ThumbnailReadyCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    thumbnailCallback_ = std::move(callback);
}

void DecorationScheduler::setGeneration(uint64_t generation) {
    currentGeneration_.store(generation, std::memory_order_release);
    cancelAll();
}

std::optional<core::PixelBuffer> DecorationScheduler::requestIcon(
    const IconKey& key,
    const std::wstring& filePath,
    uint64_t generation,
    uint32_t rowIndex,
    DecorationPriority priority)
{
    // Fast memory-cache hit check
    auto cached = iconCache_.get(key);
    if (cached.has_value()) {
        return cached;
    }

    if (generation != currentGeneration_.load(std::memory_order_relaxed)) {
        return std::nullopt; // Stale navigation generation
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (activeIconKeys_.contains(key)) {
            return std::nullopt; // Already in-flight or queued
        }

        activeIconKeys_.insert(key);
        pendingIcons_.push_back(IconRequest{key, filePath, generation, rowIndex, priority});

        // Keep highest priority items at the back for O(1) pop_back()
        std::stable_sort(pendingIcons_.begin(), pendingIcons_.end(),
            [](const IconRequest& a, const IconRequest& b) {
                return static_cast<uint8_t>(a.priority) > static_cast<uint8_t>(b.priority);
            });
    }

    dispatchNextIcon();
    return std::nullopt;
}

std::optional<core::PixelBuffer> DecorationScheduler::requestThumbnail(
    const ThumbnailKey& key,
    const std::wstring& filePath,
    uint64_t generation,
    uint32_t rowIndex,
    DecorationPriority priority)
{
    // Fast memory-cache hit check
    auto cached = thumbnailCache_.get(key);
    if (cached.has_value()) {
        return cached;
    }

    if (generation != currentGeneration_.load(std::memory_order_relaxed)) {
        return std::nullopt; // Stale navigation generation
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (activeThumbnailKeys_.contains(key)) {
            return std::nullopt; // Already in-flight or queued
        }

        activeThumbnailKeys_.insert(key);
        pendingThumbnails_.push_back(ThumbnailRequest{key, filePath, generation, rowIndex, priority});

        // Keep highest priority items at the back for O(1) pop_back()
        std::stable_sort(pendingThumbnails_.begin(), pendingThumbnails_.end(),
            [](const ThumbnailRequest& a, const ThumbnailRequest& b) {
                return static_cast<uint8_t>(a.priority) > static_cast<uint8_t>(b.priority);
            });
    }

    dispatchNextThumbnail();
    return std::nullopt;
}

void DecorationScheduler::updateVisibleRange(
    uint64_t generation,
    uint32_t startRow,
    uint32_t endRow,
    [[maybe_unused]] int scrollDelta)
{
    std::lock_guard<std::mutex> lock(mutex_);
    constexpr uint32_t kMargin = 8;
    const uint32_t minRow = (startRow > kMargin) ? (startRow - kMargin) : 0;
    const uint32_t maxRow = endRow + kMargin;

    // Prune out-of-view queued icons
    auto iconIt = std::remove_if(pendingIcons_.begin(), pendingIcons_.end(),
        [&](const IconRequest& req) {
            if (req.generation == generation && (req.rowIndex < minRow || req.rowIndex > maxRow)) {
                activeIconKeys_.erase(req.key);
                return true;
            }
            return false;
        });
    pendingIcons_.erase(iconIt, pendingIcons_.end());

    // Prune out-of-view queued thumbnails
    auto thumbIt = std::remove_if(pendingThumbnails_.begin(), pendingThumbnails_.end(),
        [&](const ThumbnailRequest& req) {
            if (req.generation == generation && (req.rowIndex < minRow || req.rowIndex > maxRow)) {
                activeThumbnailKeys_.erase(req.key);
                return true;
            }
            return false;
        });
    pendingThumbnails_.erase(thumbIt, pendingThumbnails_.end());
}

void DecorationScheduler::cancelAll() {
    std::lock_guard<std::mutex> lock(mutex_);
    pendingIcons_.clear();
    pendingThumbnails_.clear();
    activeIconKeys_.clear();
    activeThumbnailKeys_.clear();
}

void DecorationScheduler::shrinkMemoryToBudget(size_t maxBytes) {
    thumbnailCache_.shrinkToBudget(maxBytes);
}

size_t DecorationScheduler::pendingRequestCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return pendingIcons_.size() + pendingThumbnails_.size();
}

void DecorationScheduler::dispatchNextIcon() {
#ifdef _WIN32
    if (!workerThread_) return;

    IconRequest req;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (pendingIcons_.empty()) return;
        req = std::move(pendingIcons_.back());
        pendingIcons_.pop_back();
    }

    workerThread_->post([this, req]() {
        if (req.generation != currentGeneration_.load(std::memory_order_relaxed)) {
            std::lock_guard<std::mutex> lock(mutex_);
            activeIconKeys_.erase(req.key);
            return;
        }

        std::wstring lookupPath = req.filePath;
        if (lookupPath.empty()) {
            lookupPath = std::wstring(req.key.extensionOrType.begin(), req.key.extensionOrType.end());
            if (!lookupPath.empty() && lookupPath.front() != L'.') {
                lookupPath = L"." + lookupPath;
            }
        }

        auto extracted = platform::win32::ShellThumbnailProvider::extractIcon(
            lookupPath,
            req.key.isDirectory,
            req.key.sizeBucket);

        IconReadyCallback cb;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            activeIconKeys_.erase(req.key);
            cb = iconCallback_;
        }

        if (extracted.has_value() && req.generation == currentGeneration_.load(std::memory_order_relaxed)) {
            iconCache_.put(req.key, *extracted);
            if (cb) {
                cb(req.key, *extracted, req.generation);
            }
        }
    });
#endif
}

void DecorationScheduler::dispatchNextThumbnail() {
#ifdef _WIN32
    if (!workerThread_) return;

    ThumbnailRequest req;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (pendingThumbnails_.empty()) return;
        req = std::move(pendingThumbnails_.back());
        pendingThumbnails_.pop_back();
    }

    workerThread_->post([this, req]() {
        if (req.generation != currentGeneration_.load(std::memory_order_relaxed)) {
            std::lock_guard<std::mutex> lock(mutex_);
            activeThumbnailKeys_.erase(req.key);
            return;
        }

        auto extracted = platform::win32::ShellThumbnailProvider::extractThumbnail(
            req.filePath,
            req.key.targetWidth,
            req.key.targetHeight);

        ThumbnailReadyCallback cb;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            activeThumbnailKeys_.erase(req.key);
            cb = thumbnailCallback_;
        }

        if (extracted.has_value() && req.generation == currentGeneration_.load(std::memory_order_relaxed)) {
            thumbnailCache_.put(req.key, *extracted);
            if (cb) {
                cb(req.key, *extracted, req.generation);
            }
        }
    });
#endif
}

} // namespace gx::ui
