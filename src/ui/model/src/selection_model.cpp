#include "godxplr/ui/model/selection_model.h"
#include <algorithm>

namespace gx::ui {

void VirtualSelectionModel::normalizeRanges() {
    if (ranges_.size() <= 1) {
        return;
    }

    std::sort(ranges_.begin(), ranges_.end(), [](const SelectionRange& a, const SelectionRange& b) {
        return a.start < b.start;
    });

    std::vector<SelectionRange> merged;
    merged.reserve(ranges_.size());

    SelectionRange cur = ranges_[0];
    for (size_t i = 1; i < ranges_.size(); ++i) {
        const auto& next = ranges_[i];
        if (next.start <= cur.endExclusive) {
            cur.endExclusive = std::max(cur.endExclusive, next.endExclusive);
        } else {
            merged.push_back(cur);
            cur = next;
        }
    }
    merged.push_back(cur);

    ranges_ = std::move(merged);
}

void VirtualSelectionModel::clear() {
    ranges_.clear();
    anchorRow_.reset();
    focusRow_.reset();
}

void VirtualSelectionModel::select(uint64_t row) {
    clear();
    ranges_.push_back(SelectionRange{row, row + 1});
    anchorRow_ = row;
    focusRow_ = row;
}

void VirtualSelectionModel::selectRange(uint64_t startRow, uint64_t endRow, bool additive) {
    if (!additive) {
        ranges_.clear();
    }
    uint64_t low = std::min(startRow, endRow);
    uint64_t high = std::max(startRow, endRow) + 1;

    ranges_.push_back(SelectionRange{low, high});
    normalizeRanges();
    focusRow_ = endRow;
}

void VirtualSelectionModel::toggle(uint64_t row) {
    focusRow_ = row;
    anchorRow_ = row;

    for (auto it = ranges_.begin(); it != ranges_.end(); ++it) {
        if (it->contains(row)) {
            // Remove row from range
            uint64_t oldStart = it->start;
            uint64_t oldEnd = it->endExclusive;
            ranges_.erase(it);

            if (oldStart < row) {
                ranges_.push_back(SelectionRange{oldStart, row});
            }
            if (row + 1 < oldEnd) {
                ranges_.push_back(SelectionRange{row + 1, oldEnd});
            }
            normalizeRanges();
            return;
        }
    }

    // Row not in any range: add it
    ranges_.push_back(SelectionRange{row, row + 1});
    normalizeRanges();
}

void VirtualSelectionModel::selectAll(uint64_t rowCount) {
    clear();
    if (rowCount > 0) {
        ranges_.push_back(SelectionRange{0, rowCount});
        anchorRow_ = 0;
        focusRow_ = 0;
    }
}

bool VirtualSelectionModel::isSelected(uint64_t row) const {
    if (ranges_.empty()) {
        return false;
    }

    // Binary search for range covering row
    auto it = std::upper_bound(
        ranges_.begin(),
        ranges_.end(),
        row,
        [](uint64_t val, const SelectionRange& r) {
            return val < r.start;
        }
    );

    if (it == ranges_.begin()) {
        return false;
    }
    --it;
    return it->contains(row);
}

uint64_t VirtualSelectionModel::selectedCount() const noexcept {
    uint64_t count = 0;
    for (const auto& r : ranges_) {
        count += r.count();
    }
    return count;
}

std::vector<uint64_t> VirtualSelectionModel::getSelectedRows() const {
    std::vector<uint64_t> rows;
    rows.reserve(selectedCount());
    for (const auto& r : ranges_) {
        for (uint64_t i = r.start; i < r.endExclusive; ++i) {
            rows.push_back(i);
        }
    }
    return rows;
}

} // namespace gx::ui
