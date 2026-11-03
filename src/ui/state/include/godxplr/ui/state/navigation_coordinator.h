#pragma once

#include "godxplr/ui/state/tab_state.h"
#include <vector>
#include <memory>
#include <string_view>
#include <optional>
#include <cstdint>

namespace gx::ui {

class NavigationCoordinator {
public:
    NavigationCoordinator();
    ~NavigationCoordinator() = default;

    // Disallow heavy accidental copies
    NavigationCoordinator(const NavigationCoordinator&) = delete;
    NavigationCoordinator& operator=(const NavigationCoordinator&) = delete;
    NavigationCoordinator(NavigationCoordinator&&) noexcept = default;
    NavigationCoordinator& operator=(NavigationCoordinator&&) noexcept = default;

    // Tab Management
    uint64_t openTab(std::string_view uri = "godxplr://this-pc", bool activate = true);
    bool closeTab(uint64_t tabId);
    bool activateTab(uint64_t tabId);
    bool duplicateTab(uint64_t tabId);

    [[nodiscard]] TabState* getActiveTab() noexcept;
    [[nodiscard]] const TabState* getActiveTab() const noexcept;
    [[nodiscard]] TabState* getTab(uint64_t tabId) noexcept;
    [[nodiscard]] const TabState* getTab(uint64_t tabId) const noexcept;
    [[nodiscard]] const std::vector<TabState>& getTabs() const noexcept { return tabs_; }
    [[nodiscard]] size_t tabCount() const noexcept { return tabs_.size(); }
    [[nodiscard]] uint64_t getActiveTabId() const noexcept { return activeTabId_; }

    // Navigation and History Traversal
    bool navigate(uint64_t tabId, std::string_view uri);
    bool goBack(uint64_t tabId);
    bool goForward(uint64_t tabId);
    bool goUp(uint64_t tabId);
    bool refresh(uint64_t tabId);

    [[nodiscard]] bool canGoBack(uint64_t tabId) const noexcept;
    [[nodiscard]] bool canGoForward(uint64_t tabId) const noexcept;
    [[nodiscard]] bool canGoUp(uint64_t tabId) const noexcept;

    // Helpers for path manipulation
    static std::string computeParentUri(std::string_view uri);
    static std::string formatTabTitle(std::string_view uri);

private:
    std::vector<TabState> tabs_;
    uint64_t activeTabId_{0};
    uint64_t nextTabId_{1};
};

} // namespace gx::ui
