#pragma once

#include "godxplr/ui/view/file_layout.h"

namespace gx::ui {

class GridLayout final : public IFileLayout {
public:
    explicit GridLayout(int cardWidth = 120, int cardHeight = 100, int spacing = 12);
    ~GridLayout() override = default;

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

    [[nodiscard]] int columnsPerRow() const noexcept;
    [[nodiscard]] int cardWidth() const noexcept { return cardWidth_; }
    [[nodiscard]] int cardHeight() const noexcept { return cardHeight_; }
    [[nodiscard]] int spacing() const noexcept { return spacing_; }

private:
    int cardWidth_{120};
    int cardHeight_{100};
    int spacing_{12};
    Size viewportSize_{800, 600};
};

} // namespace gx::ui
