#pragma once

#include "godxplr/core/entry_key.h"
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <optional>

namespace gx::ui {

enum class ViewMode : uint8_t {
    Details,
    List,
    Grid,
    Gallery
};

enum class ViewDensity : uint8_t {
    Compact,
    Comfortable,
    Spacious
};

enum class SortColumn : uint8_t {
    Name,
    Size,
    ModifiedTime,
    Type,
    Attributes
};

enum class SortOrder : uint8_t {
    Ascending,
    Descending
};

struct SortSpec {
    SortColumn column{SortColumn::Name};
    SortOrder order{SortOrder::Ascending};
    bool directoriesFirst{true};
    bool naturalNumeric{true};

    auto operator<=>(const SortSpec&) const = default;
};

struct FilterSpec {
    std::string globPattern;
    bool showHidden{false};
    bool showSystem{false};

    auto operator<=>(const FilterSpec&) const = default;
};

struct ScrollAnchor {
    int64_t verticalOffset{0};
    int64_t horizontalOffset{0};
    std::optional<EntryKey> anchorEntry{};

    auto operator<=>(const ScrollAnchor&) const = default;
};

struct ColumnSpec {
    SortColumn column{SortColumn::Name};
    int width{180};
    bool visible{true};
    int displayIndex{0};

    auto operator<=>(const ColumnSpec&) const = default;
};

struct ColumnLayout {
    std::vector<ColumnSpec> columns;

    static ColumnLayout defaultLayout() {
        ColumnLayout layout;
        layout.columns = {
            {SortColumn::Name, 260, true, 0},
            {SortColumn::ModifiedTime, 160, true, 1},
            {SortColumn::Type, 120, true, 2},
            {SortColumn::Size, 110, true, 3},
            {SortColumn::Attributes, 90, false, 4}
        };
        return layout;
    }
};

enum class LocationStatus : uint8_t {
    Ready,
    Loading,
    Empty,
    Offline,
    PermissionDenied,
    Error
};

struct TabState {
    uint64_t tabId{0};
    std::string title{"This PC"};
    std::string providerUri{"godxplr://this-pc"};
    std::vector<std::string> backHistory;
    std::vector<std::string> forwardHistory;
    std::optional<EntryKey> focusedEntry{};
    std::vector<EntryKey> selectedEntries;
    ViewMode viewMode{ViewMode::Details};
    ViewDensity density{ViewDensity::Comfortable};
    SortSpec sort{};
    FilterSpec filter{};
    ScrollAnchor scrollAnchor{};
    ColumnLayout columns{ColumnLayout::defaultLayout()};
    LocationStatus status{LocationStatus::Ready};
    std::string statusMessage;
    bool previewVisible{false};
    bool isPinned{false};
};

} // namespace gx::ui
