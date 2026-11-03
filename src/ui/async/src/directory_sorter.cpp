#include "godxplr/ui/async/directory_sorter.h"
#include <algorithm>
#include <numeric>

namespace gx::ui {

namespace {

inline std::string_view getExtension(std::string_view name) noexcept {
    size_t dot = name.rfind('.');
    if (dot == std::string_view::npos || dot == 0 || dot == name.size() - 1) {
        return {};
    }
    return name.substr(dot + 1);
}

} // namespace

int DirectorySorter::naturalCompare(std::string_view a, std::string_view b) noexcept {
    size_t ia = 0, ib = 0;
    while (ia < a.size() && ib < b.size()) {
        char ca = a[ia];
        char cb = b[ib];

        bool isDigitA = (ca >= '0' && ca <= '9');
        bool isDigitB = (cb >= '0' && cb <= '9');

        if (isDigitA && isDigitB) {
            // Parse contiguous numeric block
            uint64_t numA = 0;
            while (ia < a.size() && (a[ia] >= '0' && a[ia] <= '9')) {
                numA = numA * 10 + static_cast<uint64_t>(a[ia] - '0');
                ia++;
            }

            uint64_t numB = 0;
            while (ib < b.size() && (b[ib] >= '0' && b[ib] <= '9')) {
                numB = numB * 10 + static_cast<uint64_t>(b[ib] - '0');
                ib++;
            }

            if (numA != numB) {
                return (numA < numB) ? -1 : 1;
            }
        } else {
            // Case-insensitive ASCII character comparison
            char la = (ca >= 'A' && ca <= 'Z') ? static_cast<char>(ca + 32) : ca;
            char lb = (cb >= 'A' && cb <= 'Z') ? static_cast<char>(cb + 32) : cb;
            if (la != lb) {
                return (la < lb) ? -1 : 1;
            }
            ia++;
            ib++;
        }
    }

    if (ia < a.size()) return 1;
    if (ib < b.size()) return -1;
    return 0;
}

std::vector<uint32_t> DirectorySorter::buildPermutation(
    std::span<const DirectoryEntry> entries,
    std::string_view nameArena,
    const SortSpec& spec,
    std::shared_ptr<CancellationToken> cancelToken)
{
    if (entries.empty()) {
        return {};
    }

    if (cancelToken && cancelToken->isCancelled()) {
        return {};
    }

    std::vector<uint32_t> perm(entries.size());
    std::iota(perm.begin(), perm.end(), 0u);

    auto getName = [&](uint32_t idx) -> std::string_view {
        const auto& slice = entries[idx].name;
        if (slice.offset + slice.length <= nameArena.size()) {
            return std::string_view(nameArena.data() + slice.offset, slice.length);
        }
        return {};
    };

    auto comparator = [&](uint32_t a, uint32_t b) -> bool {
        const auto& entryA = entries[a];
        const auto& entryB = entries[b];

        // 1. Group directories first if requested
        if (spec.directoriesFirst) {
            bool dirA = entryA.isDirectory();
            bool dirB = entryB.isDirectory();
            if (dirA != dirB) {
                return dirA; // Directories always precede files
            }
        }

        int cmp = 0;

        // 2. Primary sort field
        switch (spec.column) {
            case SortColumn::Name: {
                cmp = naturalCompare(getName(a), getName(b));
                break;
            }
            case SortColumn::Size: {
                if (entryA.size != entryB.size) {
                    cmp = (entryA.size < entryB.size) ? -1 : 1;
                } else {
                    cmp = naturalCompare(getName(a), getName(b));
                }
                break;
            }
            case SortColumn::ModifiedTime: {
                if (entryA.mtime != entryB.mtime) {
                    cmp = (entryA.mtime < entryB.mtime) ? -1 : 1;
                } else {
                    cmp = naturalCompare(getName(a), getName(b));
                }
                break;
            }
            case SortColumn::Type: {
                std::string_view extA = getExtension(getName(a));
                std::string_view extB = getExtension(getName(b));
                cmp = naturalCompare(extA, extB);
                if (cmp == 0) {
                    cmp = naturalCompare(getName(a), getName(b));
                }
                break;
            }
            case SortColumn::Attributes: {
                if (entryA.attributes != entryB.attributes) {
                    cmp = (entryA.attributes < entryB.attributes) ? -1 : 1;
                } else {
                    cmp = naturalCompare(getName(a), getName(b));
                }
                break;
            }
        }

        // Apply sort direction
        if (spec.order == SortOrder::Descending) {
            cmp = -cmp;
        }

        if (cmp != 0) {
            return cmp < 0;
        }

        // 3. Stable tie-breaker: compare EntryKey
        return entryA.key < entryB.key;
    };

    std::sort(perm.begin(), perm.end(), comparator);

    if (cancelToken && cancelToken->isCancelled()) {
        return {};
    }

    return perm;
}

} // namespace gx::ui
