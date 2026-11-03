#include "godxplr/ui/view/gallery_layout.h"
#include <algorithm>

namespace gx::ui {

GalleryLayout::GalleryLayout(int cardWidth, int cardHeight, int spacing)
    : cardWidth_(cardWidth > 0 ? cardWidth : 200)
    , cardHeight_(cardHeight > 0 ? cardHeight : 160)
    , spacing_(spacing >= 0 ? spacing : 16)
{
}

void GalleryLayout::setViewportSize(Size logicalSize) {
    viewportSize_ = logicalSize;
}

void GalleryLayout::setColumns(const ColumnLayout&) {
}

void GalleryLayout::setRowHeight(int height) {
    if (height > 0) {
        cardHeight_ = height;
    }
}

int GalleryLayout::columnsPerRow() const noexcept {
    int availableWidth = viewportSize_.width - spacing_;
    int slotWidth = cardWidth_ + spacing_;
    if (availableWidth < slotWidth || slotWidth <= 0) {
        return 1;
    }
    return availableWidth / slotWidth;
}

Size GalleryLayout::contentExtent(uint64_t rowCount) const {
    if (rowCount == 0) {
        return Size{ viewportSize_.width, 0 };
    }

    int cols = columnsPerRow();
    uint64_t gridRows = (rowCount + cols - 1) / cols;

    int64_t totalHeight = spacing_ + static_cast<int64_t>(gridRows) * (cardHeight_ + spacing_);
    if (totalHeight > INT32_MAX) totalHeight = INT32_MAX;

    int totalWidth = spacing_ + cols * (cardWidth_ + spacing_);
    if (totalWidth < viewportSize_.width) {
        totalWidth = viewportSize_.width;
    }

    return Size{ totalWidth, static_cast<int>(totalHeight) };
}

VisibleRange GalleryLayout::visibleRange(Rect exposed, uint64_t rowCount, Point scrollOffset) const {
    if (rowCount == 0) {
        return VisibleRange{0, 0};
    }

    int cols = columnsPerRow();
    int slotHeight = cardHeight_ + spacing_;

    int64_t topDocY = static_cast<int64_t>(exposed.y) + scrollOffset.y;
    int64_t bottomDocY = topDocY + exposed.height;

    int64_t firstGridRow = (topDocY - spacing_) / slotHeight;
    if (firstGridRow < 0) firstGridRow = 0;

    int64_t lastGridRowExclusive = (bottomDocY - spacing_ + slotHeight - 1) / slotHeight;
    if (lastGridRowExclusive < 0) lastGridRowExclusive = 0;

    uint64_t firstItem = static_cast<uint64_t>(firstGridRow) * cols;
    if (firstItem > rowCount) firstItem = rowCount;

    uint64_t lastItemExclusive = static_cast<uint64_t>(lastGridRowExclusive + 1) * cols;
    if (lastItemExclusive > rowCount) lastItemExclusive = rowCount;

    return VisibleRange{ firstItem, lastItemExclusive };
}

Rect GalleryLayout::itemRect(uint64_t row, Point scrollOffset) const {
    int cols = columnsPerRow();
    uint64_t gridRow = row / cols;
    uint64_t gridCol = row % cols;

    int64_t docX = spacing_ + static_cast<int64_t>(gridCol) * (cardWidth_ + spacing_);
    int64_t docY = spacing_ + static_cast<int64_t>(gridRow) * (cardHeight_ + spacing_);

    int viewX = static_cast<int>(docX - scrollOffset.x);
    int viewY = static_cast<int>(docY - scrollOffset.y);

    return Rect{ viewX, viewY, cardWidth_, cardHeight_ };
}

Rect GalleryLayout::cellRect(uint64_t row, int, Point scrollOffset) const {
    return itemRect(row, scrollOffset);
}

uint64_t GalleryLayout::hitTest(Point viewportPoint, uint64_t rowCount, Point scrollOffset) const {
    if (rowCount == 0) {
        return UINT64_MAX;
    }

    int cols = columnsPerRow();
    int slotWidth = cardWidth_ + spacing_;
    int slotHeight = cardHeight_ + spacing_;

    int64_t docX = static_cast<int64_t>(viewportPoint.x) + scrollOffset.x;
    int64_t docY = static_cast<int64_t>(viewportPoint.y) + scrollOffset.y;

    if (docX < spacing_ || docY < spacing_) {
        return UINT64_MAX;
    }

    int64_t relX = (docX - spacing_) % slotWidth;
    int64_t relY = (docY - spacing_) % slotHeight;

    if (relX >= cardWidth_ || relY >= cardHeight_) {
        return UINT64_MAX;
    }

    int64_t gridCol = (docX - spacing_) / slotWidth;
    int64_t gridRow = (docY - spacing_) / slotHeight;

    if (gridCol < 0 || gridCol >= cols || gridRow < 0) {
        return UINT64_MAX;
    }

    uint64_t itemIdx = static_cast<uint64_t>(gridRow) * cols + static_cast<uint64_t>(gridCol);
    if (itemIdx < rowCount) {
        return itemIdx;
    }

    return UINT64_MAX;
}

std::vector<Rect> GalleryLayout::selectionRects(std::span<const uint64_t> rows, Point scrollOffset) const {
    std::vector<Rect> result;
    result.reserve(rows.size());
    for (uint64_t row : rows) {
        result.push_back(itemRect(row, scrollOffset));
    }
    return result;
}

} // namespace gx::ui
