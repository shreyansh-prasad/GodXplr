#include "godxplr/core/fuzzy_matcher.h"
#include <algorithm>
#include <cctype>

namespace gx::search {

uint32_t FuzzyMatcher::computeLetterMask(std::string_view text) {
    uint32_t mask = 0;
    for (char c : text) {
        if (c >= 'a' && c <= 'z') {
            mask |= (1U << (c - 'a'));
        } else if (c >= 'A' && c <= 'Z') {
            mask |= (1U << (c - 'A'));
        } else if (c >= '0' && c <= '9') {
            mask |= (1U << 26);
        } else {
            mask |= (1U << 27);
        }
    }
    return mask;
}

static inline bool isWordDelimiter(char c) noexcept {
    return c == '.' || c == '_' || c == '-' || c == '/' || c == '\\' || c == ' ' || c == ':';
}

FuzzyMatchResult FuzzyMatcher::match(std::string_view candidate, std::string_view query) {
    FuzzyMatchResult res;
    if (query.empty()) {
        res.matched = true;
        return res;
    }
    if (candidate.size() < query.size()) {
        return res;
    }

    size_t qIdx = 0;
    int32_t score = 0;
    int32_t consecutiveBonus = 0;
    bool inGap = false;

    res.matchPositions.reserve(query.size());

    for (size_t cIdx = 0; cIdx < candidate.size() && qIdx < query.size(); ++cIdx) {
        char candChar = static_cast<char>(std::tolower(static_cast<unsigned char>(candidate[cIdx])));
        char queryChar = static_cast<char>(std::tolower(static_cast<unsigned char>(query[qIdx])));

        if (candChar == queryChar) {
            res.matchPositions.push_back(static_cast<uint32_t>(cIdx));
            score += 16; // Base character match

            if (res.matchPositions.size() > 1 && res.matchPositions.back() == res.matchPositions[res.matchPositions.size() - 2] + 1) {
                consecutiveBonus += 8;
                score += consecutiveBonus;
            } else {
                consecutiveBonus = 0;
            }

            // Word boundary bonus
            if (cIdx == 0 || isWordDelimiter(candidate[cIdx - 1])) {
                score += 12;
            } else if (std::isupper(static_cast<unsigned char>(candidate[cIdx])) &&
                       std::islower(static_cast<unsigned char>(candidate[cIdx - 1]))) {
                score += 10; // CamelCase transition
            }

            inGap = false;
            qIdx++;
        } else {
            if (!res.matchPositions.empty()) {
                if (!inGap) {
                    score -= 3; // Gap open penalty
                    inGap = true;
                } else {
                    score -= 1; // Gap extend penalty
                }
            }
            consecutiveBonus = 0;
        }
    }

    if (qIdx == query.size()) {
        res.matched = true;
        // Mild penalty for excess length to prefer more concise matches
        int32_t lenDiff = static_cast<int32_t>(candidate.size() - query.size());
        score -= std::min(lenDiff, 32);
        res.score = score;
    }

    return res;
}

uint32_t FuzzyMatcher::damerauLevenshteinDistance(std::string_view s1, std::string_view s2, uint32_t maxDistance) {
    const size_t len1 = s1.size();
    const size_t len2 = s2.size();

    if (len1 == 0) return static_cast<uint32_t>(len2);
    if (len2 == 0) return static_cast<uint32_t>(len1);

    if (std::abs(static_cast<int>(len1) - static_cast<int>(len2)) > static_cast<int>(maxDistance)) {
        return maxDistance + 1;
    }

    // Dynamic programming matrix with 3 rows (current, previous, before-previous)
    std::vector<uint32_t> d0(len2 + 1, 0);
    std::vector<uint32_t> d1(len2 + 1, 0);
    std::vector<uint32_t> d2(len2 + 1, 0);

    for (size_t j = 0; j <= len2; ++j) {
        d1[j] = static_cast<uint32_t>(j);
    }

    for (size_t i = 1; i <= len1; ++i) {
        d0[0] = static_cast<uint32_t>(i);

        for (size_t j = 1; j <= len2; ++j) {
            uint32_t cost = (std::tolower(static_cast<unsigned char>(s1[i - 1])) ==
                             std::tolower(static_cast<unsigned char>(s2[j - 1]))) ? 0 : 1;

            uint32_t deletion = d1[j] + 1;
            uint32_t insertion = d0[j - 1] + 1;
            uint32_t substitution = d1[j - 1] + cost;

            d0[j] = std::min({deletion, insertion, substitution});

            // Transposition check
            if (i > 1 && j > 1 &&
                std::tolower(static_cast<unsigned char>(s1[i - 1])) == std::tolower(static_cast<unsigned char>(s2[j - 2])) &&
                std::tolower(static_cast<unsigned char>(s1[i - 2])) == std::tolower(static_cast<unsigned char>(s2[j - 1]))) {
                d0[j] = std::min(d0[j], d2[j - 2] + 1);
            }
        }

        d2 = d1;
        d1 = d0;
    }

    return d1[len2];
}

} // namespace gx::search

