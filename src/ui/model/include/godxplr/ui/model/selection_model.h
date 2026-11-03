#pragma once

#include <cstdint>
#include <vector>
#include <optional>

namespace gx::ui {

struct SelectionRange {
    uint64_t start{0};
    uint64_t endExclusive{0};

    [[nodiscard]] bool contains(uint64_t row) const noexcept {
        return row >= start && row < endExclusive;
    }
    [[nodiscard]] uint64_t count() const noexcept {
        return (endExclusive > start) ? (endExclusive - start) : 0;
    }

    auto operator<=>(const SelectionRange&) const = default;
};

class VirtualSelectionModel {
public:
    VirtualSelectionModel() = default;
    ~VirtualSelectionModel() = default;

    void select(uint64_t row);
    void selectRange(uint64_t startRow, uint64_t endRow, bool additive = false);
    void toggle(uint64_t row);
    void selectAll(uint64_t rowCount);
    void clear();

    [[nodiscard]] bool isSelected(uint64_t row) const;
    [[nodiscard]] uint64_t selectedCount() const noexcept;
    [[nodiscard]] bool empty() const noexcept { return ranges_.empty(); }

    [[nodiscard]] std::vector<uint64_t> getSelectedRows() const;
    [[nodiscard]] const std::vector<SelectionRange>& getRanges() const noexcept { return ranges_; }

    [[nodiscard]] std::optional<uint64_t> anchorRow() const noexcept { return anchorRow_; }
    void setAnchorRow(uint64_t row) noexcept { anchorRow_ = row; }

    [[nodiscard]] std::optional<uint64_t> focusRow() const noexcept { return focusRow_; }
    void setFocusRow(uint64_t row) noexcept { focusRow_ = row; }

private:
    void normalizeRanges();

    std::vector<SelectionRange> ranges_;
    std::optional<uint64_t> anchorRow_;
    std::optional<uint64_t> focusRow_;
};

} // namespace gx::ui
