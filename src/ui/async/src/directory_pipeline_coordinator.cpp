#include "godxplr/ui/async/directory_pipeline_coordinator.h"

namespace gx::ui {

DirectoryPipelineCoordinator::DirectoryPipelineCoordinator(
    std::shared_ptr<IDirectoryProvider> provider,
    size_t mailboxCapacity)
    : provider_(std::move(provider))
    , mailbox_(mailboxCapacity)
{
    cancelToken_ = std::make_shared<CancellationToken>();
}

DirectoryPipelineCoordinator::~DirectoryPipelineCoordinator() {
    cancelAndJoinActiveWorkers();
    mailbox_.close();
}

void DirectoryPipelineCoordinator::cancelAndJoinActiveWorkers() {
    if (cancelToken_) {
        cancelToken_->cancel();
    }
    mailbox_.wakeProducers();

    while (isEnumerating_.load(std::memory_order_acquire)) {
        drain(16);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    if (enumWorker_.joinable()) {
        enumWorker_.join();
    }

    if (sortWorker_.joinable()) {
        sortWorker_.join();
    }

    isEnumerating_.store(false, std::memory_order_release);
}

void DirectoryPipelineCoordinator::navigateTo(std::string_view uri, const SortSpec& sortSpec) {
    const uint64_t gen = ++currentGeneration_;
    mailbox_.setGeneration(gen);

    cancelAndJoinActiveWorkers();

    const uint64_t reqId = ++requestIdCounter_;

    cancelToken_ = std::make_shared<CancellationToken>();
    currentUri_ = std::string(uri);
    sortSpec_ = sortSpec;

    milestones_ = PipelineMilestones{};
    milestones_.t0_navigate = std::chrono::steady_clock::now();

    store_.clear();
    store_.setGeneration(gen);
    status_ = ProviderStatus::Loading;

    isEnumerating_.store(true, std::memory_order_release);

    enumWorker_ = std::thread([this, uriStr = currentUri_, reqId, gen, cancel = cancelToken_]() {
        provider_->enumerate(
            uriStr,
            reqId,
            gen,
            cancel,
            [this, cancel](DirectoryBatch&& batch) -> bool {
                return mailbox_.push(std::move(batch), cancel);
            }
        );
        isEnumerating_.store(false, std::memory_order_release);
    });
}

bool DirectoryPipelineCoordinator::drain(size_t maxBatches) {
    std::vector<DirectoryBatch> batches = mailbox_.drain(maxBatches);

    for (const auto& batch : batches) {
        if (batch.generation != currentGeneration_) {
            continue; // Drop obsolete generation
        }

        if (!batch.entries.empty()) {
            store_.appendBatch(batch);
            if (milestones_.t1_first_row.time_since_epoch().count() == 0) {
                milestones_.t1_first_row = std::chrono::steady_clock::now();
            }
        }

        if (batch.isFinal) {
            status_ = batch.status;
            milestones_.t3_enum_complete = std::chrono::steady_clock::now();

            if (store_.size() > 0) {
                // Background sort on worker or inline
                std::vector<uint32_t> perm = DirectorySorter::buildPermutation(
                    store_.entries(),
                    store_.arena(),
                    sortSpec_,
                    cancelToken_
                );
                if (!perm.empty()) {
                    store_.applyPermutation(std::move(perm), currentGeneration_);
                }
            }
            milestones_.t4_sort_complete = std::chrono::steady_clock::now();
        }
    }

    return !mailbox_.empty() || isEnumerating_.load(std::memory_order_acquire);
}

void DirectoryPipelineCoordinator::join() {
    while (isEnumerating_.load(std::memory_order_acquire)) {
        drain(16);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    if (enumWorker_.joinable()) {
        enumWorker_.join();
    }
    if (sortWorker_.joinable()) {
        sortWorker_.join();
    }
    isEnumerating_.store(false, std::memory_order_release);

    // Drain all remaining batches until mailbox is empty
    while (!mailbox_.empty()) {
        drain(16);
    }
}

void DirectoryPipelineCoordinator::applySort(const SortSpec& sortSpec) {
    sortSpec_ = sortSpec;
    if (store_.empty()) {
        return;
    }

    std::vector<uint32_t> perm = DirectorySorter::buildPermutation(
        store_.entries(),
        store_.arena(),
        sortSpec_,
        cancelToken_
    );

    if (!perm.empty()) {
        store_.applyPermutation(std::move(perm), currentGeneration_);
    }
}

void DirectoryPipelineCoordinator::setSavedSelection(std::vector<EntryKey> keys, std::optional<EntryKey> anchor) {
    savedSelectionKeys_ = std::move(keys);
    savedAnchorKey_ = anchor;
}

std::vector<uint64_t> DirectoryPipelineCoordinator::resolveSavedSelectionRows() const {
    std::vector<uint64_t> rows;
    rows.reserve(savedSelectionKeys_.size());
    for (const auto& key : savedSelectionKeys_) {
        uint64_t row = store_.findVisibleRowByKey(key);
        if (row != UINT64_MAX) {
            rows.push_back(row);
        }
    }
    return rows;
}

std::optional<uint64_t> DirectoryPipelineCoordinator::resolveSavedAnchorRow() const {
    if (!savedAnchorKey_.has_value()) {
        return std::nullopt;
    }
    uint64_t row = store_.findVisibleRowByKey(*savedAnchorKey_);
    if (row != UINT64_MAX) {
        return row;
    }
    return std::nullopt;
}

} // namespace gx::ui
