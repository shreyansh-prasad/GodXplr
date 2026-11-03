#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <string_view>

namespace gx::ui {

struct UiAction {
    std::string id;
    std::string label;
    std::string shortcut;
    std::string description;
    bool isImplemented{false};
    std::string targetPhase; // Phase that delivers implementation if not yet active
};

class ActionRegistry {
public:
    static ActionRegistry& instance() {
        static ActionRegistry reg;
        return reg;
    }

    void registerAction(UiAction action) {
        actions_[action.id] = std::move(action);
    }

    [[nodiscard]] const UiAction* getAction(std::string_view id) const {
        auto it = actions_.find(std::string(id));
        return (it != actions_.end()) ? &it->second : nullptr;
    }

    [[nodiscard]] std::vector<UiAction> getAllActions() const {
        std::vector<UiAction> list;
        list.reserve(actions_.size());
        for (const auto& [_, act] : actions_) {
            list.push_back(act);
        }
        return list;
    }

private:
    ActionRegistry() {
        // Active / Implemented in P04
        registerAction({"app.new_tab", "New Tab", "Ctrl+T", "Opens a new tab", true, "P04"});
        registerAction({"app.close_tab", "Close Tab", "Ctrl+W", "Closes active tab", true, "P04"});
        registerAction({"nav.back", "Back", "Alt+Left", "Navigates to previous folder in history", true, "P04"});
        registerAction({"nav.forward", "Forward", "Alt+Right", "Navigates to forward folder in history", true, "P04"});
        registerAction({"nav.up", "Up", "Alt+Up", "Navigates to parent directory", true, "P04"});
        registerAction({"nav.refresh", "Refresh", "F5", "Refreshes active folder listing", true, "P04"});
        registerAction({"view.mode_details", "Details View", "Ctrl+Shift+6", "Switches view to Details mode", true, "P04"});
        registerAction({"view.mode_grid", "Grid View", "Ctrl+Shift+2", "Switches view to Grid mode", true, "P04"});
        registerAction({"view.mode_list", "List View", "Ctrl+Shift+5", "Switches view to List mode", true, "P04"});

        // Explicit Placeholders for Future Phases (Disabled, clearly attributed)
        registerAction({"file.copy", "Copy", "Ctrl+C", "Copies selected files (Phase P11)", false, "P11"});
        registerAction({"file.paste", "Paste", "Ctrl+V", "Pastes files from clipboard (Phase P11)", false, "P11"});
        registerAction({"file.delete", "Delete", "Del", "Deletes selected files (Phase P11)", false, "P11"});
        registerAction({"file.rename", "Rename", "F2", "Renames selected file (Phase P11)", false, "P11"});
        registerAction({"file.properties", "Properties", "Alt+Enter", "Displays file properties (Phase P12)", false, "P12"});
        registerAction({"search.focus", "Search", "Ctrl+F", "Focuses disk-wide filename search", true, "P09"});
    }

    std::unordered_map<std::string, UiAction> actions_;
};

} // namespace gx::ui
