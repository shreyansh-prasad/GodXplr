#include "godxplr/fileops/operation_journal.h"
#include "godxplr/fileops/conflict_resolver.h"
#include "godxplr/fileops/streaming_file_executor.h"
#include "godxplr/fileops/fileops_coordinator.h"

#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <atomic>
#include <cstdlib>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#define TEST_ASSERT(cond) \
    do { \
        if (!(cond)) { \
            std::cerr << "Assertion failed at line " << __LINE__ << ": " #cond "\n"; \
            std::abort(); \
        } \
    } while (0)

using namespace gx::fileops;

class TempTestFixture {
public:
    TempTestFixture() {
        wchar_t tempPath[MAX_PATH];
        GetTempPathW(MAX_PATH, tempPath);
        root_ = std::filesystem::path(tempPath) / (L"GodXplr_OpsTests_" + std::to_wstring(GetTickCount64()));
        std::error_code ec;
        std::filesystem::create_directories(root_, ec);
    }

    ~TempTestFixture() {
        std::error_code ec;
        std::filesystem::remove_all(root_, ec);
    }

    [[nodiscard]] std::wstring getPath(const std::wstring& filename) const {
        return (root_ / filename).wstring();
    }

    void createFile(const std::wstring& filename, const std::string& content) {
        std::ofstream out(getPath(filename), std::ios::binary);
        out.write(content.data(), content.size());
    }

    std::string readFile(const std::wstring& filename) {
        std::ifstream in(getPath(filename), std::ios::binary);
        return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    }

    bool fileExists(const std::wstring& filename) {
        return GetFileAttributesW(getPath(filename).c_str()) != INVALID_FILE_ATTRIBUTES;
    }

private:
    std::filesystem::path root_;
};

void testCrc32() {
    std::cout << "[Test] Fast CRC32...\n";
    const std::string sample = "123456789";
    uint32_t crc = StreamingFileExecutor::computeCrc32(
        reinterpret_cast<const uint8_t*>(sample.data()), sample.size());
    // Known CRC-32 for "123456789" is 0xCBF43926
    TEST_ASSERT(crc == 0xCBF43926);
    std::cout << "  -> PASS\n";
}

void testAtomicSameVolumeMove(TempTestFixture& fix) {
    std::cout << "[Test] Atomic Same-Volume Move...\n";

    fix.createFile(L"src.txt", "Atomic rename payload content.");
    std::wstring srcPath = fix.getPath(L"src.txt");
    std::wstring dstPath = fix.getPath(L"dst.txt");

    TEST_ASSERT(StreamingFileExecutor::isSameVolume(srcPath, dstPath));

    auto res = StreamingFileExecutor::atomicSameVolumeMove(srcPath, dstPath, false);
    TEST_ASSERT(res == FileOpResult::Success);

    TEST_ASSERT(!fix.fileExists(L"src.txt"));
    TEST_ASSERT(fix.fileExists(L"dst.txt"));
    TEST_ASSERT(fix.readFile(L"dst.txt") == "Atomic rename payload content.");

    std::cout << "  -> PASS\n";
}

void testStreamingCopy(TempTestFixture& fix) {
    std::cout << "[Test] Streaming Copy with Preallocation & Verification...\n";

    std::string largeData(128 * 1024, 'X'); // 128 KB
    fix.createFile(L"source_large.bin", largeData);

    std::wstring srcPath = fix.getPath(L"source_large.bin");
    std::wstring dstPath = fix.getPath(L"dest_large.bin");

    uint32_t copiedCrc = 0;
    uint64_t bytesReported = 0;

    auto res = StreamingFileExecutor::streamingCopy(
        srcPath,
        dstPath,
        nullptr,
        nullptr,
        [&](uint64_t justWritten, uint64_t /*totalWritten*/, uint64_t /*fileSize*/) {
            bytesReported += justWritten;
        },
        &copiedCrc);

    TEST_ASSERT(res == FileOpResult::Success);
    TEST_ASSERT(bytesReported == largeData.size());
    TEST_ASSERT(fix.fileExists(L"dest_large.bin"));
    TEST_ASSERT(fix.readFile(L"dest_large.bin") == largeData);

    uint32_t expectedCrc = StreamingFileExecutor::computeCrc32(
        reinterpret_cast<const uint8_t*>(largeData.data()), largeData.size());
    TEST_ASSERT(copiedCrc == expectedCrc);

    std::cout << "  -> PASS\n";
}

void testConflictResolution(TempTestFixture& fix) {
    std::cout << "[Test] Conflict Resolution Policies...\n";

    fix.createFile(L"doc.txt", "Original Document");
    fix.createFile(L"doc_target.txt", "Existing Target");

    std::wstring srcPath = fix.getPath(L"doc.txt");
    std::wstring dstPath = fix.getPath(L"doc_target.txt");

    // 1. Skip policy
    std::wstring resolved;
    auto actSkip = ConflictResolver::resolve(srcPath, dstPath, ConflictPolicy::Skip, resolved);
    TEST_ASSERT(actSkip == ConflictAction::SkipFile);

    // 2. Replace policy
    auto actReplace = ConflictResolver::resolve(srcPath, dstPath, ConflictPolicy::Replace, resolved);
    TEST_ASSERT(actReplace == ConflictAction::ExecuteReplace);
    TEST_ASSERT(resolved == dstPath);

    // 3. KeepBoth policy
    auto actKeepBoth = ConflictResolver::resolve(srcPath, dstPath, ConflictPolicy::KeepBoth, resolved);
    TEST_ASSERT(actKeepBoth == ConflictAction::ExecuteKeepBoth);
    TEST_ASSERT(resolved.find(L"doc_target (2).txt") != std::wstring::npos);

    std::cout << "  -> PASS\n";
}

void testCancellationScrub(TempTestFixture& fix) {
    std::cout << "[Test] Cancellation & .gxpartial Scrubbing...\n";

    std::string data(256 * 1024, 'C');
    fix.createFile(L"cancel_source.bin", data);

    std::wstring srcPath = fix.getPath(L"cancel_source.bin");
    std::wstring dstPath = fix.getPath(L"cancel_dest.bin");

    std::atomic<bool> cancelToken{true}; // Cancelled immediately

    auto res = StreamingFileExecutor::streamingCopy(
        srcPath,
        dstPath,
        &cancelToken);

    TEST_ASSERT(res == FileOpResult::Cancelled);
    TEST_ASSERT(!fix.fileExists(L"cancel_dest.bin"));
    TEST_ASSERT(!fix.fileExists(L"cancel_dest.bin.gxpartial")); // Cleanly wiped!
    TEST_ASSERT(fix.fileExists(L"cancel_source.bin"));          // Source untouched!

    std::cout << "  -> PASS\n";
}

void testOperationJournalAndRecovery(TempTestFixture& fix) {
    std::cout << "[Test] Operation Journal & Interrupted Recovery...\n";

    std::wstring journalPath = fix.getPath(L"test_ops.journal");
    OperationJournal journal(journalPath);

    // 1. Log an in-progress operation with an orphaned .gxpartial file
    std::wstring tempFile = fix.getPath(L"orphan.txt.gxpartial");
    fix.createFile(L"orphan.txt.gxpartial", "Incomplete data before crash");
    TEST_ASSERT(fix.fileExists(L"orphan.txt.gxpartial"));

    JournalEntry entry;
    entry.opId = 42;
    entry.timestamp = 12345678;
    entry.type = OpType::Copy;
    entry.state = OpState::InProgress;
    entry.sourcePath = fix.getPath(L"orig.txt");
    entry.destPath = fix.getPath(L"orphan.txt");
    entry.tempPath = tempFile;
    entry.totalBytes = 1000;
    journal.logIntent(entry);

    // 2. Perform recovery
    auto rec = journal.recoverInterruptedOperations();
    TEST_ASSERT(rec.inProgressFound == 1);
    TEST_ASSERT(rec.rolledBackCount == 1);
    TEST_ASSERT(rec.tempFilesCleaned == 1);

    // Verify temp file scrubbed
    TEST_ASSERT(!fix.fileExists(L"orphan.txt.gxpartial"));

    // Verify journal updated on disk
    OperationJournal reloadedJournal(journalPath);
    auto recoveredEntry = reloadedJournal.getEntry(42);
    TEST_ASSERT(recoveredEntry.has_value());
    TEST_ASSERT(recoveredEntry->state == OpState::RolledBack);

    std::cout << "  -> PASS\n";
}

void testCoordinatorUndoRedo(TempTestFixture& fix) {
    std::cout << "[Test] FileOpsCoordinator Undo / Redo Pipeline...\n";

    auto journal = std::make_shared<OperationJournal>(fix.getPath(L"coord.journal"));
    FileOpsCoordinator coordinator(journal);

    fix.createFile(L"item.txt", "UndoRedo Test Content");

    std::wstring src = fix.getPath(L"item.txt");
    std::wstring dst = fix.getPath(L"item_copy.txt");

    // 1. Copy file
    auto resCopy = coordinator.copyFile(src, dst);
    TEST_ASSERT(resCopy == FileOpResult::Success);
    TEST_ASSERT(fix.fileExists(L"item_copy.txt"));
    TEST_ASSERT(coordinator.canUndo());

    // 2. Undo copy -> copied file is deleted
    auto resUndo = coordinator.undo();
    TEST_ASSERT(resUndo == FileOpResult::Success);
    TEST_ASSERT(!fix.fileExists(L"item_copy.txt"));
    TEST_ASSERT(coordinator.canRedo());

    // 3. Redo copy -> copied file is restored
    auto resRedo = coordinator.redo();
    TEST_ASSERT(resRedo == FileOpResult::Success);
    TEST_ASSERT(fix.fileExists(L"item_copy.txt"));

    // 4. Move file
    std::wstring dst2 = fix.getPath(L"item_moved.txt");
    auto resMove = coordinator.moveFile(dst, dst2);
    TEST_ASSERT(resMove == FileOpResult::Success);
    TEST_ASSERT(!fix.fileExists(L"item_copy.txt"));
    TEST_ASSERT(fix.fileExists(L"item_moved.txt"));

    // 5. Undo move -> moved back to item_copy.txt
    auto resUndoMove = coordinator.undo();
    TEST_ASSERT(resUndoMove == FileOpResult::Success);
    TEST_ASSERT(fix.fileExists(L"item_copy.txt"));
    TEST_ASSERT(!fix.fileExists(L"item_moved.txt"));

    std::cout << "  -> PASS\n";
}

void testPauseResume(TempTestFixture& fix) {
    std::cout << "[Test] Pause and Resume Synchronization...\n";

    FileOpsCoordinator coordinator;
    fix.createFile(L"p_src.txt", "Pause test data");

    coordinator.pause();
    auto p1 = coordinator.progress();
    TEST_ASSERT(p1.status == OperationStatus::Paused);

    coordinator.resume();
    auto p2 = coordinator.progress();
    TEST_ASSERT(p2.status == OperationStatus::Running);

    std::cout << "  -> PASS\n";
}

int main() {
    std::cout << "========================================\n";
    std::cout << "  GodXplr Spec 04 File Operations Tests \n";
    std::cout << "========================================\n";

    TempTestFixture fixture;

    testCrc32();
    testAtomicSameVolumeMove(fixture);
    testStreamingCopy(fixture);
    testConflictResolution(fixture);
    testCancellationScrub(fixture);
    testOperationJournalAndRecovery(fixture);
    testCoordinatorUndoRedo(fixture);
    testPauseResume(fixture);

    std::cout << "========================================\n";
    std::cout << "  ALL FILE OPS TESTS PASSED (100%)      \n";
    std::cout << "========================================\n";
    return 0;
}

