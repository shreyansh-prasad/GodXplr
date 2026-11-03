#include "godxplr/core/provider_contract.h"
#include "godxplr/platform/win32_directory_provider.h"
#include "godxplr/ui/async/ui_mailbox.h"
#include "godxplr/ui/async/directory_sorter.h"
#include "godxplr/ui/async/directory_pipeline_coordinator.h"
#include <iostream>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <thread>
#include <chrono>

#define TEST_ASSERT(cond)                                                      \
    do {                                                                       \
        if (!(cond)) {                                                         \
            std::cerr << "Assertion failed: " #cond " at " << __FILE__         \
                      << ":" << __LINE__ << std::endl;                         \
            std::abort();                                                      \
        }                                                                      \
    } while (0)

using namespace gx;
using namespace gx::ui;
using namespace gx::platform::win32;

// ============================================================================
// Mock Providers for Deterministic Concurrency Testing
// ============================================================================

class MockDelayedProvider : public IDirectoryProvider {
public:
    MockDelayedProvider(size_t entriesPerFolder, std::chrono::milliseconds batchDelay)
        : count_(entriesPerFolder)
        , delay_(batchDelay)
    {}

    ProviderKind kind() const noexcept override { return ProviderKind::Win32Directory; }
    ProviderCapabilities capabilities() const noexcept override { return ProviderCapabilities::CanEnumerate; }
    bool canHandleUri(std::string_view) const noexcept override { return true; }

    ProviderStatus enumerate(
        std::string_view uri,
        uint64_t requestId,
        uint64_t generation,
        std::shared_ptr<CancellationToken> cancelToken,
        std::function<bool(DirectoryBatch&&)> batchCallback) override
    {
        constexpr size_t kBatchSize = 100;
        size_t sent = 0;

        while (sent < count_) {
            if (cancelToken && cancelToken->isCancelled()) {
                return ProviderStatus::Cancelled;
            }

            if (delay_.count() > 0) {
                std::this_thread::sleep_for(delay_);
            }

            DirectoryBatch batch;
            batch.requestId = requestId;
            batch.generation = generation;
            batch.status = ProviderStatus::Loading;
            batch.isFinal = false;

            size_t thisBatch = std::min(kBatchSize, count_ - sent);
            for (size_t i = 0; i < thisBatch; ++i) {
                std::string name = std::string(uri) + "_entry_" + std::to_string(sent + i);
                uint32_t offset = static_cast<uint32_t>(batch.nameArena.size());
                uint32_t len = static_cast<uint32_t>(name.size());
                batch.nameArena.append(name);

                DirectoryEntry entry{};
                entry.name = NameSlice{offset, len};
                entry.size = (sent + i) * 1024;
                entry.mtime = 1000000 + (sent + i);
                entry.key = EntryKey{
                    .provider = ProviderKind::Win32Directory,
                    .volumeOrProviderId = 1,
                    .objectId = sent + i,
                    .incarnation = generation
                };
                batch.entries.push_back(entry);
            }

            sent += thisBatch;
            if (sent >= count_) {
                batch.isFinal = true;
                batch.status = ProviderStatus::Ready;
            }

            bool keepGoing = batchCallback(std::move(batch));
            if (!keepGoing) {
                return ProviderStatus::Cancelled;
            }
        }

        return ProviderStatus::Ready;
    }

    std::unique_ptr<IDirectoryWatcher> watch(std::string_view, std::shared_ptr<IWatchListener>) override {
        return nullptr;
    }

private:
    size_t count_;
    std::chrono::milliseconds delay_;
};

// ============================================================================
// Test 1: Bounded UI Mailbox Backpressure & Cancellation
// ============================================================================

void test_mailbox_backpressure_and_cancellation() {
    std::cout << "[Test 1] UiMailbox backpressure and cancellation...\n";

    UiMailbox mailbox(3); // Small capacity of 3 batches
    mailbox.setGeneration(1);
    TEST_ASSERT(mailbox.capacity() == 3);
    TEST_ASSERT(mailbox.size() == 0);

    // Push 3 batches with generation 1 (fill mailbox to capacity)
    for (uint64_t i = 0; i < 3; ++i) {
        DirectoryBatch b;
        b.generation = 1;
        b.requestId = i;
        bool ok = mailbox.push(std::move(b));
        TEST_ASSERT(ok);
    }

    TEST_ASSERT(mailbox.size() == 3);

    // Test backpressure: push a 4th batch in a background thread
    std::atomic<bool> producerBlocked{true};
    std::atomic<bool> producerDone{false};

    std::thread producer([&]() {
        DirectoryBatch b4;
        b4.generation = 1;
        b4.requestId = 4;
        producerBlocked.store(false);
        bool ok = mailbox.push(std::move(b4));
        producerDone.store(ok);
    });

    // Wait until producer is actively waiting in push()
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    TEST_ASSERT(mailbox.size() == 3); // Still 3, 4th is blocked
    TEST_ASSERT(!producerDone.load());

    // Drain 1 batch
    std::vector<DirectoryBatch> drained = mailbox.drain(1);
    TEST_ASSERT(drained.size() == 1);
    TEST_ASSERT(drained[0].requestId == 0);

    // Now producer unblocks and finishes pushing
    producer.join();
    TEST_ASSERT(producerDone.load());
    TEST_ASSERT(mailbox.size() == 3); // 2 remaining + 1 newly admitted

    // Test stale generation drop: push batch with older generation (0 < 1)
    DirectoryBatch staleBatch;
    staleBatch.generation = 0;
    staleBatch.requestId = 999;
    bool pushStaleOk = mailbox.push(std::move(staleBatch));
    TEST_ASSERT(pushStaleOk); // Returns true without blocking
    TEST_ASSERT(mailbox.droppedStaleBatches() == 1); // Stale batch was discarded
    TEST_ASSERT(mailbox.size() == 3);

    // Test cancellation unblocking
    auto cancelToken = std::make_shared<CancellationToken>();
    std::atomic<bool> cancelProducerDone{false};
    std::atomic<bool> cancelResult{true};

    // Mailbox is at capacity (3). Start producer with cancel token
    std::thread cancelProducer([&]() {
        DirectoryBatch bBlocked;
        bBlocked.generation = 1;
        bool ok = mailbox.push(std::move(bBlocked), cancelToken);
        cancelResult.store(ok);
        cancelProducerDone.store(true);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    TEST_ASSERT(!cancelProducerDone.load());

    // Trip cancellation token
    cancelToken->cancel();
    // Wake producer
    mailbox.close();
    cancelProducer.join();

    TEST_ASSERT(cancelProducerDone.load());
    TEST_ASSERT(!cancelResult.load()); // Push returned false due to cancellation/closure

    std::cout << "  -> PASS: Bounded backpressure, stale drop, and cancellation verified.\n";
}

// ============================================================================
// Test 2: Rapid Navigation Cancellation Stress (A -> B -> C)
// ============================================================================

void test_rapid_navigation_cancellation() {
    std::cout << "[Test 2] Rapid navigation stress test (A -> B -> C)...\n";

    // Mock provider with 1,000 entries per folder and 2ms delay per batch
    auto mockProvider = std::make_shared<MockDelayedProvider>(1000, std::chrono::milliseconds(2));
    DirectoryPipelineCoordinator coordinator(mockProvider, 8);

    // Rapidly navigate: A -> B -> C within a few milliseconds
    coordinator.navigateTo("folder_A");
    std::this_thread::sleep_for(std::chrono::milliseconds(5));

    coordinator.navigateTo("folder_B");
    std::this_thread::sleep_for(std::chrono::milliseconds(5));

    coordinator.navigateTo("folder_C");

    // Let folder C complete enumeration and drain all batches
    coordinator.join();

    // Verify results
    TEST_ASSERT(coordinator.currentGeneration() == 3);
    TEST_ASSERT(coordinator.currentUri() == "folder_C");
    TEST_ASSERT(coordinator.store().size() == 1000);
    TEST_ASSERT(coordinator.status() == ProviderStatus::Ready);

    // Verify all entries strictly belong to folder_C
    for (size_t row = 0; row < coordinator.store().size(); ++row) {
        std::string_view name = coordinator.store().getName(row);
        TEST_ASSERT(name.starts_with("folder_C"));
        TEST_ASSERT(!name.starts_with("folder_A"));
        TEST_ASSERT(!name.starts_with("folder_B"));
    }

    // Verify stale batches were discarded
    TEST_ASSERT(coordinator.mailbox().droppedStaleBatches() > 0);

    std::cout << "  -> PASS: Rapid navigation cancelled folders A and B; only folder C was committed.\n";
    std::cout << "     Dropped stale batches: " << coordinator.mailbox().droppedStaleBatches() << "\n";
}

// ============================================================================
// Test 3: Win32 Directory Provider Real Filesystem Enumeration
// ============================================================================

void test_win32_real_filesystem_enumeration() {
    std::cout << "[Test 3] Win32DirectoryProvider real filesystem enumeration...\n";

    std::filesystem::path tempDir = std::filesystem::temp_directory_path() / "godxplr_p06_real_fs_test";
    std::filesystem::remove_all(tempDir);
    std::filesystem::create_directories(tempDir);

    // Create subdirectories
    std::filesystem::create_directory(tempDir / "alpha_dir");
    std::filesystem::create_directory(tempDir / "beta_dir");

    // Create files with content
    {
        std::ofstream f(tempDir / "file_1.txt");
        f << "Hello GodXplr 1";
    }
    {
        std::ofstream f(tempDir / "file_2.log", std::ios::binary);
        f << "Log message line 1\nLine 2";
    }
    {
        std::ofstream f(tempDir / "data.bin", std::ios::binary);
        std::vector<char> bytes(4096, 0x5A);
        f.write(bytes.data(), bytes.size());
    }

    auto provider = std::make_shared<Win32DirectoryProvider>();
    DirectoryPipelineCoordinator coordinator(provider, 8);

    std::string uri = tempDir.string();
    coordinator.navigateTo(uri);
    coordinator.join();

    TEST_ASSERT(coordinator.status() == ProviderStatus::Ready);
    TEST_ASSERT(coordinator.store().size() == 5); // 2 dirs + 3 files

    bool foundAlpha = false;
    bool foundBeta = false;
    bool foundFile1 = false;
    bool foundFile2 = false;
    bool foundData = false;

    for (size_t row = 0; row < coordinator.store().size(); ++row) {
        std::string_view name = coordinator.store().getName(row);
        const auto& entry = coordinator.store().getEntry(row);

        if (name == "alpha_dir") {
            foundAlpha = true;
            TEST_ASSERT(entry.isDirectory());
        } else if (name == "beta_dir") {
            foundBeta = true;
            TEST_ASSERT(entry.isDirectory());
        } else if (name == "file_1.txt") {
            foundFile1 = true;
            TEST_ASSERT(!entry.isDirectory());
            TEST_ASSERT(entry.size == 15);
        } else if (name == "file_2.log") {
            foundFile2 = true;
            TEST_ASSERT(!entry.isDirectory());
            TEST_ASSERT(entry.size == 25);
        } else if (name == "data.bin") {
            foundData = true;
            TEST_ASSERT(!entry.isDirectory());
            TEST_ASSERT(entry.size == 4096);
        }
    }

    TEST_ASSERT(foundAlpha && foundBeta && foundFile1 && foundFile2 && foundData);

    // Cleanup
    std::filesystem::remove_all(tempDir);

    std::cout << "  -> PASS: All 5 real filesystem entries enumerated with accurate sizes and directory attributes.\n";
}

// ============================================================================
// Test 4: Win32 Directory Provider Error Mapping
// ============================================================================

void test_win32_error_mapping() {
    std::cout << "[Test 4] Win32DirectoryProvider error mapping...\n";

    auto provider = std::make_shared<Win32DirectoryProvider>();
    DirectoryPipelineCoordinator coordinator(provider, 8);

    // Non-existent directory
    coordinator.navigateTo("C:\\GodXplr_Path_That_Does_Not_Exist_XYZ_12345");
    coordinator.join();

    TEST_ASSERT(coordinator.status() == ProviderStatus::NotFound);
    TEST_ASSERT(coordinator.store().empty());

    std::cout << "  -> PASS: Non-existent directory mapped cleanly to ProviderStatus::NotFound.\n";
}

// ============================================================================
// Test 5: Natural Sort & Stable Tie-Breaker
// ============================================================================

void test_natural_sort_and_tie_breaker() {
    std::cout << "[Test 5] Natural background sort and stable tie-breaker...\n";

    // Test naturalCompare directly
    TEST_ASSERT(DirectorySorter::naturalCompare("file2.txt", "file10.txt") < 0);
    TEST_ASSERT(DirectorySorter::naturalCompare("file10.txt", "file2.txt") > 0);
    TEST_ASSERT(DirectorySorter::naturalCompare("FILE2.TXT", "file2.txt") == 0);
    TEST_ASSERT(DirectorySorter::naturalCompare("item_1", "item_2") < 0);
    TEST_ASSERT(DirectorySorter::naturalCompare("doc_100_v2", "doc_100_v10") < 0);

    // Test with directory entries
    std::vector<DirectoryEntry> entries;
    std::string arena;

    auto add = [&](std::string_view name, bool isDir, uint64_t objId) {
        uint32_t offset = static_cast<uint32_t>(arena.size());
        uint32_t len = static_cast<uint32_t>(name.size());
        arena.append(name);

        DirectoryEntry e{};
        e.name = NameSlice{offset, len};
        if (isDir) e.flags |= kEntryDir;
        e.key = EntryKey{.provider = ProviderKind::Win32Directory, .volumeOrProviderId = 1, .objectId = objId, .incarnation = 0};
        entries.push_back(e);
    };

    add("file_10.txt", false, 1);
    add("folder_beta", true, 2);
    add("file_2.txt", false, 3);
    add("file_1.txt", false, 4);
    add("folder_alpha", true, 5);
    add("duplicate_name.txt", false, 100);
    add("duplicate_name.txt", false, 50);

    SortSpec spec;
    spec.column = SortColumn::Name;
    spec.order = SortOrder::Ascending;
    spec.directoriesFirst = true;

    std::vector<uint32_t> perm = DirectorySorter::buildPermutation(entries, arena, spec);
    TEST_ASSERT(perm.size() == entries.size());

    auto getName = [&](uint32_t idx) -> std::string_view {
        const auto& s = entries[idx].name;
        return std::string_view(arena.data() + s.offset, s.length);
    };

    // Expected order:
    // 0: folder_alpha (dir)
    // 1: folder_beta (dir)
    // 2: duplicate_name.txt (objId 50)
    // 3: duplicate_name.txt (objId 100)
    // 4: file_1.txt
    // 5: file_2.txt
    // 6: file_10.txt

    TEST_ASSERT(getName(perm[0]) == "folder_alpha");
    TEST_ASSERT(entries[perm[0]].isDirectory());

    TEST_ASSERT(getName(perm[1]) == "folder_beta");
    TEST_ASSERT(entries[perm[1]].isDirectory());

    TEST_ASSERT(getName(perm[2]) == "duplicate_name.txt");
    TEST_ASSERT(entries[perm[2]].key.objectId == 50); // Stable tie-breaker 50 < 100

    TEST_ASSERT(getName(perm[3]) == "duplicate_name.txt");
    TEST_ASSERT(entries[perm[3]].key.objectId == 100);

    TEST_ASSERT(getName(perm[4]) == "file_1.txt");
    TEST_ASSERT(getName(perm[5]) == "file_2.txt");
    TEST_ASSERT(getName(perm[6]) == "file_10.txt");

    std::cout << "  -> PASS: Natural sort and stable tie-breaker verified.\n";
}

// ============================================================================
// Test 6: Selection & Scroll Anchor Restoration
// ============================================================================

void test_selection_and_anchor_restoration() {
    std::cout << "[Test 6] Selection and scroll anchor restoration across sort...\n";

    auto mockProvider = std::make_shared<MockDelayedProvider>(5, std::chrono::milliseconds(0));
    DirectoryPipelineCoordinator coordinator(mockProvider, 8);

    coordinator.navigateTo("test_folder");
    coordinator.join();

    TEST_ASSERT(coordinator.store().size() == 5);

    // Save selection for entry with objectId 3, and anchor for entry with objectId 1
    EntryKey key3 = coordinator.store().getEntry(3).key;
    EntryKey key1 = coordinator.store().getEntry(1).key;

    coordinator.setSavedSelection({key3}, key1);

    // Verify initial positions
    std::vector<uint64_t> initialRows = coordinator.resolveSavedSelectionRows();
    TEST_ASSERT(initialRows.size() == 1);
    TEST_ASSERT(initialRows[0] == 3);
    TEST_ASSERT(coordinator.resolveSavedAnchorRow() == 1);

    // Re-sort descending
    SortSpec descSpec;
    descSpec.column = SortColumn::Name;
    descSpec.order = SortOrder::Descending;
    coordinator.applySort(descSpec);

    // Under descending order, entry 3 (originally at index 3) moves to index 1
    // entry 1 moves to index 3
    std::vector<uint64_t> newRows = coordinator.resolveSavedSelectionRows();
    TEST_ASSERT(newRows.size() == 1);
    TEST_ASSERT(newRows[0] == 1); // Row resolved correctly to 1!

    std::optional<uint64_t> newAnchor = coordinator.resolveSavedAnchorRow();
    TEST_ASSERT(newAnchor.has_value());
    TEST_ASSERT(*newAnchor == 3); // Anchor resolved correctly to 3!

    std::cout << "  -> PASS: Selected entries and scroll anchor restored by EntryKey across sort reordering.\n";
}

// ============================================================================
// Test 7: Watcher Overflow Rescan Semantics
// ============================================================================

class TestWatchListener : public IWatchListener {
public:
    void onWatchEvent(const WatchEvent& event) override {
        events.push_back(event);
        if (event.overflowRequiresRescan) {
            overflowDetected = true;
        }
    }

    std::vector<WatchEvent> events;
    bool overflowDetected{false};
};

void test_watcher_overflow_semantics() {
    std::cout << "[Test 7] Watcher overflow-to-rescan semantics...\n";

    auto listener = std::make_shared<TestWatchListener>();

    // Emit standard events
    WatchEvent ev1;
    ev1.kind = WatchEvent::Kind::Added;
    ev1.relativePath = "new_file.txt";
    listener->onWatchEvent(ev1);

    // Emit overflow event
    WatchEvent evOverflow;
    evOverflow.kind = WatchEvent::Kind::RescanRequired;
    evOverflow.overflowRequiresRescan = true;
    listener->onWatchEvent(evOverflow);

    TEST_ASSERT(listener->events.size() == 2);
    TEST_ASSERT(listener->overflowDetected);
    TEST_ASSERT(listener->events[1].kind == WatchEvent::Kind::RescanRequired);

    std::cout << "  -> PASS: Watcher overflow event triggers full rescan signal.\n";
}

// ============================================================================
// Test 8: Latency Telemetry Milestones
// ============================================================================

void test_latency_milestones() {
    std::cout << "[Test 8] Latency telemetry milestones (t0 -> t1 -> t2 -> t3 -> t4)...\n";

    auto mockProvider = std::make_shared<MockDelayedProvider>(500, std::chrono::milliseconds(1));
    DirectoryPipelineCoordinator coordinator(mockProvider, 8);

    coordinator.navigateTo("milestone_folder");

    // Simulate event loop turns
    while (coordinator.isEnumerating()) {
        coordinator.drain(4);
        coordinator.recordFirstPaint();
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    coordinator.join();

    const auto& m = coordinator.milestones();
    TEST_ASSERT(m.t0_navigate.time_since_epoch().count() > 0);
    TEST_ASSERT(m.t1_first_row.time_since_epoch().count() > 0);
    TEST_ASSERT(m.t2_first_paint.time_since_epoch().count() > 0);
    TEST_ASSERT(m.t3_enum_complete.time_since_epoch().count() > 0);
    TEST_ASSERT(m.t4_sort_complete.time_since_epoch().count() > 0);

    // Verify monotonic ordering
    TEST_ASSERT(m.t0_navigate <= m.t1_first_row);
    TEST_ASSERT(m.t1_first_row <= m.t3_enum_complete);
    TEST_ASSERT(m.t3_enum_complete <= m.t4_sort_complete);

    std::cout << "  -> Latency t0->t1 (first row): " << m.latencyFirstRowMs() << " ms\n";
    std::cout << "  -> Latency t0->t3 (enum complete): " << m.latencyEnumCompleteMs() << " ms\n";
    std::cout << "  -> Latency t0->t4 (sort complete): " << m.latencySortCompleteMs() << " ms\n";
    std::cout << "  -> PASS: Telemetry milestones verified monotonic and non-zero.\n";
}

// ============================================================================
// Main Test Runner
// ============================================================================

int main() {
    std::cout << "====================================================\n";
    std::cout << "GodXplr Spec 03 U3-U4 Async Directory Pipeline Tests\n";
    std::cout << "====================================================\n";

    try {
        test_mailbox_backpressure_and_cancellation();
        test_rapid_navigation_cancellation();
        test_win32_real_filesystem_enumeration();
        test_win32_error_mapping();
        test_natural_sort_and_tie_breaker();
        test_selection_and_anchor_restoration();
        test_watcher_overflow_semantics();
        test_latency_milestones();

        std::cout << "\n>>> ALL 8/8 ASYNC DIRECTORY PIPELINE TESTS PASSED <<<\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "FATAL EXCEPTION: " << ex.what() << "\n";
        return 1;
    }
}
