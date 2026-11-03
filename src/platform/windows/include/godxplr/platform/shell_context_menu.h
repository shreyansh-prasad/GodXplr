#pragma once

#include <string>
#include <vector>
#include <cstdint>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>

namespace gx::platform {

struct ShellMenuItem {
    uint32_t id{0};
    std::wstring text;
    std::string verb;
    bool isDefault{false};
    bool isSeparator{false};
    bool isSubmenu{false};
};

class ShellContextMenu {
public:
    ShellContextMenu() = default;
    ~ShellContextMenu();

    ShellContextMenu(const ShellContextMenu&) = delete;
    ShellContextMenu& operator=(const ShellContextMenu&) = delete;

    // Populates context menu for a collection of file/folder paths
    bool populate(const std::vector<std::wstring>& paths);

    // Displays the popup menu at (x, y) coordinates and invokes the selected command
    bool showAndInvoke(HWND hwndParent, int x, int y);

    // Extracts list of textual menu items (for headless inspection and testing)
    [[nodiscard]] std::vector<ShellMenuItem> getMenuItems() const;

    [[nodiscard]] HMENU menuHandle() const noexcept { return hMenu_; }
    void clear();

private:
    HMENU hMenu_{nullptr};
    IContextMenu* pcm_{nullptr};
    IContextMenu2* pcm2_{nullptr};
    IContextMenu3* pcm3_{nullptr};
};

} // namespace gx::platform
