#include "godxplr/core/fuzzy_matcher.h"
#include "godxplr/core/ranking.h"
#include "godxplr/core/usage_db.h"
#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include <cstdlib>

#define TEST_ASSERT(cond) do { \
    if (!(cond)) { \
        std::cerr << "Assertion failed: " #cond " at " << __FILE__ << ":" << __LINE__ << std::endl; \
        std::abort(); \
    } \
} while (0)

using namespace gx::search;

void testLetterMaskPrefilter() {
    std::cout << "[Test] Letter Mask Prefilter..." << std::endl;

    uint32_t maskAbc = FuzzyMatcher::computeLetterMask("abc");
    TEST_ASSERT((maskAbc & (1U << 0)) != 0); // 'a'
    TEST_ASSERT((maskAbc & (1U << 1)) != 0); // 'b'
    TEST_ASSERT((maskAbc & (1U << 2)) != 0); // 'c'
    TEST_ASSERT((maskAbc & (1U << 3)) == 0); // 'd'

    uint32_t maskAlphabet = FuzzyMatcher::computeLetterMask("alphabet_soup_123");
    TEST_ASSERT(FuzzyMatcher::passesLetterMask(maskAlphabet, maskAbc) == false); // missing 'c'

    uint32_t maskCab = FuzzyMatcher::computeLetterMask("taxi_cab.png");
    TEST_ASSERT(FuzzyMatcher::passesLetterMask(maskCab, maskAbc) == true); // contains a, b, c
}

void testFuzzySubsequenceScoring() {
    std::cout << "[Test] Fuzzy Subsequence Scoring..." << std::endl;

    // Direct subsequence match
    auto res1 = FuzzyMatcher::match("calculator.cpp", "calc");
    TEST_ASSERT(res1.matched);
    TEST_ASSERT(res1.score > 0);
    TEST_ASSERT(res1.matchPositions.size() == 4);

    // Non-matching subsequence
    auto res2 = FuzzyMatcher::match("document.pdf", "xyz");
    TEST_ASSERT(!res2.matched);

    // Word boundary bonus: "fldr" in "my_folder.cpp" vs "folderless.cpp"
    auto resBoundary = FuzzyMatcher::match("my_folder.cpp", "fldr");
    auto resInternal = FuzzyMatcher::match("affllddrr.cpp", "fldr");
    TEST_ASSERT(resBoundary.matched && resInternal.matched);
    TEST_ASSERT(resBoundary.score > resInternal.score); // Boundary bonus beats internal gap

    // Shorter filename preference
    auto resShort = FuzzyMatcher::match("calc.cpp", "calc");
    auto resLong = FuzzyMatcher::match("calculator_specification_v2.cpp", "calc");
    TEST_ASSERT(resShort.matched && resLong.matched);
    TEST_ASSERT(resShort.score > resLong.score); // Length penalty on excess characters
}

void testDamerauLevenshteinDistance() {
    std::cout << "[Test] Damerau-Levenshtein Distance..." << std::endl;

    // Identical
    TEST_ASSERT(FuzzyMatcher::damerauLevenshteinDistance("apple", "apple") == 0);

    // Adjacent transposition (counts as 1 operation in Damerau-Levenshtein)
    TEST_ASSERT(FuzzyMatcher::damerauLevenshteinDistance("teh", "the") == 1);
    TEST_ASSERT(FuzzyMatcher::damerauLevenshteinDistance("recieve", "receive") == 1);

    // Single insertion
    TEST_ASSERT(FuzzyMatcher::damerauLevenshteinDistance("aple", "apple") == 1);

    // Single deletion
    TEST_ASSERT(FuzzyMatcher::damerauLevenshteinDistance("apple", "aple") == 1);

    // Single substitution
    TEST_ASSERT(FuzzyMatcher::damerauLevenshteinDistance("cat", "bat") == 1);

    // Cutoff on distance > maxDistance
    TEST_ASSERT(FuzzyMatcher::damerauLevenshteinDistance("completely_different", "abc", 2) > 2);
}

void testMultiTierRankingStrictOrdering() {
    std::cout << "[Test] Multi-Tier Ranking Strict Ordering (ADR-0016)..." << std::endl;

    SearchRanker ranker;

    // Test tier classification
    TEST_ASSERT(SearchRanker::classifyMatchTier("calc.exe", "calc.exe") == MatchTier::ExactMatch);
    TEST_ASSERT(SearchRanker::classifyMatchTier("calc_helper.dll", "calc") == MatchTier::PrefixMatch);
    TEST_ASSERT(SearchRanker::classifyMatchTier("my_calc_tool.exe", "calc") == MatchTier::WordBoundarySubstring);
    TEST_ASSERT(SearchRanker::classifyMatchTier("recalculation.doc", "calc") == MatchTier::GeneralSubstring);
    TEST_ASSERT(SearchRanker::classifyMatchTier("calculator.exe", "clc", true) == MatchTier::FuzzyMatch);

    // Strict tier ordering verification: Tier 0 > Tier 1 > Tier 2 > Tier 3 > Tier 4
    EntryRankingFeatures f0{MatchTier::ExactMatch, 100, 0, 0, 1, 8};
    EntryRankingFeatures f1{MatchTier::PrefixMatch, 100, 0, 0, 1, 10};
    EntryRankingFeatures f2{MatchTier::WordBoundarySubstring, 100, 0, 0, 1, 12};
    EntryRankingFeatures f3{MatchTier::GeneralSubstring, 100, 0, 0, 1, 14};
    EntryRankingFeatures f4{MatchTier::FuzzyMatch, 100, 0, 0, 1, 14};

    double s0 = ranker.computeScore(f0);
    double s1 = ranker.computeScore(f1);
    double s2 = ranker.computeScore(f2);
    double s3 = ranker.computeScore(f3);
    double s4 = ranker.computeScore(f4);

    TEST_ASSERT(s0 > s1);
    TEST_ASSERT(s1 > s2);
    TEST_ASSERT(s2 > s3);
    TEST_ASSERT(s3 > s4);

    // CRITICAL ADR-0016 verification: Even with maximum recency, high frequency, and shallow depth,
    // a fuzzy match MUST NEVER outrank a general substring match!
    EntryRankingFeatures fuzzyMaxed{
        MatchTier::FuzzyMatch,
        500, // Very high match score
        133000000000000000ULL, // Recent
        1000, // Opened 1000 times!
        1,    // Shallow
        4
    };
    EntryRankingFeatures substringCold{
        MatchTier::GeneralSubstring,
        10,
        100000000000000000ULL, // Very old
        0, // Never opened
        10, // Deep
        50
    };

    uint64_t now = 133000000000000000ULL;
    double fuzzyScore = ranker.computeScore(fuzzyMaxed, now);
    double substringScore = ranker.computeScore(substringCold, now);

    TEST_ASSERT(substringScore > fuzzyScore); // Substring strictly outranks fuzzy
}

void testUsageTrackerLruAndHash() {
    std::cout << "[Test] Usage Tracker LRU & Path Hash..." << std::endl;

    // FNV-1a normalization
    uint64_t h1 = UsageTracker::hashPath("C:\\Windows\\System32\\notepad.exe");
    uint64_t h2 = UsageTracker::hashPath("c:/windows/system32/NOTEPAD.EXE");
    TEST_ASSERT(h1 == h2); // Case and slash normalized

    UsageTracker tracker(3); // Capacity 3
    TEST_ASSERT(tracker.size() == 0);

    tracker.recordOpen(h1, "note");
    TEST_ASSERT(tracker.size() == 1);

    auto entry = tracker.getUsage(h1);
    TEST_ASSERT(entry.has_value());
    TEST_ASSERT(entry->openCount == 1);
    TEST_ASSERT(entry->lastQueryPrefix == "note");

    // Second open increments count
    tracker.recordOpen(h1, "notepad");
    entry = tracker.getUsage(h1);
    TEST_ASSERT(entry.has_value());
    TEST_ASSERT(entry->openCount == 2);
    TEST_ASSERT(entry->lastQueryPrefix == "notepad");

    // Insert 2 more
    uint64_t hB = UsageTracker::hashPath("C:\\b.txt");
    uint64_t hC = UsageTracker::hashPath("C:\\c.txt");
    uint64_t hD = UsageTracker::hashPath("C:\\d.txt");

    tracker.recordOpen(hB);
    tracker.recordOpen(hC);
    TEST_ASSERT(tracker.size() == 3);

    // Access h1 to make hB the oldest
    tracker.getUsage(h1);

    // 4th insert evicts hB
    tracker.recordOpen(hD);
    TEST_ASSERT(tracker.size() == 3);
    TEST_ASSERT(!tracker.getUsage(hB).has_value()); // hB was evicted
    TEST_ASSERT(tracker.getUsage(h1).has_value());
    TEST_ASSERT(tracker.getUsage(hC).has_value());
    TEST_ASSERT(tracker.getUsage(hD).has_value());

    tracker.clear();
    TEST_ASSERT(tracker.size() == 0);
}

int main() {
    std::cout << "=== GodXplr Fuzzy, Ranking & Usage DB Tests (Spec 01 S10) ===" << std::endl;

    testLetterMaskPrefilter();
    testFuzzySubsequenceScoring();
    testDamerauLevenshteinDistance();
    testMultiTierRankingStrictOrdering();
    testUsageTrackerLruAndHash();

    std::cout << "All Fuzzy, Ranking & Usage DB tests PASSED!" << std::endl;
    return 0;
}
