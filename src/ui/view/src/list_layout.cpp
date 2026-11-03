#include "godxplr/ui/view/list_layout.h"
#include <algorithm>

namespace gx::ui {

ListLayout::ListLayout(int rowHeight)
    : rowHeight_(rowHeight > 0 ? rowHeight : 24)
{
}

void ListLayout::setViewportSize(Size logicalSize) {
    viewportSize_ = logicalSize;
    contentWidth_ = std::max(300, logicalSize.width);
}

void ListLayout::setColumns(const ColumnLayout&) {
    // List layout renders single-column compact rows
}

void ListLayout::setRowHeight(int height) {
    if (height > 0) {
        rowHeight_ = height;
    }
}

Size ListLayout::contentExtent(uint64_t rowCount) const {
    int64_t totalHeight = static_cast<int64_t>(rowCount) * rowHeight_;
    if (totalHeight > INT32_MAX) totalHeight = INT32_MAX;
    return Size{ contentWidth_, static_cast<int>(totalHeight) };
}

VisibleRange ListLayout::visibleRange(Rect exposed, uint64_t rowCount, Point scrollOffset) const {
    if (rowCount == 0) {
        return VisibleRange{0, 0};
    }

    int64_t topDocY = static_cast<int64_t>(exposed.y) + scrollOffset.y;
    int64_t bottomDocY = topDocY + exposed.height;

    if (bottomDocY <= 0 || topDocY >= static_cast<int64_t>(rowCount) * rowHeight_) {
        return VisibleRange{0, 0};
    }

    int64_t firstRow = topDocY / rowHeight_;
    if (firstRow < 0) firstRow = 0;

    int64_t lastRowExclusive = (bottomDocY + rowHeight_ - 1) / rowHeight_;
    if (lastRowExclusive > static_cast<int64_t>(rowCount)) {
        lastRowExclusive = static_cast<int64_t>(rowCount);
    }

    return VisibleRange{
        static_cast<uint64_t>(firstRow),
        static_cast<uint64_t>(lastRowExclusive)
    };
}

Rect ListLayout::itemRect(uint64_t row, Point scrollOffset) const {
    int64_t docY = static_cast<int64_t>(row) * rowHeight_;
    int viewY = static_cast<int>(docY - scrollOffset.y);
    int viewX = -scrollOffset.x;
    return Rect{ viewX, viewY, contentWidth_, rowHeight_ };
}

Rect ListLayout::cellRect(uint64_t row, int, Point scrollOffset) const {
    return itemRect(row, scrollOffset);
}

uint64_t ListLayout::hitTest(Point viewportPoint, uint64_t rowCount, Point scrollOffset) const {
    if (rowCount == 0) {
        return UINT64_MAX;
    }

    int64_t docX = static_cast<int64_t>(viewportPoint.x) + scrollOffset.x;
    int64_t docY = static_cast<int64_t>(viewportPoint.y) + scrollOffset.y;

    if (docX < 0 || docX >= contentWidth_ || docY < 0) {
        return UINT64_MAX;
    }

    int64_t row = docY / rowHeight_;
    if (row >= 0 && row < static_cast<int64_t>(rowCount)) {
        return static_cast<uint64_t>(row);
    }

    return UINT64_MAX;
}

std::vector<Rect> ListLayout::selectionRects(std::span<const uint64_t> rows, Point scrollOffset) const {
    if (rows.empty()) {
        return {};
    }

    std::vector<Rect> result;
    result.reserve(rows.size());

    uint64_t runStart = rows[0];
    uint64_t runEnd = rows[0] + 1;

    for (size_t i = 1; i < rows.size(); ++i) {
        if (rows[i] == runEnd) {
            runEnd++;
        } else {
            int64_t docY = static_cast<int64_t>(runStart) * rowHeight_;
            int viewY = static_cast<int>(docY - scrollOffset.y);
            int height = static_cast<int>((runEnd - runStart) * rowHeight_);
            result.push_back(Rect{ -scrollOffset.x, viewY, contentWidth_, height });

            runStart = rows[i];
            runEnd = rows[i] + 1;
        }
    }

    int64_t docY = static_cast<int64_t>(runStart) * rowHeight_;
    int viewY = static_cast<int>(docY - scrollOffset.y);
    int height = static_cast<int>((runEnd - runStart) * rowHeight_);
    result.push_back(Rect{ -scrollOffset.x, viewY, contentWidth_, height });

    return result;
}

} // namespace gx::ui
