#pragma once

#include "godxplr/core/entry_key.h"
#include <cstdint>
#include <string_view>
#include <cmath>

namespace gx::search {

enum class MatchTier : uint8_t {
    ExactMatch = 0,             // Exact filename match
    PrefixMatch = 1,            // Name starts with query
    WordBoundarySubstring = 2,  // Substring starting at word/delimiter boundary
    GeneralSubstring = 3,       // Arbitrary substring match
    FuzzyMatch = 4              // Fuzzy subsequence / typo match
};

struct RankingWeights {
    double w_recency{3.0};      // Weight for recency (decay half-life 14 days)
    double w_frequency{2.0};    // Weight for frequency: log1p(openCount)
    double w_depth{0.5};        // Weight for shallow paths: 1 / (1 + depth)
    double w_length{0.01};      // Penalty for long names
};

struct EntryRankingFeatures {
    MatchTier tier{MatchTier::GeneralSubstring};
    int32_t matchScore{0};
    uint64_t lastWriteTime{0};  // Windows FILETIME or 100ns units
    uint32_t openCount{0};
    uint32_t pathDepth{1};
    uint32_t nameLength{0};
};

class SearchRanker {
public:
    explicit SearchRanker(RankingWeights weights = {}) : weights_(weights) {}

    // Computes composite score for ranking entries within their tier.
    // Score ensures Tier 0 > Tier 1 > Tier 2 > Tier 3 > Tier 4 strictly.
    [[nodiscard]] double computeScore(const EntryRankingFeatures& features, uint64_t currentTimestamp = 0) const;

    // Determines the match tier of candidate given the query term
    static MatchTier classifyMatchTier(std::string_view foldedName, std::string_view foldedQuery, bool isFuzzy = false);

private:
    RankingWeights weights_;
};

} // namespace gx::search
