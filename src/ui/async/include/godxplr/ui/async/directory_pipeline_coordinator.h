#pragma once

#include "godxplr/core/provider_contract.h"
#include "godxplr/ui/model/directory_store.h"
#include "godxplr/ui/async/ui_mailbox.h"
#include "godxplr/ui/async/directory_sorter.h"
#include <memory>
#include <string>
#include <chrono>
#include <thread>
#include <vector>
#include <optional>

namespace gx::ui {

struct PipelineMilestones {
    std::chrono::steady_clock::time_point t0_navigate{};
    std::chrono::steady_clock::time_point t1_first_row{};
    std::chrono::steady_clock::time_point t2_first_paint{};
    std::chrono::steady_clock::time_point t3_enum_complete{};
    std::chrono::steady_clock::time_point t4_sort_complete{};

    [[nodiscard]] double latencyFirstRowMs() const noexcept {
        if (t0_navigate.time_since_epoch().count() == 0 || t1_first_row.time_since_epoch().count() == 0) return 0.0;
        return std::chrono::duration<double, std::milli>(t1_first_row - t0_navigate).count();
    }

    [[nodiscard]] double latencyEnumCompleteMs() const noexcept {
        if (t0_navigate.time_since_epoch().count() == 0 || t3_enum_complete.time_since_epoch().count() == 0) return 0.0;
        return std::chrono::duration<double, std::milli>(t3_enum_complete - t0_navigate).count();
    }

    [[nodiscard]] double latencySortCompleteMs() const noexcept {
        if (t0_navigate.time_since_epoch().count() == 0 || t4_sort_complete.time_since_epoch().count() == 0) return 0.0;
        return std::chrono::duration<double, std::milli>(t4_sort_complete - t0_navigate).count();
    }
};

class DirectoryPipelineCoordinator {
public:
    explicit DirectoryPipelineCoordinator(
        std::shared_ptr<IDirectoryProvider> provider,
        size_t mailboxCapacity = 8);
    ~DirectoryPipelineCoordinator();

    DirectoryPipelineCoordinator(const DirectoryPipelineCoordinator&) = delete;
    DirectoryPipelineCoordinator& operator=(const DirectoryPipelineCoordinator&) = delete;

    // Initiates navigation to a new location.
    // Cancels any in-flight enumeration/sort tasks and advances the generation token.
    void navigateTo(std::string_view uri, const SortSpec& sortSpec = {});

    // Drains batches from the mailbox into the store.
    // Call this from the UI thread or harness event loop turn.
    // Returns true if more batches are pending or enumeration is in progress.
    bool drain(size_t maxBatches = 8);

    // Forces immediate completion of worker threads (e.g. for deterministic testing)
    void join();

    // Updates sort specification for the current store, running sort in the background
    void applySort(const SortSpec& sortSpec);

    // Selection and anchor restoration across sort/refresh
    void setSavedSelection(std::vector<EntryKey> keys, std::optional<EntryKey> anchor);
    [[nodiscard]] std::vector<uint64_t> resolveSavedSelectionRows() const;
    [[nodiscard]] std::optional<uint64_t> resolveSavedAnchorRow() const;

    // Accessors
    [[nodiscard]] const DirectoryStore& store() const noexcept { return store_; }
    [[nodiscard]] DirectoryStore& store() noexcept { return store_; }
    [[nodiscard]] const UiMailbox& mailbox() const noexcept { return mailbox_; }
    [[nodiscard]] ProviderStatus status() const noexcept { return status_; }
    [[nodiscard]] uint64_t currentGeneration() const noexcept { return currentGeneration_; }
    [[nodiscard]] const std::string& currentUri() const noexcept { return currentUri_; }
    [[nodiscard]] const PipelineMilestones& milestones() const noexcept { return milestones_; }
    [[nodiscard]] bool isEnumerating() const noexcept { return isEnumerating_.load(std::memory_order_acquire); }

    void recordFirstPaint() noexcept {
        if (milestones_.t2_first_paint.time_since_epoch().count() == 0) {
            milestones_.t2_first_paint = std::chrono::steady_clock::now();
        }
    }

private:
    void cancelAndJoinActiveWorkers();

    std::shared_ptr<IDirectoryProvider> provider_;
    DirectoryStore store_;
    UiMailbox mailbox_;
    SortSpec sortSpec_{};

    std::string currentUri_;
    uint64_t currentGeneration_{0};
    uint64_t requestIdCounter_{0};
    ProviderStatus status_{ProviderStatus::Ready};

    std::shared_ptr<CancellationToken> cancelToken_;
    std::thread enumWorker_;
    std::thread sortWorker_;
    std::atomic<bool> isEnumerating_{false};

    std::vector<EntryKey> savedSelectionKeys_;
    std::optional<EntryKey> savedAnchorKey_;

    PipelineMilestones milestones_;
};

} // namespace gx::ui
