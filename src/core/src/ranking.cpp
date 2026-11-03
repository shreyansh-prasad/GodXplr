#include "godxplr/core/ranking.h"
#include <algorithm>

namespace gx::search {

static inline bool isWordBoundaryChar(char c) noexcept {
    return c == '.' || c == '_' || c == '-' || c == '/' || c == '\\' || c == ' ' || c == ':';
}

MatchTier SearchRanker::classifyMatchTier(
    std::string_view foldedName,
    std::string_view foldedQuery,
    bool isFuzzy)
{
    if (foldedName == foldedQuery) {
        return MatchTier::ExactMatch;
    }

    if (foldedName.starts_with(foldedQuery)) {
        return MatchTier::PrefixMatch;
    }

    size_t pos = foldedName.find(foldedQuery);
    if (pos != std::string_view::npos) {
        if (pos > 0 && isWordBoundaryChar(foldedName[pos - 1])) {
            return MatchTier::WordBoundarySubstring;
        }
        return MatchTier::GeneralSubstring;
    }

    return isFuzzy ? MatchTier::FuzzyMatch : MatchTier::GeneralSubstring;
}

double SearchRanker::computeScore(const EntryRankingFeatures& features, uint64_t currentTimestamp) const {
    // Base tier separation: Tier 0 (+40,000) > Tier 1 (+30,000) > Tier 2 (+20,000) > Tier 3 (+10,000) > Tier 4 (+0)
    // This strictly ensures that fuzzy matches NEVER outrank substring or exact matches (ADR-0016)
    const double baseTierScore = (4.0 - static_cast<double>(features.tier)) * 10000.0;

    double score = baseTierScore + static_cast<double>(features.matchScore);

    // Recency exponential decay (half-life of 14 days)
    if (currentTimestamp > 0 && currentTimestamp >= features.lastWriteTime && features.lastWriteTime > 0) {
        // Windows FILETIME is in 100-nanosecond intervals (864,000,000,000 per day)
        double ageDays = static_cast<double>(currentTimestamp - features.lastWriteTime) / 864000000000.0;
        score += weights_.w_recency * std::exp(-ageDays / 14.0);
    }

    // Frequency: log1p of open count
    if (features.openCount > 0) {
        score += weights_.w_frequency * std::log1p(static_cast<double>(features.openCount));
    }

    // Path depth: shallower paths preferred
    score += weights_.w_depth * (1.0 / (1.0 + static_cast<double>(features.pathDepth)));

    // Name length penalty: prefer shorter, concise filenames
    score -= weights_.w_length * static_cast<double>(features.nameLength);

    return score;
}

} // namespace gx::search
