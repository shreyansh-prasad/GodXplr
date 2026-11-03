#include "godxplr/core/name_index.h"
#include "godxplr/core/simd_search.h"
#include <iostream>
#include <cstdlib>

#define TEST_ASSERT(cond) \
    do { \
        if (!(cond)) { \
            std::cerr << "Assertion failed: " #cond " at " << __FILE__ << ":" << __LINE__ << "\n"; \
            std::abort(); \
        } \
    } while(0)

void test_simd_basic_search() {
    gx::NameIndex index;
    index.add(1, 0, "kernel32.dll", 0);
    index.add(2, 0, "user32.dll", 0);
    index.add(3, 0, "gdi32.dll", 0);
    index.add(4, 0, "shell32.dll", 0);
    index.add(5, 0, "explorer.exe", 0);
    index.add(6, 0, "notepad.exe", 0);

    std::vector<gx::EntryId> hits;
    size_t count = gx::search::scanArenaAVX2(index, "32.dll", hits);
    TEST_ASSERT(count == 4);
    TEST_ASSERT(hits.size() == 4);

    hits.clear();
    count = gx::search::scanArenaAVX2(index, ".EXE", hits);
    TEST_ASSERT(count == 2);
    TEST_ASSERT(hits.size() == 2);

    hits.clear();
    count = gx::search::scanArenaAVX2(index, "nonexistent", hits);
    TEST_ASSERT(count == 0);
    TEST_ASSERT(hits.empty());

    std::cout << "[PASS] test_simd_basic_search\n";
}

void test_differential_scalar_vs_avx2() {
    gx::NameIndex index;
    for (int i = 0; i < 200; ++i) {
        std::string name = "file_" + std::to_string(i) + ((i % 2 == 0) ? ".cpp" : ".h");
        index.add(static_cast<uint64_t>(i + 1000), 0, name, 0);
    }
    index.add(5000, 0, "SUPER_SPECIAL_LOG.TXT", 0);
    index.add(5001, 0, "another_special_doc.docx", 0);

    const char* queries[] = {"special", "FILE_1", ".cpp", "99", ".txt", "xyz_not_here"};

    for (const char* q : queries) {
        std::vector<gx::EntryId> scalarHits;
        std::vector<gx::EntryId> avx2Hits;

        size_t sCount = gx::search::scanArenaScalar(index, q, scalarHits);
        size_t aCount = gx::search::scanArenaAVX2(index, q, avx2Hits);

        TEST_ASSERT(sCount == aCount);
        TEST_ASSERT(scalarHits.size() == avx2Hits.size());
        for (size_t i = 0; i < scalarHits.size(); ++i) {
            TEST_ASSERT(scalarHits[i] == avx2Hits[i]);
        }
    }

    std::cout << "[PASS] test_differential_scalar_vs_avx2\n";
}

void test_poisoned_exclusion() {
    gx::NameIndex index;
    gx::EntryId id1 = index.add(10, 0, "important_secret.txt", 0);
    index.add(20, 0, "other_secret.txt", 0);

    std::vector<gx::EntryId> hits;
    size_t count = gx::search::scanArenaAVX2(index, "secret", hits);
    TEST_ASSERT(count == 2);

    // Delete id1
    index.remove(id1);

    hits.clear();
    count = gx::search::scanArenaAVX2(index, "secret", hits);
    TEST_ASSERT(count == 1);
    TEST_ASSERT(hits[0] != id1);

    std::cout << "[PASS] test_poisoned_exclusion\n";
}

int main() {
    std::cout << "Running SIMD search tests...\n";
    test_simd_basic_search();
    test_differential_scalar_vs_avx2();
    test_poisoned_exclusion();
    std::cout << "All SIMD search tests passed successfully.\n";
    return 0;
}
