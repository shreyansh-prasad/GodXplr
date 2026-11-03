#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <thread>
#include <future>
#include <filesystem>
#include <fstream>
#include <cstdlib>

#include "godxplr/core/name_index.h"
#include "godxplr/core/search_engine.h"
#include "godxplr/core/fuzzy_matcher.h"
#include "godxplr/core/pixel_buffer.h"
#include "godxplr/fileops/operation_journal.h"
#include "godxplr/fileops/conflict_resolver.h"
#include "godxplr/content/text_extractor.h"
#include "godxplr/content/ooxml_extractor.h"
#include "godxplr/content/content_queue.h"
#include "godxplr/providers/virtual_provider.h"
#include "godxplr/providers/zip_archive_provider.h"
#include "godxplr/ui/model/directory_store.h"
#include "godxplr/ui/model/selection_model.h"
#include "godxplr/ui/view/details_layout.h"
#include "godxplr/ui/cache/icon_cache.h"
#include "godxplr/platform/shm_channel.h"

#define TEST_ASSERT(cond) \
    do { \
        if (!(cond)) { \
            std::cerr << "Assertion failed: " #cond " at line " << __LINE__ << std::endl; \
            std::abort(); \
        } \
    } while (0)

namespace fs = std::filesystem;

// 1. Fuzz & Adversarial Path Traversal Tests
void test_adversarial_paths_and_unicode() {
    std::cout << "[Test] Adversarial paths and extreme unicode...\n";

    // Test Zip PathSanitizer against directory traversal exploits
    TEST_ASSERT(!gx::providers::PathSanitizer::isSafePath("../../../Windows/System32/cmd.exe"));
    TEST_ASSERT(!gx::providers::PathSanitizer::isSafePath("/etc/passwd"));
    TEST_ASSERT(!gx::providers::PathSanitizer::isSafePath("C:\\Windows\\System32\\calc.exe"));
    TEST_ASSERT(!gx::providers::PathSanitizer::isSafePath(std::string("file\0.txt", 9)));

    // Safe path normalization
    TEST_ASSERT(gx::providers::PathSanitizer::isSafePath("documents/specs/2026.pdf"));
    std::wstring sanitized = gx::providers::PathSanitizer::sanitizeRelativePath("documents/specs/2026.pdf");
    TEST_ASSERT(sanitized == L"documents\\specs\\2026.pdf");

    // Unicode edge cases in NameIndex
    gx::NameIndex index;
    gx::EntryId root = index.add(5, 5, "C:", gx::kDir);
    (void)root;

    const std::vector<std::string> exotic_names = {
        "Dokumentação_Técnica_2026.pdf",
        "日本語のファイル名_仕様書.docx",
        "Русский_текстовый_документ_версия_3.txt",
        "ملف_توصيف_المشروع_النهائي.dat",
        "quantum_accelerator_spec.rs",
        "spaces and  double   spaces    and-dashes.log"
    };

    for (std::uint32_t i = 0; i < exotic_names.size(); ++i) {
        index.add(100 + i, 5, exotic_names[i], 0, 1024, 13300000000ULL);
    }
    TEST_ASSERT(index.size() == exotic_names.size() + 1);

    // Search query
    gx::SearchEngine engine(index);
    auto results_jp = engine.search("日本語");
    TEST_ASSERT(results_jp.total == 1);

    auto results_en = engine.search("quantum");
    TEST_ASSERT(results_en.total == 1);

    // Fuzzy matching resilience with special unicode
    auto match_res = gx::search::FuzzyMatcher::match("Dokumentação", "dok");
    TEST_ASSERT(match_res.matched && match_res.score > 0);
}

// 2. Fault Injection & Crash Recovery Test
void test_fileops_crash_injection_and_recovery() {
    std::cout << "[Test] File operations crash injection and journal recovery...\n";

    fs::path temp_dir = fs::temp_directory_path() / ("gx_hardening_" + std::to_string(GetTickCount64()));
    fs::create_directories(temp_dir);

    fs::path journal_path = temp_dir / "test_ops.journal";
    fs::path partial_file = temp_dir / "corrupted_target.txt.gxpartial";

    // Create orphaned partial file simulating crash mid-copy
    {
        std::ofstream pfile(partial_file, std::ios::binary);
        pfile << "partial corrupted payload before power outage";
    }
    TEST_ASSERT(fs::exists(partial_file));

    // Create journal with in-progress state
    {
        gx::fileops::OperationJournal journal(journal_path.wstring());
        gx::fileops::JournalEntry entry{};
        entry.opId = 1001;
        entry.type = gx::fileops::OpType::Copy;
        entry.state = gx::fileops::OpState::InProgress;
        entry.sourcePath = (temp_dir / "source.txt").wstring();
        entry.destPath = (temp_dir / "corrupted_target.txt").wstring();
        entry.tempPath = partial_file.wstring();
        entry.totalBytes = 1024;
        entry.timestamp = 12345678;

        journal.logIntent(entry);
    }

    // Recover from journal
    {
        gx::fileops::OperationJournal journal(journal_path.wstring());
        auto rec = journal.recoverInterruptedOperations();
        TEST_ASSERT(rec.inProgressFound == 1);
        TEST_ASSERT(rec.rolledBackCount == 1);
        TEST_ASSERT(rec.tempFilesCleaned == 1);

        // Crash recovery cleaned up orphaned .gxpartial
        TEST_ASSERT(!fs::exists(partial_file));

        auto recovered = journal.getEntry(1001);
        TEST_ASSERT(recovered.has_value());
        TEST_ASSERT(recovered->state == gx::fileops::OpState::RolledBack);
    }

    fs::remove_all(temp_dir);
}

// 3. Content Extraction Limits & Poisoning
void test_content_extraction_safety() {
    std::cout << "[Test] Content extraction limits and poisoning...\n";

    // TextExtractor caps extraction at 100 bytes
    gx::content::TextExtractor extractor(100);
    std::string longText(300, 'X');
    gx::content::StringTextSink sink(100);

    auto status = extractor.extractBuffer(
        reinterpret_cast<const uint8_t*>(longText.data()),
        longText.size(),
        sink
    );
    TEST_ASSERT(status == gx::content::ExtractionStatus::Truncated);
    TEST_ASSERT(sink.text().size() == 100);

    // Test ContentQueue Poisoning
    gx::content::ContentQueue queue;
    gx::content::QueueItem item;
    item.frn = 101;
    item.filePath = L"C:\\poison_pill.dat";
    item.priority = 100;
    item.size = 100;
    item.mtime = 1000;
    queue.enqueue(item);

    // Fail 1
    queue.markFailed(101);
    TEST_ASSERT(queue.failedCount() == 1);
    TEST_ASSERT(queue.poisonedCount() == 0);

    // Fail 2 -> should be poisoned
    queue.markFailed(101);
    TEST_ASSERT(queue.poisonedCount() == 1);

    // Dequeue next -> should be empty since only item is poisoned
    auto next = queue.dequeueNext();
    TEST_ASSERT(!next.has_value());
}

// 4. UI 100k Synthetic Virtualization & Memory Bound Test
void test_ui_100k_virtualization_soak() {
    std::cout << "[Test] UI 100k synthetic virtualization soak...\n";

    gx::ui::DirectoryStore store;
    constexpr size_t kCount = 100'000;
    store.reserve(kCount, kCount * 28);

    for (std::uint64_t i = 0; i < kCount; ++i) {
        gx::EntryKey k{gx::ProviderKind::NtfsVolume, 0x1234, 1000 + i, 1};
        std::string name = "Synthetic_File_Batch_" + std::to_string(i) + ".dat";
        uint32_t flags = (i % 10 == 0) ? gx::kEntryDir : 0;
        store.addEntry(k, name, i * 1024, 1330000000ULL + i, 0, flags);
    }

    TEST_ASSERT(store.size() == kCount);

    // Test layout virtualization
    gx::ui::DetailsLayout layout(28);
    layout.setViewportSize(gx::ui::Size{1920, 1080});

    auto start_time = std::chrono::high_resolution_clock::now();

    // Compute visible ranges at top, middle, and bottom
    gx::ui::Rect exposed{0, 0, 1920, 1080};
    auto vrTop = layout.visibleRange(exposed, kCount, gx::ui::Point{0, 0});
    TEST_ASSERT(vrTop.first == 0);

    auto vrMid = layout.visibleRange(exposed, kCount, gx::ui::Point{0, 50000 * 28});
    TEST_ASSERT(vrMid.first == 50000);

    auto vrBot = layout.visibleRange(exposed, kCount, gx::ui::Point{0, static_cast<int32_t>(kCount * 28 - 1080)});
    TEST_ASSERT(vrBot.lastExclusive == kCount);

    auto elapsed_us = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::high_resolution_clock::now() - start_time).count();

    std::cout << "  Calculated 3 visible ranges over 100k rows in: " << elapsed_us << " us\n";
    TEST_ASSERT(elapsed_us < 5000);

    // SelectionModel range test
    gx::ui::VirtualSelectionModel sel;
    sel.selectRange(1000, 2000);
    TEST_ASSERT(sel.selectedCount() == 1001);
    TEST_ASSERT(sel.isSelected(1500));
    TEST_ASSERT(!sel.isSelected(999));
    TEST_ASSERT(!sel.isSelected(2001));

    sel.clear();
    TEST_ASSERT(sel.selectedCount() == 0);
}

// 5. Concurrent Cache Contention Test
void test_concurrent_cache_contention() {
    std::cout << "[Test] Concurrent icon cache thread safety...\n";

    gx::ui::IconCache cache(256);

    const int thread_count = 4;
    const int iterations = 500;
    std::vector<std::thread> workers;

    for (int t = 0; t < thread_count; ++t) {
        workers.emplace_back([&cache, t, iterations]() {
            for (int i = 0; i < iterations; ++i) {
                std::string ext = "ext_" + std::to_string((t * 50 + i) % 100);
                gx::ui::IconKey key{ext, false, false, 24, 100, 0};

                gx::ui::PixelBuffer buf;
                buf.allocate(16, 16);

                cache.put(key, std::move(buf));
                auto retrieved = cache.get(key);
                if (retrieved.has_value()) {
                    TEST_ASSERT(retrieved->width() == 16);
                }
            }
        });
    }

    for (auto& w : workers) {
        w.join();
    }

    // Total elements in cache must not exceed maxEntries
    TEST_ASSERT(cache.size() <= 256);
}

// 6. IPC Buffer Boundary & Validation Test
void test_ipc_buffer_boundary_hardening() {
    std::cout << "[Test] IPC / SHM boundary hardening...\n";

    constexpr uint32_t kSession = 8888;
    gx::platform::win32::ShmServer server;
    bool created = server.create(kSession, false);
    TEST_ASSERT(created);

    gx::platform::win32::ShmClient client;
    bool connected = client.open(kSession, false);
    TEST_ASSERT(connected);

    // Normal query submit
    uint64_t gen = client.submitQuery("hardening query test", 0);
    TEST_ASSERT(gen > 0);

    bool arrived = server.waitForQuery(500);
    TEST_ASSERT(arrived);

    auto* sBuf = server.getBuffer();
    TEST_ASSERT(sBuf != nullptr);
    TEST_ASSERT(std::string_view(sBuf->queryText) == "hardening query test");

    // Close clean
    client.close();
    server.close();
}

int main() {
    std::cout << "=== Running GodXplr P16 Hardening & Resilience Suite ===\n";

    test_adversarial_paths_and_unicode();
    test_fileops_crash_injection_and_recovery();
    test_content_extraction_safety();
    test_ui_100k_virtualization_soak();
    test_concurrent_cache_contention();
    test_ipc_buffer_boundary_hardening();

    std::cout << "=== ALL HARDENING & RESILIENCE TESTS PASSED ===\n";
    return 0;
}
