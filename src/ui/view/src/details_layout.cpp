#include "godxplr/ui/view/details_layout.h"
#include <algorithm>

namespace gx::ui {

DetailsLayout::DetailsLayout(int rowHeight)
    : rowHeight_(rowHeight) {
    updateCachedWidths();
}

void DetailsLayout::setViewportSize(Size logicalSize) {
    viewportSize_ = logicalSize;
}

void DetailsLayout::setColumns(const ColumnLayout& columns) {
    columns_ = columns;
    updateCachedWidths();
}

void DetailsLayout::setRowHeight(int height) {
    if (height > 0) {
        rowHeight_ = height;
    }
}

void DetailsLayout::updateCachedWidths() {
    colStarts_.clear();
    colStarts_.reserve(columns_.columns.size());
    int curX = 0;
    for (const auto& col : columns_.columns) {
        if (col.visible) {
            colStarts_.push_back(curX);
            curX += col.width;
        } else {
            colStarts_.push_back(-1);
        }
    }
    totalWidth_ = curX;
}

Size DetailsLayout::contentExtent(uint64_t rowCount) const {
    int w = std::max(viewportSize_.width, totalWidth_);
    int h = static_cast<int>(rowCount * static_cast<uint64_t>(rowHeight_));
    return Size{w, h};
}

VisibleRange DetailsLayout::visibleRange(Rect exposed,
                                         uint64_t rowCount,
                                         Point scrollOffset) const {
    if (rowCount == 0 || rowHeight_ <= 0) {
        return VisibleRange{0, 0};
    }

    int topY = exposed.y + scrollOffset.y;
    int bottomY = exposed.bottom() + scrollOffset.y;

    if (bottomY <= 0) {
        return VisibleRange{0, 0};
    }

    int64_t first = std::max<int64_t>(0, topY / rowHeight_);
    int64_t last = std::min<int64_t>(
        static_cast<int64_t>(rowCount),
        (static_cast<int64_t>(bottomY) + rowHeight_ - 1) / rowHeight_
    );

    if (first >= last) {
        return VisibleRange{0, 0};
    }

    return VisibleRange{static_cast<uint64_t>(first), static_cast<uint64_t>(last)};
}

Rect DetailsLayout::itemRect(uint64_t row, Point scrollOffset) const {
    int y = static_cast<int>(row * static_cast<uint64_t>(rowHeight_)) - scrollOffset.y;
    int x = -scrollOffset.x;
    return Rect{x, y, totalWidth_, rowHeight_};
}

Rect DetailsLayout::cellRect(uint64_t row, int columnIdx, Point scrollOffset) const {
    if (columnIdx < 0 || columnIdx >= static_cast<int>(colStarts_.size())) {
        return Rect{};
    }

    int startX = colStarts_[columnIdx];
    if (startX < 0) {
        return Rect{}; // Column hidden
    }

    int colW = columns_.columns[columnIdx].width;
    int y = static_cast<int>(row * static_cast<uint64_t>(rowHeight_)) - scrollOffset.y;
    int x = startX - scrollOffset.x;

    return Rect{x, y, colW, rowHeight_};
}

uint64_t DetailsLayout::hitTest(Point viewportPoint,
                                uint64_t rowCount,
                                Point scrollOffset) const {
    if (rowHeight_ <= 0 || rowCount == 0) {
        return UINT64_MAX;
    }

    int logicalY = viewportPoint.y + scrollOffset.y;
    if (logicalY < 0) {
        return UINT64_MAX;
    }

    uint64_t row = static_cast<uint64_t>(logicalY / rowHeight_);
    if (row >= rowCount) {
        return UINT64_MAX;
    }

    return row;
}

std::vector<Rect> DetailsLayout::selectionRects(std::span<const uint64_t> rows,
                                                Point scrollOffset) const {
    std::vector<Rect> rects;
    if (rows.empty()) {
        return rects;
    }

    std::vector<uint64_t> sortedRows(rows.begin(), rows.end());
    std::sort(sortedRows.begin(), sortedRows.end());

    uint64_t runStart = sortedRows[0];
    uint64_t runLength = 1;

    for (size_t i = 1; i < sortedRows.size(); ++i) {
        if (sortedRows[i] == sortedRows[i - 1] + 1) {
            runLength++;
        } else {
            // Flush current run
            int y = static_cast<int>(runStart * static_cast<uint64_t>(rowHeight_)) - scrollOffset.y;
            int h = static_cast<int>(runLength * static_cast<uint64_t>(rowHeight_));
            rects.push_back(Rect{-scrollOffset.x, y, totalWidth_, h});

            runStart = sortedRows[i];
            runLength = 1;
        }
    }

    // Flush final run
    int y = static_cast<int>(runStart * static_cast<uint64_t>(rowHeight_)) - scrollOffset.y;
    int h = static_cast<int>(runLength * static_cast<uint64_t>(rowHeight_));
    rects.push_back(Rect{-scrollOffset.x, y, totalWidth_, h});

    return rects;
}

} // namespace gx::ui
