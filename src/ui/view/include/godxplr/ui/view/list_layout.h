#pragma once

#include "godxplr/ui/view/file_layout.h"

namespace gx::ui {

class ListLayout final : public IFileLayout {
public:
    explicit ListLayout(int rowHeight = 24);
    ~ListLayout() override = default;

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

private:
    int rowHeight_{24};
    Size viewportSize_{800, 600};
    int contentWidth_{400};
};

} // namespace gx::ui
