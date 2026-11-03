#pragma once

#include "godxplr/core/directory_entry.h"
#include "godxplr/core/provider_contract.h"
#include "godxplr/ui/state/tab_state.h"
#include <span>
#include <vector>
#include <string_view>
#include <memory>
#include <cstdint>

namespace gx::ui {

class DirectorySorter {
public:
    DirectorySorter() = default;

    // Compares two strings using deterministic natural sorting (e.g. "file2" < "file10")
    [[nodiscard]] static int naturalCompare(std::string_view a, std::string_view b) noexcept;

    // Builds a sorted permutation vector for the given entries using the specified sort specification
    [[nodiscard]] static std::vector<uint32_t> buildPermutation(
        std::span<const DirectoryEntry> entries,
        std::string_view nameArena,
        const SortSpec& spec,
        std::shared_ptr<CancellationToken> cancelToken = nullptr);
};

} // namespace gx::ui
