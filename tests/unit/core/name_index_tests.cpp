#include "godxplr/core/name_index.h"
#include <iostream>
#include <cstdlib>

#define TEST_ASSERT(cond) \
    do { \
        if (!(cond)) { \
            std::cerr << "Assertion failed: " #cond " at " << __FILE__ << ":" << __LINE__ << "\n"; \
            std::abort(); \
        } \
    } while(0)

void test_add_and_lookup() {
    gx::NameIndex index;
    // Add root: C:
    gx::EntryId root = index.add(5, 5, "C:", gx::kDir);
    TEST_ASSERT(root != gx::kNoEntry);
    TEST_ASSERT(index.getName(root) == "C:");
    TEST_ASSERT(index.isDirectory(root));

    // Add child: Windows
    gx::EntryId win = index.add(100, 5, "Windows", gx::kDir);
    TEST_ASSERT(win != gx::kNoEntry);
    TEST_ASSERT(index.getName(win) == "Windows");
    TEST_ASSERT(index.getParent(win) == root);

    // Add child: System32
    gx::EntryId sys32 = index.add(200, 100, "System32", gx::kDir);
    TEST_ASSERT(sys32 != gx::kNoEntry);

    // Add file: notepad.exe
    gx::EntryId notepad = index.add(300, 200, "notepad.exe", 0, 256000, 13350000000ULL);
    TEST_ASSERT(notepad != gx::kNoEntry);
    TEST_ASSERT(!index.isDirectory(notepad));
    TEST_ASSERT(index.getFileSize(notepad) == 256000);

    // Verify path reconstruction
    std::string path = index.buildPath(notepad);
    TEST_ASSERT(path == "C:\\Windows\\System32\\notepad.exe");

    // Verify FRN lookup
    TEST_ASSERT(index.findByFrn(300) == notepad);
    TEST_ASSERT(index.findByFrn(999999) == gx::kNoEntry);

    std::cout << "[PASS] test_add_and_lookup\n";
}

void test_rename_and_move() {
    gx::NameIndex index;
    gx::EntryId root = index.add(5, 5, "C:", gx::kDir);
    gx::EntryId file = index.add(10, 5, "old_name.txt", 0);
    (void)root;

    TEST_ASSERT(index.getName(file) == "old_name.txt");
    TEST_ASSERT(index.buildPath(file) == "C:\\old_name.txt");

    // Rename
    index.rename(file, "new_name.log");
    TEST_ASSERT(index.getName(file) == "new_name.log");
    TEST_ASSERT(index.getFoldedName(file) == "new_name.log");
    TEST_ASSERT(index.buildPath(file) == "C:\\new_name.log");

    // Move
    gx::EntryId docs = index.add(20, 5, "Documents", gx::kDir);
    index.move(file, docs);
    TEST_ASSERT(index.buildPath(file) == "C:\\Documents\\new_name.log");

    std::cout << "[PASS] test_rename_and_move\n";
}

void test_remove_and_poison() {
    gx::NameIndex index;
    gx::EntryId f1 = index.add(10, 0, "secret.key", 0);
    TEST_ASSERT(!index.isDeleted(f1));

    index.remove(f1);
    TEST_ASSERT(index.isDeleted(f1));
    TEST_ASSERT(index.findByFrn(10) == gx::kNoEntry);

    // Verify arena is poisoned
    std::span<const char> folded = index.getFoldedArena();
    TEST_ASSERT(folded.size() > 0);
    TEST_ASSERT(folded[0] == 0x01);

    std::cout << "[PASS] test_remove_and_poison\n";
}

void test_case_folding_and_letters() {
    std::string folded = gx::NameIndex::foldCase("Document_2026.PDF");
    TEST_ASSERT(folded == "document_2026.pdf");

    uint32_t mask = gx::NameIndex::computeLetterMask(folded);
    TEST_ASSERT((mask & (1u << ('d' - 'a'))) != 0);
    TEST_ASSERT((mask & (1u << ('p' - 'a'))) != 0);
    TEST_ASSERT((mask & (1u << (26 + 2))) != 0); // '2'
    TEST_ASSERT((mask & (1u << ('z' - 'a'))) == 0); // 'z' not present

    std::cout << "[PASS] test_case_folding_and_letters\n";
}

int main() {
    std::cout << "Running NameIndex unit tests...\n";
    test_add_and_lookup();
    test_rename_and_move();
    test_remove_and_poison();
    test_case_folding_and_letters();
    std::cout << "All NameIndex unit tests passed successfully.\n";
    return 0;
}
