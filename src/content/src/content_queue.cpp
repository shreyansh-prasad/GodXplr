#include "godxplr/content/content_queue.h"
#include <algorithm>
#include <unordered_set>

namespace gx::content {

void ContentQueue::enqueue(QueueItem item) {
    std::lock_guard<std::mutex> lock(mutex_);
    item.status = QueueStatus::Pending;
    items_[item.frn] = std::move(item);
}

std::optional<QueueItem> ContentQueue::dequeueNext() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    QueueItem* bestItem = nullptr;
    for (auto& [frn, item] : items_) {
        if (item.status == QueueStatus::Pending) {
            if (!bestItem || item.priority > bestItem->priority ||
                (item.priority == bestItem->priority && item.queuedAt < bestItem->queuedAt)) {
                bestItem = &item;
            }
        }
    }

    if (bestItem) {
        bestItem->status = QueueStatus::Processing;
        return *bestItem;
    }

    return std::nullopt;
}

void ContentQueue::markComplete(uint64_t frn) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = items_.find(frn);
    if (it != items_.end()) {
        it->second.status = QueueStatus::Indexed;
    }
}

void ContentQueue::markFailed(uint64_t frn) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = items_.find(frn);
    if (it != items_.end()) {
        it->second.attempts++;
        if (it->second.attempts >= 2) {
            it->second.status = QueueStatus::Poisoned;
        } else {
            it->second.status = QueueStatus::Failed;
        }
    }
}

std::optional<QueueItem> ContentQueue::getItem(uint64_t frn) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = items_.find(frn);
    if (it != items_.end()) {
        return it->second;
    }
    return std::nullopt;
}

size_t ContentQueue::pendingCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    size_t count = 0;
    for (const auto& [_, item] : items_) {
        if (item.status == QueueStatus::Pending) {
            count++;
        }
    }
    return count;
}

size_t ContentQueue::failedCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    size_t count = 0;
    for (const auto& [_, item] : items_) {
        if (item.status == QueueStatus::Failed) {
            count++;
        }
    }
    return count;
}

size_t ContentQueue::poisonedCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    size_t count = 0;
    for (const auto& [_, item] : items_) {
        if (item.status == QueueStatus::Poisoned) {
            count++;
        }
    }
    return count;
}

size_t ContentQueue::indexedCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    size_t count = 0;
    for (const auto& [_, item] : items_) {
        if (item.status == QueueStatus::Indexed) {
            count++;
        }
    }
    return count;
}

size_t ContentQueue::totalCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return items_.size();
}

ReconciliationReport ContentQueue::reconcile(
    const std::vector<FileSnapshot>& diskSnapshot,
    uint32_t currentExtractorVersion)
{
    std::lock_guard<std::mutex> lock(mutex_);
    ReconciliationReport report;

    std::unordered_set<uint64_t> snapshotFrns;
    snapshotFrns.reserve(diskSnapshot.size());

    for (const auto& snap : diskSnapshot) {
        snapshotFrns.insert(snap.frn);

        if (snap.isExcluded) {
            auto it = items_.find(snap.frn);
            if (it != items_.end()) {
                items_.erase(it);
                report.removedCount++;
            }
            continue;
        }

        auto it = items_.find(snap.frn);
        if (it == items_.end()) {
            // New file discovered
            QueueItem newItem;
            newItem.frn = snap.frn;
            newItem.filePath = snap.filePath;
            newItem.size = snap.size;
            newItem.mtime = snap.mtime;
            newItem.extractorVersion = currentExtractorVersion;
            newItem.status = QueueStatus::Pending;
            newItem.reason = QueueReason::New;
            newItem.attempts = 0;
            items_[snap.frn] = std::move(newItem);
            report.newEnqueued++;
        } else {
            // File already in queue / indexed
            QueueItem& existing = it->second;
            bool versionMismatch = (existing.extractorVersion < currentExtractorVersion);
            bool metadataChanged = (existing.size != snap.size || existing.mtime != snap.mtime);

            if (existing.status == QueueStatus::Indexed && (metadataChanged || versionMismatch)) {
                existing.size = snap.size;
                existing.mtime = snap.mtime;
                existing.extractorVersion = currentExtractorVersion;
                existing.status = QueueStatus::Pending;
                existing.reason = metadataChanged ? QueueReason::Changed : QueueReason::Upgrade;
                existing.attempts = 0;
                report.changedEnqueued++;
            } else if (existing.status == QueueStatus::Indexed) {
                report.untouchedCount++;
            }
        }
    }

    // Check for deleted files (in queue but missing from snapshot)
    std::vector<uint64_t> toDelete;
    for (const auto& [frn, _] : items_) {
        if (snapshotFrns.find(frn) == snapshotFrns.end()) {
            toDelete.push_back(frn);
        }
    }

    for (uint64_t frn : toDelete) {
        items_.erase(frn);
        report.removedCount++;
    }

    return report;
}

void ContentQueue::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    items_.clear();
}

} // namespace gx::content

