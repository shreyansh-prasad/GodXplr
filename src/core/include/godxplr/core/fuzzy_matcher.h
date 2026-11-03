#pragma once

#include <string_view>
#include <cstdint>
#include <vector>
#include <optional>

namespace gx::search {

struct FuzzyMatchResult {
    int32_t score{0};
    bool matched{false};
    std::vector<uint32_t> matchPositions;
};

class FuzzyMatcher {
public:
    // Calculates a 32-bit letter bitmask (bits 0-25 for 'a'-'z', bit 26 for '0'-'9', bit 27 for other)
    static uint32_t computeLetterMask(std::string_view text);

    // Fast bitmask prefilter check: returns true if candidate contains all required letters of query
    static bool passesLetterMask(uint32_t candidateMask, uint32_t queryMask) noexcept {
        return (candidateMask & queryMask) == queryMask;
    }

    // Evaluates fzf-style fuzzy subsequence match with bonus scoring:
    // - Base score: +16 per matched char
    // - Consecutive match bonus: +8
    // - Word boundary / delimiter bonus: +12 (after '.', '_', '-', '/', '\\', ' ')
    // - CamelCase / digit transition bonus: +10
    // - Gap open penalty: -3
    // - Gap extend penalty: -1
    // - Length penalty: -1 per excess character
    static FuzzyMatchResult match(std::string_view candidate, std::string_view query);

    // Damerau-Levenshtein distance (insert, delete, replace, transpose) for typo tolerance
    static uint32_t damerauLevenshteinDistance(std::string_view s1, std::string_view s2, uint32_t maxDistance = 2);
};

} // namespace gx::search

