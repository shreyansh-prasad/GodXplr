#pragma once

#include "godxplr/ui/model/geometry_types.h"
#include "godxplr/ui/state/tab_state.h"
#include <span>
#include <vector>
#include <cstdint>

namespace gx::ui {

class IFileLayout {
public:
    virtual ~IFileLayout() = default;

    virtual void setViewportSize(Size logicalSize) = 0;
    virtual void setColumns(const ColumnLayout& columns) = 0;
    virtual void setRowHeight(int height) = 0;

    [[nodiscard]] virtual Size contentExtent(uint64_t rowCount) const = 0;

    [[nodiscard]] virtual VisibleRange visibleRange(Rect exposed,
                                                    uint64_t rowCount,
                                                    Point scrollOffset) const = 0;

    [[nodiscard]] virtual Rect itemRect(uint64_t row,
                                        Point scrollOffset) const = 0;

    [[nodiscard]] virtual Rect cellRect(uint64_t row,
                                        int columnIdx,
                                        Point scrollOffset) const = 0;

    [[nodiscard]] virtual uint64_t hitTest(Point viewportPoint,
                                           uint64_t rowCount,
                                           Point scrollOffset) const = 0;

    [[nodiscard]] virtual std::vector<Rect> selectionRects(
        std::span<const uint64_t> rows,
        Point scrollOffset) const = 0;
};

} // namespace gx::ui
