#include "godxplr/ui/view/interaction_controller.h"
#include "godxplr/ui/view/grid_layout.h"
#include "godxplr/ui/view/gallery_layout.h"
#include <algorithm>

namespace gx::ui {

InteractionController::InteractionController(IFileLayout* layout)
    : layout_(layout)
{
}

int InteractionController::calculateStepDelta(KeyCode key) const noexcept {
    int cols = 1;

    // Check if current layout is a multi-column grid
    if (auto* grid = dynamic_cast<GridLayout*>(layout_)) {
        cols = grid->columnsPerRow();
    } else if (auto* gallery = dynamic_cast<GalleryLayout*>(layout_)) {
        cols = gallery->columnsPerRow();
    }

    switch (key) {
        case KeyCode::Up:        return -cols;
        case KeyCode::Down:      return cols;
        case KeyCode::Left:      return -1;
        case KeyCode::Right:     return 1;
        case KeyCode::PageUp:    return -cols * 5;
        case KeyCode::PageDown:  return cols * 5;
        default:                 return 0;
    }
}

bool InteractionController::handleKeyDown(KeyCode key, KeyModifier modifiers) {
    if (itemCount_ == 0) {
        return false;
    }

    const bool isShift = hasModifier(modifiers, KeyModifier::Shift);
    const bool isCtrl = hasModifier(modifiers, KeyModifier::Ctrl);

    // 1. Select All (Ctrl+A)
    if (key == KeyCode::KeyA && isCtrl) {
        selection_.selectAll(itemCount_);
        return true;
    }

    // 2. Rename Initiation (F2)
    if (key == KeyCode::F2 && focusIndex_.has_value()) {
        if (onBeginRename) {
            onBeginRename(*focusIndex_);
        }
        return true;
    }

    // 3. Activation (Enter)
    if (key == KeyCode::Enter && focusIndex_.has_value()) {
        if (onActivate) {
            onActivate(*focusIndex_);
        }
        return true;
    }

    // 4. Space Toggle (Ctrl+Space)
    if (key == KeyCode::Space) {
        if (focusIndex_.has_value()) {
            if (isCtrl) {
                selection_.toggle(*focusIndex_);
            } else {
                selection_.select(*focusIndex_);
                anchorIndex_ = *focusIndex_;
            }
            return true;
        }
    }

    // 5. Jump to Home / End
    if (key == KeyCode::Home) {
        uint64_t target = 0;
        if (isShift) {
            selection_.selectRange(anchorIndex_, target);
        } else if (!isCtrl) {
            selection_.select(target);
            anchorIndex_ = target;
        }
        focusIndex_ = target;
        return true;
    }

    if (key == KeyCode::End) {
        uint64_t target = itemCount_ - 1;
        if (isShift) {
            selection_.selectRange(anchorIndex_, target);
        } else if (!isCtrl) {
            selection_.select(target);
            anchorIndex_ = target;
        }
        focusIndex_ = target;
        return true;
    }

    // 6. Navigation keys: Up, Down, Left, Right, PageUp, PageDown
    int delta = calculateStepDelta(key);
    if (delta != 0) {
        int64_t next = 0;
        if (!focusIndex_.has_value()) {
            next = (delta > 0) ? 0 : static_cast<int64_t>(itemCount_ - 1);
        } else {
            next = static_cast<int64_t>(*focusIndex_) + delta;
        }

        if (next < 0) next = 0;
        if (next >= static_cast<int64_t>(itemCount_)) next = static_cast<int64_t>(itemCount_ - 1);

        uint64_t nextIdx = static_cast<uint64_t>(next);

        if (isShift) {
            selection_.selectRange(anchorIndex_, nextIdx);
        } else if (!isCtrl) {
            selection_.select(nextIdx);
            anchorIndex_ = nextIdx;
        }

        focusIndex_ = nextIdx;
        return true;
    }

    return false;
}

void InteractionController::handleMouseDown(Point viewportPoint, Point scrollOffset, KeyModifier modifiers) {
    if (!layout_ || itemCount_ == 0) {
        return;
    }

    uint64_t hit = layout_->hitTest(viewportPoint, itemCount_, scrollOffset);
    const bool isShift = hasModifier(modifiers, KeyModifier::Shift);
    const bool isCtrl = hasModifier(modifiers, KeyModifier::Ctrl);

    if (hit != UINT64_MAX) {
        focusIndex_ = hit;

        if (isShift) {
            selection_.selectRange(anchorIndex_, hit);
        } else if (isCtrl) {
            selection_.toggle(hit);
            anchorIndex_ = hit;
        } else {
            selection_.select(hit);
            anchorIndex_ = hit;
        }
    } else {
        // Clicked on empty canvas whitespace
        if (!isCtrl && !isShift) {
            selection_.clear();
            focusIndex_ = std::nullopt;
        }
    }
}

void InteractionController::handleDoubleClick(Point viewportPoint, Point scrollOffset) {
    if (!layout_ || itemCount_ == 0) {
        return;
    }

    uint64_t hit = layout_->hitTest(viewportPoint, itemCount_, scrollOffset);
    if (hit != UINT64_MAX && onActivate) {
        onActivate(hit);
    }
}

std::optional<DragPayload> InteractionController::createDragPayload(
    std::string_view sourceUri,
    const DirectoryStore& store) const
{
    if (selection_.empty()) {
        return std::nullopt;
    }

    DragPayload payload;
    payload.sourceUri = std::string(sourceUri);

    std::vector<uint64_t> rows = selection_.getSelectedRows();
    payload.keys.reserve(rows.size());

    for (uint64_t row : rows) {
        if (row < store.size()) {
            payload.keys.push_back(store.getEntry(row).key);
        }
    }

    return payload;
}

DropEffect InteractionController::evaluateDropTarget(
    Point viewportPoint,
    Point scrollOffset,
    const DragPayload& payload,
    const DirectoryStore& store,
    uint64_t& outTargetRow) const
{
    outTargetRow = UINT64_MAX;
    if (!layout_ || itemCount_ == 0 || payload.keys.empty()) {
        return DropEffect::None;
    }

    uint64_t hit = layout_->hitTest(viewportPoint, itemCount_, scrollOffset);
    if (hit == UINT64_MAX || hit >= store.size()) {
        return DropEffect::None;
    }

    const auto& targetEntry = store.getEntry(hit);

    // Dropping into a subfolder initiates Move or Copy
    if (targetEntry.isDirectory()) {
        outTargetRow = hit;
        return DropEffect::Move;
    }

    return DropEffect::None;
}

} // namespace gx::ui
