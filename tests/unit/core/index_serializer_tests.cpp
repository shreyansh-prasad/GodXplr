#include "godxplr/core/index_serializer.h"
#include "godxplr/core/search_engine.h"
#include <iostream>
#include <cstdlib>
#include <filesystem>
#include <fstream>

#define TEST_ASSERT(cond)                                                      \
    do {                                                                       \
        if (!(cond)) {                                                         \
            std::cerr << "Assertion failed: " #cond " at " << __FILE__         \
                      << ":" << __LINE__ << std::endl;                         \
            std::abort();                                                      \
        }                                                                      \
    } while (0)

using namespace gx;

void testSaveAndLoadSnapshot() {
    std::cout << "[Test] SaveAndLoadSnapshot...\n";

    NameIndex original;
    original.reserve(100);

    // Populate sample entries
    EntryId dirRoot = original.add(5, 5, "C:", kDir, 0, 100000);
    EntryId dirWindows = original.add(100, 5, "Windows", kDir, 0, 100000);
    EntryId dirSystem32 = original.add(101, 100, "System32", kDir, 0, 100000);
    EntryId fileKernel = original.add(102, 101, "ntoskrnl.exe", 0, 10485760, 1330000000ULL);
    EntryId fileConfig = original.add(103, 100, "win.ini", 0, 512, 1320000000ULL);

    (void)dirRoot;
    (void)dirWindows;
    (void)dirSystem32;
    (void)fileKernel;
    (void)fileConfig;

    original.resolveParents();

    std::string snapPath = "test_snapshot.gxi";
    uint32_t volSerial = 0xABCD1234;
    uint64_t journalId = 0x9988776655443322ULL;
    uint64_t nextUsn = 543210;

    bool saved = IndexSerializer::save(original, volSerial, journalId, nextUsn, snapPath);
    TEST_ASSERT(saved);
    TEST_ASSERT(std::filesystem::exists(snapPath));

    NameIndex restored;
    uint32_t loadedVolSerial = 0;
    uint64_t loadedJournalId = 0;
    uint64_t loadedNextUsn = 0;

    bool loaded = IndexSerializer::load(restored, loadedVolSerial, loadedJournalId, loadedNextUsn, snapPath);
    TEST_ASSERT(loaded);

    // Verify metadata
    TEST_ASSERT(loadedVolSerial == volSerial);
    TEST_ASSERT(loadedJournalId == journalId);
    TEST_ASSERT(loadedNextUsn == nextUsn);
    TEST_ASSERT(restored.size() == original.size());

    // Verify entries and resolution
    EntryId rKernel = restored.findByFrn(102);
    TEST_ASSERT(rKernel != kNoEntry);
    TEST_ASSERT(restored.getName(rKernel) == "ntoskrnl.exe");
    TEST_ASSERT(restored.getFileSize(rKernel) == 10485760);
    TEST_ASSERT(restored.getMtime(rKernel) == 1330000000ULL);

    // Verify path resolution
    std::string path = restored.buildPath(rKernel);
    TEST_ASSERT(path.find("Windows/System32/ntoskrnl.exe") != std::string::npos ||
                path.find("Windows\\System32\\ntoskrnl.exe") != std::string::npos);

    // Verify search queries work identically on restored index
    SearchEngine engine(restored);
    QueryOptions opts;
    auto page = engine.search("ntoskrnl", opts);
    TEST_ASSERT(page.total == 1);
    TEST_ASSERT(page.rows[0].name == "ntoskrnl.exe");

    std::filesystem::remove(snapPath);
    std::cout << "  -> Passed!\n";
}

void testCorruptedSnapshotRejection() {
    std::cout << "[Test] CorruptedSnapshotRejection...\n";

    NameIndex original;
    original.reserve(50);
    original.add(1, 0, "file1.txt", 0, 100, 100);
    original.add(2, 0, "file2.txt", 0, 200, 200);

    std::string snapPath = "corrupt_test.gxi";
    TEST_ASSERT(IndexSerializer::save(original, 1, 2, 3, snapPath));

    // Corrupt one byte in the payload
    {
        std::fstream file(snapPath, std::ios::in | std::ios::out | std::ios::binary);
        file.seekp(sizeof(SnapshotHeader) + 10);
        char badByte = static_cast<char>(0xFF);
        file.write(&badByte, 1);
    }

    NameIndex restored;
    uint32_t volSerial = 0;
    uint64_t journalId = 0;
    uint64_t nextUsn = 0;

    bool loaded = IndexSerializer::load(restored, volSerial, journalId, nextUsn, snapPath);
    // Must reject due to CRC32 mismatch
    TEST_ASSERT(!loaded);

    std::filesystem::remove(snapPath);
    std::cout << "  -> Passed!\n";
}

int main() {
    std::cout << "========================================\n";
    std::cout << "Running GodXplr IndexSerializer Tests   \n";
    std::cout << "========================================\n";

    testSaveAndLoadSnapshot();
    testCorruptedSnapshotRejection();

    std::cout << "All IndexSerializer tests passed successfully!\n";
    return 0;
}

