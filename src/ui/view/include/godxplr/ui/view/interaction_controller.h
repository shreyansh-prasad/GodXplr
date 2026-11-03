#pragma once

#include "godxplr/ui/model/selection_model.h"
#include "godxplr/ui/model/directory_store.h"
#include "godxplr/ui/view/file_layout.h"
#include <cstdint>
#include <optional>
#include <functional>
#include <vector>
#include <string>

namespace gx::ui {

enum class KeyCode : uint16_t {
    Up,
    Down,
    Left,
    Right,
    PageUp,
    PageDown,
    Home,
    End,
    Enter,
    Space,
    F2,
    KeyA,
    Delete
};

enum class KeyModifier : uint8_t {
    None  = 0,
    Shift = 1 << 0,
    Ctrl  = 1 << 1,
    Alt   = 1 << 2
};

inline constexpr KeyModifier operator|(KeyModifier a, KeyModifier b) noexcept {
    return static_cast<KeyModifier>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
}

inline constexpr bool hasModifier(KeyModifier mods, KeyModifier flag) noexcept {
    return (static_cast<uint8_t>(mods) & static_cast<uint8_t>(flag)) != 0;
}

struct DragPayload {
    std::string sourceUri;
    std::vector<EntryKey> keys;
};

enum class DropEffect : uint8_t {
    None,
    Copy,
    Move,
    Link
};

class InteractionController {
public:
    explicit InteractionController(IFileLayout* layout = nullptr);
    ~InteractionController() = default;

    void setLayout(IFileLayout* layout) noexcept { layout_ = layout; }
    void setItemCount(uint64_t count) noexcept { itemCount_ = count; }

    // Keyboard navigation
    bool handleKeyDown(KeyCode key, KeyModifier modifiers = KeyModifier::None);

    // Mouse interactions
    void handleMouseDown(Point viewportPoint, Point scrollOffset, KeyModifier modifiers = KeyModifier::None);
    void handleDoubleClick(Point viewportPoint, Point scrollOffset);

    // Drag and drop scaffolding
    [[nodiscard]] std::optional<DragPayload> createDragPayload(std::string_view sourceUri, const DirectoryStore& store) const;
    [[nodiscard]] DropEffect evaluateDropTarget(Point viewportPoint, Point scrollOffset, const DragPayload& payload, const DirectoryStore& store, uint64_t& outTargetRow) const;

    // Selection & Focus accessors
    [[nodiscard]] VirtualSelectionModel& selectionModel() noexcept { return selection_; }
    [[nodiscard]] const VirtualSelectionModel& selectionModel() const noexcept { return selection_; }
    [[nodiscard]] std::optional<uint64_t> focusIndex() const noexcept { return focusIndex_; }
    void setFocusIndex(std::optional<uint64_t> focus) noexcept { focusIndex_ = focus; }

    // Action callbacks
    std::function<void(uint64_t row)> onBeginRename;
    std::function<void(uint64_t row)> onActivate;

private:
    [[nodiscard]] int calculateStepDelta(KeyCode key) const noexcept;

    IFileLayout* layout_{nullptr};
    uint64_t itemCount_{0};
    VirtualSelectionModel selection_;
    std::optional<uint64_t> focusIndex_{std::nullopt};
    uint64_t anchorIndex_{0};
};

} // namespace gx::ui
