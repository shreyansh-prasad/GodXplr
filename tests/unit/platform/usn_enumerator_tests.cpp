#include "godxplr/platform/usn_enumerator.h"
#include "godxplr/platform/usn_tail.h"
#include <iostream>
#include <cstdlib>

#define TEST_ASSERT(cond)                                                      \
    do {                                                                       \
        if (!(cond)) {                                                         \
            std::cerr << "Assertion failed: " #cond " at " << __FILE__         \
                      << ":" << __LINE__ << std::endl;                         \
            std::abort();                                                      \
        }                                                                      \
    } while (0)

using namespace gx;
using namespace gx::platform::win32;

void testElevationQuery() {
    std::cout << "[Test] ElevationQuery...\n";
    bool elevated = UsnEnumerator::isElevated();
    std::cout << "  Process elevated token: " << (elevated ? "YES" : "NO") << "\n";
    std::cout << "  -> Passed!\n";
}

void testQueryCursor() {
    std::cout << "[Test] QueryCursor...\n";
    UsnEnumerator enumerator('C');
    UsnJournalCursor cursor{};
    UsnStatus status = enumerator.queryCursor(cursor);

    if (status == UsnStatus::Success) {
        std::cout << "  Successfully queried C: journal. ID=" << cursor.journalId
                  << " NextUsn=" << cursor.nextUsn << "\n";
        TEST_ASSERT(cursor.journalId != 0);
    } else if (status == UsnStatus::AccessDenied) {
        std::cout << "  Access denied (expected in non-elevated user context).\n";
    } else {
        std::cout << "  Status code: " << static_cast<int>(status) << "\n";
    }

    // Invalid drive letter should fail with InvalidHandle
    UsnEnumerator invalidEnum('?');
    UsnJournalCursor invCursor{};
    UsnStatus invStatus = invalidEnum.queryCursor(invCursor);
    TEST_ASSERT(invStatus == UsnStatus::InvalidHandle);

    std::cout << "  -> Passed!\n";
}

void testTailerLifecycle() {
    std::cout << "[Test] TailerLifecycle...\n";
    NameIndex index;
    index.reserve(1000);

    UsnJournalTailer tailer(index, 'C');
    TEST_ASSERT(!tailer.isRunning());

    UsnJournalCursor dummyCursor{12345, 100, 0};
    // Starting on non-elevated volume will simply exit or tail gracefully
    tailer.start(dummyCursor);
    // Let worker thread start
    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    // Stop must cleanly join without hanging
    tailer.stop();
    TEST_ASSERT(!tailer.isRunning());

    std::cout << "  -> Passed!\n";
}

void testIndexMutationDeltaApplication() {
    std::cout << "[Test] IndexMutationDeltaApplication...\n";
    NameIndex index;
    index.reserve(1000);

    // Seed file
    EntryId id = index.add(1001, 5, "document.txt", 0, 1024, 100000);
    TEST_ASSERT(id != kNoEntry);
    TEST_ASSERT(index.findByFrn(1001) == id);
    TEST_ASSERT(index.getName(id) == "document.txt");

    // Test rename
    index.rename(id, "document_v2.txt");
    TEST_ASSERT(index.getName(id) == "document_v2.txt");

    // Test move by FRN
    index.add(2002, 5, "archive", kDir, 0, 100000);
    EntryId archiveId = index.findByFrn(2002);
    TEST_ASSERT(archiveId != kNoEntry);

    index.moveByFrn(id, 2002);
    TEST_ASSERT(index.getParent(id) == archiveId);

    // Test metadata update
    index.updateMetadata(id, 2048, 200000, 0);
    TEST_ASSERT(index.getFileSize(id) == 2048);
    TEST_ASSERT(index.getMtime(id) == 200000);

    // Test delete
    index.remove(id);
    TEST_ASSERT(index.isDeleted(id));
    TEST_ASSERT(index.findByFrn(1001) == kNoEntry);

    std::cout << "  -> Passed!\n";
}

int main() {
    std::cout << "======================================\n";
    std::cout << "Running GodXplr UsnEnumerator Tests   \n";
    std::cout << "======================================\n";

    testElevationQuery();
    testQueryCursor();
    testTailerLifecycle();
    testIndexMutationDeltaApplication();

    std::cout << "All UsnEnumerator tests passed successfully!\n";
    return 0;
}
