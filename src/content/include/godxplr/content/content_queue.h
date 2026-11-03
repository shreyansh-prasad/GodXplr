#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <optional>

namespace gx::content {

enum class QueueReason : uint8_t {
    New,
    Changed,
    Upgrade,
    Retry
};

enum class QueueStatus : uint8_t {
    Pending,
    Processing,
    Indexed,
    Failed,
    Poisoned
};

struct QueueItem {
    uint64_t frn{0};
    std::wstring filePath;
    uint32_t priority{50};
    uint64_t mtime{0};
    uint64_t size{0};
    uint32_t extractorVersion{1};
    QueueReason reason{QueueReason::New};
    QueueStatus status{QueueStatus::Pending};
    uint32_t attempts{0};
    uint64_t queuedAt{0};
};

struct FileSnapshot {
    uint64_t frn{0};
    std::wstring filePath;
    uint64_t size{0};
    uint64_t mtime{0};
    bool isExcluded{false};
};

struct ReconciliationReport {
    size_t newEnqueued{0};
    size_t changedEnqueued{0};
    size_t removedCount{0};
    size_t untouchedCount{0};
};

class ContentQueue {
public:
    ContentQueue() = default;
    ~ContentQueue() = default;

    ContentQueue(const ContentQueue&) = delete;
    ContentQueue& operator=(const ContentQueue&) = delete;

    void enqueue(QueueItem item);

    // Dequeues highest priority pending item
    std::optional<QueueItem> dequeueNext();

    void markComplete(uint64_t frn);
    void markFailed(uint64_t frn);

    [[nodiscard]] std::optional<QueueItem> getItem(uint64_t frn) const;
    [[nodiscard]] size_t pendingCount() const;
    [[nodiscard]] size_t failedCount() const;
    [[nodiscard]] size_t poisonedCount() const;
    [[nodiscard]] size_t indexedCount() const;
    [[nodiscard]] size_t totalCount() const;

    // Spec 02 §5.3: Reconciles filesystem snapshot against queue state
    ReconciliationReport reconcile(
        const std::vector<FileSnapshot>& diskSnapshot,
        uint32_t currentExtractorVersion = 1);

    void clear();

private:
    mutable std::mutex mutex_;
    std::unordered_map<uint64_t, QueueItem> items_;
};

} // namespace gx::content

