#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include <cstdlib>

#include "godxplr/core/entry_key.h"
#include "godxplr/core/name_index.h"
#include "godxplr/core/search_engine.h"
#include "godxplr/core/query_parser.h"
#include "godxplr/core/fuzzy_matcher.h"
#include "godxplr/core/pixel_buffer.h"

#define TEST_ASSERT(cond) \
    do { \
        if (!(cond)) { \
            std::cerr << "Assertion failed at line " << __LINE__ << ": " #cond "\n"; \
            std::abort(); \
        } \
    } while (0)

using namespace gx;

void testEntryKeyPortability() {
    std::cout << "[Test] Portable EntryKey binary layout...\n";

    EntryKey k1{ProviderKind::NtfsVolume, 0x1234, 1001, 1};
    EntryKey k2{ProviderKind::NtfsVolume, 0x1234, 1001, 1};
    EntryKey k3{ProviderKind::NetworkShare, 0x5678, 2002, 1};

    TEST_ASSERT(k1 == k2);
    TEST_ASSERT(k1 != k3);
    TEST_ASSERT(k1 < k3);

    std::hash<EntryKey> hasher;
    TEST_ASSERT(hasher(k1) == hasher(k2));

    std::cout << "  -> PASS\n";
}

void testFuzzyMatcherPortability() {
    std::cout << "[Test] Portable FuzzyMatcher fzf scoring & Damerau-Levenshtein...\n";

    // Subsequence match test
    auto res1 = search::FuzzyMatcher::match("godxplr_virtual_filesystem.cpp", "gxfs");
    TEST_ASSERT(res1.matched);
    TEST_ASSERT(res1.score > 0);

    // No match
    auto res2 = search::FuzzyMatcher::match("notepad.exe", "calc");
    TEST_ASSERT(!res2.matched);

    // Damerau-Levenshtein typo tolerance
    uint32_t dist1 = search::FuzzyMatcher::damerauLevenshteinDistance("explorer", "xplorer");
    TEST_ASSERT(dist1 == 1); // 1 deletion

    uint32_t dist2 = search::FuzzyMatcher::damerauLevenshteinDistance("godxplr", "goxdplr");
    TEST_ASSERT(dist2 == 1); // 1 transposition ('d' and 'x')

    std::cout << "  -> PASS\n";
}

void testPixelBufferDecompressionSafety() {
    std::cout << "[Test] Portable PixelBuffer memory boundaries...\n";

    core::PixelBuffer buf;
    // Normal allocation
    TEST_ASSERT(buf.allocate(256, 256));
    TEST_ASSERT(buf.width() == 256);
    TEST_ASSERT(buf.height() == 256);
    TEST_ASSERT(buf.byteCount() == 256 * 256 * 4);

    // Dimension bomb (> 8K dimension limit)
    core::PixelBuffer bomb1;
    TEST_ASSERT(!bomb1.allocate(16384, 100));

    // Memory bomb (> 32 MB safety limit)
    core::PixelBuffer bomb2;
    TEST_ASSERT(!bomb2.allocate(8000, 8000)); // 8000 * 8000 * 4 = 256 MB > 32 MB

    std::cout << "  -> PASS\n";
}

int main() {
    std::cout << "==============================================\n";
    std::cout << "  GodXplr Spec 10 Core Portability Validation \n";
    std::cout << "==============================================\n";

    testEntryKeyPortability();
    testFuzzyMatcherPortability();
    testPixelBufferDecompressionSafety();

    std::cout << "==============================================\n";
    std::cout << "  ALL PORTABILITY TESTS PASSED (100%)         \n";
    std::cout << "==============================================\n";
    return 0;
}

