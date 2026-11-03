#pragma once

#include "godxplr/ui/view/file_layout.h"
#include <vector>

namespace gx::ui {

class DetailsLayout final : public IFileLayout {
public:
    explicit DetailsLayout(int rowHeight = 28);
    ~DetailsLayout() override = default;

    void setViewportSize(Size logicalSize) override;
    void setColumns(const ColumnLayout& columns) override;
    void setRowHeight(int height) override;

    [[nodiscard]] Size contentExtent(uint64_t rowCount) const override;

    [[nodiscard]] VisibleRange visibleRange(Rect exposed,
                                            uint64_t rowCount,
                                            Point scrollOffset) const override;

    [[nodiscard]] Rect itemRect(uint64_t row,
                                Point scrollOffset) const override;

    [[nodiscard]] Rect cellRect(uint64_t row,
                                int columnIdx,
                                Point scrollOffset) const override;

    [[nodiscard]] uint64_t hitTest(Point viewportPoint,
                                   uint64_t rowCount,
                                   Point scrollOffset) const override;

    [[nodiscard]] std::vector<Rect> selectionRects(
        std::span<const uint64_t> rows,
        Point scrollOffset) const override;

    [[nodiscard]] int getRowHeight() const noexcept { return rowHeight_; }
    [[nodiscard]] int totalColumnWidth() const noexcept { return totalWidth_; }

private:
    void updateCachedWidths();

    int rowHeight_{28};
    Size viewportSize_{800, 600};
    ColumnLayout columns_{ColumnLayout::defaultLayout()};
    int totalWidth_{0};
    std::vector<int> colStarts_;
};

} // namespace gx::ui
