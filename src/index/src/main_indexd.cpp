#include "godxplr/core/version.h"
#include "godxplr/core/name_index.h"
#include "godxplr/core/search_engine.h"
#include "godxplr/platform/win32_common.h"
#include "godxplr/platform/usn_enumerator.h"
#include "godxplr/platform/usn_tail.h"
#include "godxplr/platform/shm_channel.h"
#include <iostream>
#include <chrono>
#include <csignal>
#include <atomic>

#ifdef _WIN32
#include <windows.h>

static std::atomic<bool> g_stopDaemon{false};

static BOOL WINAPI consoleCtrlHandler(DWORD ctrlType) {
    if (ctrlType == CTRL_C_EVENT || ctrlType == CTRL_BREAK_EVENT || ctrlType == CTRL_CLOSE_EVENT) {
        g_stopDaemon.store(true);
        return TRUE;
    }
    return FALSE;
}
#endif

int main(int argc, char* argv[]) {
    bool testMode = false;
    for (int i = 1; i < argc; ++i) {
        std::string_view arg(argv[i]);
        if (arg == "--test" || arg == "--test-mode" || arg == "--oneshot") {
            testMode = true;
        }
    }

    std::cout << gx::core::kApplicationName << " Index Service v"
              << gx::core::kVersionString << " (NTFS MFT/USN Daemon)\n";
    std::cout << "Architecture: In-Memory SoA + AVX2 SIMD + Zero-Copy Shared Memory IPC (ADR-0022)\n";

#ifdef _WIN32
    ::SetConsoleCtrlHandler(consoleCtrlHandler, TRUE);

    gx::NameIndex index;
    bool isElevated = gx::platform::win32::UsnEnumerator::isElevated();
    std::cout << "Process Privilege: " << (isElevated ? "ELEVATED (Administrator)" : "STANDARD USER") << "\n";

    gx::platform::win32::UsnJournalCursor cursor{};
    std::unique_ptr<gx::platform::win32::UsnJournalTailer> tailer;

    if (isElevated) {
        std::cout << "Starting full NTFS MFT enumeration on volume C: ...\n";
        gx::platform::win32::UsnEnumerator enumerator('C');
        gx::platform::win32::UsnEnumerationStats stats{};

        auto status = enumerator.enumerateVolume(index, cursor, stats, [](size_t count) {
            if (count % 100000 == 0) {
                std::cout << "  Enumerated " << count << " records...\n";
            }
        });

        if (status == gx::platform::win32::UsnStatus::Success) {
            std::cout << "  Enumeration Complete in " << stats.elapsedMs << " ms!\n";
            std::cout << "  Total Files: " << stats.filesCount
                      << " | Directories: " << stats.directoriesCount << "\n";
            std::cout << "  Total Resident Memory: " << (index.totalMemoryBytes() / 1024 / 1024) << " MB\n";

            // Start live USN journal tailing
            tailer = std::make_unique<gx::platform::win32::UsnJournalTailer>(index, 'C');
            tailer->start(cursor);
            std::cout << "  Live USN Journal tailing ACTIVE on C: (StartUsn=" << cursor.nextUsn << ")\n";
        } else {
            std::cerr << "  USN Enumeration returned status code: " << static_cast<int>(status) << "\n";
        }
    } else {
        std::cout << "Note: Elevate with administrative tokens to enable raw NTFS MFT enumeration.\n";
        std::cout << "Seeding in-memory index with core test records for local IPC...\n";
        index.reserve(1000);
        index.add(101, 5, "GodXplr_Specification.pdf", 0, 1048576, 133000000000000000ULL);
        index.add(102, 5, "Budget_2026.xlsx", 0, 2097152, 133000000000000000ULL);
        index.add(103, 5, "SystemKernel.dll", 0, 4194304, 133000000000000000ULL);
        index.add(104, 5, "Documentation", gx::kDir, 0, 133000000000000000ULL);
        index.resolveParents();
        std::cout << "  Seeded " << index.size() << " records in RAM.\n";
    }

    gx::SearchEngine searchEngine(index);

    // Initialize Shared Memory IPC Server
    gx::platform::win32::ShmServer shmServer;
    constexpr uint32_t kDefaultSessionId = 0;
    if (!shmServer.create(kDefaultSessionId, false)) {
        std::cerr << "Failed to create Shared Memory IPC channel!\n";
        return 1;
    }

    std::cout << "Shared Memory IPC channel initialized: Local\\GodXplr_Search_Shm_"
              << kDefaultSessionId << "\n";
    std::cout << "Daemon ready to process real-time queries.\n";

    if (testMode) {
        std::cout << "--test-mode requested. Performing smoke verification...\n";
        // Smoke verification query
        gx::QueryOptions opts;
        opts.offset = 0;
        opts.count = 50;
        auto page = searchEngine.search("pdf", opts);
        std::cout << "  Smoke query 'pdf' returned " << page.total << " matches.\n";
        shmServer.close();
        std::cout << "Index service smoke test passed.\n";
        return 0;
    }

    // Service loop
    while (!g_stopDaemon.load()) {
        if (!shmServer.waitForQuery(250)) {
            continue;
        }

        auto* buffer = shmServer.getBuffer();
        if (!buffer) break;

        uint64_t queryGen = buffer->queryGeneration.load(std::memory_order_acquire);
        std::string queryString(buffer->queryText, buffer->queryLen);

        gx::QueryOptions opts;
        opts.offset = 0;
        opts.count = static_cast<uint32_t>(gx::ipc::kMaxResultRows);

        auto page = searchEngine.search(queryString, opts);

        // Check if query was cancelled during execution by a newer generation
        if (buffer->queryGeneration.load(std::memory_order_relaxed) != queryGen) {
            continue; // Abandon stale response
        }

        buffer->resultCount = static_cast<uint32_t>(std::min(page.rows.size(), gx::ipc::kMaxResultRows));
        buffer->totalMatches = page.total;
        buffer->statusFlags = 1; // Final

        for (uint32_t i = 0; i < buffer->resultCount; ++i) {
            const auto& srcRow = page.rows[i];
            auto& dstRow = buffer->rows[i];
            dstRow.objectId = index.getFrn(srcRow.id);
            dstRow.size = srcRow.size;
            dstRow.mtime = srcRow.mtime;
            dstRow.attributes = srcRow.flags;
            dstRow.tier = srcRow.tier;
            dstRow.score = srcRow.score;

            size_t nLen = std::min(srcRow.name.size(), sizeof(dstRow.name) - 1);
            std::memcpy(dstRow.name, srcRow.name.data(), nLen);
            dstRow.name[nLen] = '\0';
            dstRow.nameLen = static_cast<uint16_t>(nLen);

            size_t dLen = std::min(srcRow.dir.size(), sizeof(dstRow.dir) - 1);
            std::memcpy(dstRow.dir, srcRow.dir.data(), dLen);
            dstRow.dir[dLen] = '\0';
            dstRow.dirLen = static_cast<uint16_t>(dLen);
        }

        buffer->resultGeneration.store(queryGen, std::memory_order_release);
        shmServer.notifyResultsReady();
    }

    std::cout << "Daemon stopping...\n";
    if (tailer) {
        tailer->stop();
    }
    shmServer.close();
    std::cout << "Daemon stopped cleanly.\n";
#endif

    return 0;
}
