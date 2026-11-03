#pragma once

#include "godxplr/core/provider_contract.h"
#include <deque>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <atomic>
#include <cstdint>

namespace gx::ui {

class UiMailbox {
public:
    explicit UiMailbox(size_t capacity = 8);
    ~UiMailbox();

    UiMailbox(const UiMailbox&) = delete;
    UiMailbox& operator=(const UiMailbox&) = delete;

    // Enqueues a batch. Blocks if queue is full until space is available,
    // or until closed, or until cancelToken is tripped.
    // If batch.generation < currentGeneration_, immediately drops the batch without blocking.
    bool push(DirectoryBatch&& batch, std::shared_ptr<CancellationToken> cancelToken = nullptr);

    // Drains up to maxBatches from the queue, discarding any batches where generation < currentGeneration_.
    std::vector<DirectoryBatch> drain(size_t maxBatches = 8);

    // Advances the current generation token. Any batches currently in the queue
    // with older generations are immediately discarded.
    void setGeneration(uint64_t generation);

    // Clears the queue and wakes waiting producers.
    void clear();

    // Explicitly wakes any waiting producers
    void wakeProducers();

    // Closes the mailbox and unblocks all waiting threads.
    void close();

    // Sets a callback to be invoked when new data becomes available.
    void setWakeupCallback(std::function<void()> callback);

    [[nodiscard]] size_t size() const;
    [[nodiscard]] size_t capacity() const noexcept { return capacity_; }
    [[nodiscard]] bool empty() const;
    [[nodiscard]] uint64_t currentGeneration() const noexcept {
        return currentGeneration_.load(std::memory_order_relaxed);
    }
    [[nodiscard]] size_t droppedStaleBatches() const noexcept {
        return droppedStaleBatches_.load(std::memory_order_relaxed);
    }
    [[nodiscard]] size_t totalPushed() const noexcept {
        return totalPushed_.load(std::memory_order_relaxed);
    }
    [[nodiscard]] size_t totalDrained() const noexcept {
        return totalDrained_.load(std::memory_order_relaxed);
    }

private:
    const size_t capacity_;
    mutable std::mutex mutex_;
    std::condition_variable cvProducer_;
    std::condition_variable cvConsumer_;
    std::deque<DirectoryBatch> queue_;
    std::function<void()> wakeupCallback_;

    std::atomic<bool> closed_{false};
    std::atomic<uint64_t> currentGeneration_{0};
    std::atomic<size_t> droppedStaleBatches_{0};
    std::atomic<size_t> totalPushed_{0};
    std::atomic<size_t> totalDrained_{0};
};

} // namespace gx::ui
