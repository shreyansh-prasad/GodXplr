#pragma once

#include "godxplr/core/name_index.h"
#include <string_view>
#include <vector>

namespace gx::search {

// Performs an AVX2-accelerated substring scan over the case-folded arena.
// Collects matching EntryIds into outIds up to maxMatches.
size_t scanArenaAVX2(const NameIndex& index,
                     std::string_view needle,
                     std::vector<EntryId>& outIds,
                     size_t maxMatches = 1000);

// Scalar reference scan for differential correctness testing and fallback.
size_t scanArenaScalar(const NameIndex& index,
                       std::string_view needle,
                       std::vector<EntryId>& outIds,
                       size_t maxMatches = 1000);

} // namespace gx::search

