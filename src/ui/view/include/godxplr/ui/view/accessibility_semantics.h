#pragma once

#include <string>
#include <string_view>
#include <cstdint>

namespace gx::ui {

enum class AccessibleRole : uint8_t {
    Window,
    TitleBar,
    ToolBar,
    MenuBar,
    List,
    ListItem,
    Table,
    TableRow,
    TableCell,
    Tree,
    TreeItem,
    Button,
    CheckBox,
    Edit,
    StaticText,
    ScrollBar
};

struct AccessibleProperties {
    AccessibleRole role{AccessibleRole::ListItem};
    std::string name;
    std::string description;
    std::string value;
    bool isSelected{false};
    bool isFocused{false};
    bool isExpanded{false};
    bool isExpandable{false};
    int childCount{0};
    int depthLevel{0};
};

} // namespace gx::ui
