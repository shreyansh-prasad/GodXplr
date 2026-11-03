#include "godxplr/ui/state/navigation_coordinator.h"
#include <algorithm>

namespace gx::ui {

NavigationCoordinator::NavigationCoordinator() {
    openTab("godxplr://this-pc", true);
}

uint64_t NavigationCoordinator::openTab(std::string_view uri, bool activate) {
    uint64_t id = nextTabId_++;
    TabState tab;
    tab.tabId = id;
    tab.providerUri = std::string(uri);
    tab.title = formatTabTitle(uri);

    tabs_.push_back(std::move(tab));

    if (activate || tabs_.size() == 1) {
        activeTabId_ = id;
    }
    return id;
}

bool NavigationCoordinator::closeTab(uint64_t tabId) {
    auto it = std::find_if(tabs_.begin(), tabs_.end(), [tabId](const TabState& t) {
        return t.tabId == tabId;
    });

    if (it == tabs_.end()) {
        return false;
    }

    size_t idx = std::distance(tabs_.begin(), it);
    bool wasActive = (it->tabId == activeTabId_);

    tabs_.erase(it);

    if (tabs_.empty()) {
        // Always maintain at least one tab
        openTab("godxplr://this-pc", true);
        return true;
    }

    if (wasActive) {
        // Activate previous or next adjacent tab
        if (idx > 0) {
            activeTabId_ = tabs_[idx - 1].tabId;
        } else {
            activeTabId_ = tabs_[0].tabId;
        }
    }

    return true;
}

bool NavigationCoordinator::activateTab(uint64_t tabId) {
    auto it = std::find_if(tabs_.begin(), tabs_.end(), [tabId](const TabState& t) {
        return t.tabId == tabId;
    });

    if (it != tabs_.end()) {
        activeTabId_ = tabId;
        return true;
    }
    return false;
}

bool NavigationCoordinator::duplicateTab(uint64_t tabId) {
    const TabState* source = getTab(tabId);
    if (!source) {
        return false;
    }

    uint64_t newId = openTab(source->providerUri, true);
    TabState* newTab = getTab(newId);
    if (newTab) {
        newTab->viewMode = source->viewMode;
        newTab->density = source->density;
        newTab->sort = source->sort;
        newTab->filter = source->filter;
        newTab->columns = source->columns;
        newTab->previewVisible = source->previewVisible;
    }
    return true;
}

TabState* NavigationCoordinator::getActiveTab() noexcept {
    return getTab(activeTabId_);
}

const TabState* NavigationCoordinator::getActiveTab() const noexcept {
    return getTab(activeTabId_);
}

TabState* NavigationCoordinator::getTab(uint64_t tabId) noexcept {
    auto it = std::find_if(tabs_.begin(), tabs_.end(), [tabId](const TabState& t) {
        return t.tabId == tabId;
    });
    return (it != tabs_.end()) ? &(*it) : nullptr;
}

const TabState* NavigationCoordinator::getTab(uint64_t tabId) const noexcept {
    auto it = std::find_if(tabs_.begin(), tabs_.end(), [tabId](const TabState& t) {
        return t.tabId == tabId;
    });
    return (it != tabs_.end()) ? &(*it) : nullptr;
}

bool NavigationCoordinator::navigate(uint64_t tabId, std::string_view uri) {
    TabState* tab = getTab(tabId);
    if (!tab) {
        return false;
    }

    if (tab->providerUri == uri) {
        return true; // Already there
    }

    tab->backHistory.push_back(tab->providerUri);
    tab->forwardHistory.clear();
    tab->providerUri = std::string(uri);
    tab->title = formatTabTitle(uri);
    tab->focusedEntry.reset();
    tab->selectedEntries.clear();
    tab->scrollAnchor = {};
    tab->status = LocationStatus::Ready;
    tab->statusMessage.clear();
    return true;
}

bool NavigationCoordinator::goBack(uint64_t tabId) {
    TabState* tab = getTab(tabId);
    if (!tab || tab->backHistory.empty()) {
        return false;
    }

    std::string previous = tab->backHistory.back();
    tab->backHistory.pop_back();

    tab->forwardHistory.push_back(tab->providerUri);
    tab->providerUri = previous;
    tab->title = formatTabTitle(previous);
    tab->focusedEntry.reset();
    tab->selectedEntries.clear();
    tab->scrollAnchor = {};
    tab->status = LocationStatus::Ready;
    tab->statusMessage.clear();
    return true;
}

bool NavigationCoordinator::goForward(uint64_t tabId) {
    TabState* tab = getTab(tabId);
    if (!tab || tab->forwardHistory.empty()) {
        return false;
    }

    std::string next = tab->forwardHistory.back();
    tab->forwardHistory.pop_back();

    tab->backHistory.push_back(tab->providerUri);
    tab->providerUri = next;
    tab->title = formatTabTitle(next);
    tab->focusedEntry.reset();
    tab->selectedEntries.clear();
    tab->scrollAnchor = {};
    tab->status = LocationStatus::Ready;
    tab->statusMessage.clear();
    return true;
}

bool NavigationCoordinator::goUp(uint64_t tabId) {
    TabState* tab = getTab(tabId);
    if (!tab) {
        return false;
    }

    std::string parent = computeParentUri(tab->providerUri);
    if (parent != tab->providerUri) {
        return navigate(tabId, parent);
    }
    return false;
}

bool NavigationCoordinator::refresh(uint64_t tabId) {
    TabState* tab = getTab(tabId);
    if (!tab) {
        return false;
    }
    tab->status = LocationStatus::Loading;
    return true;
}

bool NavigationCoordinator::canGoBack(uint64_t tabId) const noexcept {
    const TabState* tab = getTab(tabId);
    return tab && !tab->backHistory.empty();
}

bool NavigationCoordinator::canGoForward(uint64_t tabId) const noexcept {
    const TabState* tab = getTab(tabId);
    return tab && !tab->forwardHistory.empty();
}

bool NavigationCoordinator::canGoUp(uint64_t tabId) const noexcept {
    const TabState* tab = getTab(tabId);
    if (!tab) return false;
    return computeParentUri(tab->providerUri) != tab->providerUri;
}

std::string NavigationCoordinator::computeParentUri(std::string_view uri) {
    if (uri.empty() || uri == "godxplr://this-pc") {
        return "godxplr://this-pc";
    }

    // Standardize separators
    std::string path(uri);
    for (char& c : path) {
        if (c == '/') c = '\\';
    }

    // Strip trailing slashes
    while (path.size() > 3 && path.back() == '\\') {
        path.pop_back();
    }

    // If drive root (e.g. "C:\\" or "C:"), go up to "This PC"
    if (path.size() <= 3 && path[1] == ':') {
        return "godxplr://this-pc";
    }

    size_t lastSep = path.find_last_of('\\');
    if (lastSep == std::string::npos) {
        return "godxplr://this-pc";
    }

    // E.g. "C:\Windows" -> "C:\"
    if (lastSep == 2 && path[1] == ':') {
        return path.substr(0, 3);
    }

    return path.substr(0, lastSep);
}

std::string NavigationCoordinator::formatTabTitle(std::string_view uri) {
    if (uri.empty() || uri == "godxplr://this-pc") {
        return "This PC";
    }
    if (uri == "godxplr://trash") {
        return "Recycle Bin";
    }

    std::string path(uri);
    for (char& c : path) {
        if (c == '/') c = '\\';
    }

    while (path.size() > 3 && path.back() == '\\') {
        path.pop_back();
    }

    if (path.size() <= 3 && path[1] == ':') {
        std::string s = "Local Disk (";
        s.push_back(path[0]);
        s.append(":)");
        return s;
    }

    size_t lastSep = path.find_last_of('\\');
    if (lastSep != std::string::npos && lastSep + 1 < path.size()) {
        return path.substr(lastSep + 1);
    }

    return path;
}

} // namespace gx::ui
