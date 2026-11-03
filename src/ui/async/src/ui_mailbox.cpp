#include "godxplr/ui/async/ui_mailbox.h"

namespace gx::ui {

UiMailbox::UiMailbox(size_t capacity)
    : capacity_(capacity > 0 ? capacity : 1)
{
}

UiMailbox::~UiMailbox() {
    close();
}

bool UiMailbox::push(DirectoryBatch&& batch, std::shared_ptr<CancellationToken> cancelToken) {
    std::function<void()> callbackToInvoke;

    {
        std::unique_lock<std::mutex> lock(mutex_);

        // Fast generation check before waiting
        if (batch.generation < currentGeneration_.load(std::memory_order_relaxed)) {
            droppedStaleBatches_.fetch_add(1, std::memory_order_relaxed);
            return true; // Discarded safely without backpressure stall
        }

        // Wait with backpressure if queue is full
        while (queue_.size() >= capacity_) {
            if (closed_.load(std::memory_order_relaxed)) return false;
            if (cancelToken && cancelToken->isCancelled()) return false;
            if (batch.generation < currentGeneration_.load(std::memory_order_relaxed)) {
                droppedStaleBatches_.fetch_add(1, std::memory_order_relaxed);
                return true;
            }
            cvProducer_.wait_for(lock, std::chrono::milliseconds(10));
        }

        if (closed_.load(std::memory_order_relaxed) || (cancelToken && cancelToken->isCancelled())) {
            return false;
        }

        // Re-check generation after wait
        if (batch.generation < currentGeneration_.load(std::memory_order_relaxed)) {
            droppedStaleBatches_.fetch_add(1, std::memory_order_relaxed);
            return true;
        }

        queue_.push_back(std::move(batch));
        totalPushed_.fetch_add(1, std::memory_order_relaxed);
        callbackToInvoke = wakeupCallback_;
    }

    cvConsumer_.notify_one();

    if (callbackToInvoke) {
        callbackToInvoke();
    }

    return true;
}

std::vector<DirectoryBatch> UiMailbox::drain(size_t maxBatches) {
    std::vector<DirectoryBatch> result;
    result.reserve(maxBatches);

    {
        std::lock_guard<std::mutex> lock(mutex_);
        const uint64_t curGen = currentGeneration_.load(std::memory_order_relaxed);

        while (!queue_.empty() && result.size() < maxBatches) {
            DirectoryBatch batch = std::move(queue_.front());
            queue_.pop_front();

            if (batch.generation < curGen) {
                droppedStaleBatches_.fetch_add(1, std::memory_order_relaxed);
                continue; // Skip obsolete batch
            }

            result.push_back(std::move(batch));
        }

        totalDrained_.fetch_add(result.size(), std::memory_order_relaxed);
    }

    if (!result.empty()) {
        cvProducer_.notify_all();
    }

    return result;
}

void UiMailbox::setGeneration(uint64_t generation) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        currentGeneration_.store(generation, std::memory_order_relaxed);

        // Prune old generations immediately
        auto it = queue_.begin();
        while (it != queue_.end()) {
            if (it->generation < generation) {
                droppedStaleBatches_.fetch_add(1, std::memory_order_relaxed);
                it = queue_.erase(it);
            } else {
                ++it;
            }
        }
    }

    cvProducer_.notify_all();
}

void UiMailbox::clear() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        droppedStaleBatches_.fetch_add(queue_.size(), std::memory_order_relaxed);
        queue_.clear();
    }
    cvProducer_.notify_all();
}

void UiMailbox::wakeProducers() {
    cvProducer_.notify_all();
}

void UiMailbox::close() {
    closed_.store(true, std::memory_order_relaxed);
    cvProducer_.notify_all();
    cvConsumer_.notify_all();
}

void UiMailbox::setWakeupCallback(std::function<void()> callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    wakeupCallback_ = std::move(callback);
}

size_t UiMailbox::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.size();
}

bool UiMailbox::empty() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.empty();
}

} // namespace gx::ui
