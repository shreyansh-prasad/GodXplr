#pragma once

#include "godxplr/core/entry_key.h"
#include <string>
#include <vector>
#include <cstdint>

namespace gx::ui {

enum class SidebarSectionKind : uint8_t {
    QuickAccess,
    Pinned,
    Drives,
    Network
};

struct SidebarItem {
    std::string id;
    std::string displayName;
    std::string uri;
    EntryKey key{};
    SidebarSectionKind section{SidebarSectionKind::QuickAccess};
    bool isRemovable{false};
};

struct SidebarSection {
    SidebarSectionKind kind{SidebarSectionKind::QuickAccess};
    std::string title;
    std::vector<SidebarItem> items;
};

class SidebarModel {
public:
    SidebarModel();
    ~SidebarModel() = default;

    // Populates Quick Access, system drives, and initial places
    void refresh();

    // User pin management
    void pinLocation(std::string displayName, std::string uri, const EntryKey& key);
    void unpinLocation(const std::string& uri);

    [[nodiscard]] const std::vector<SidebarSection>& sections() const noexcept { return sections_; }
    [[nodiscard]] const SidebarItem* findItemByUri(std::string_view uri) const noexcept;

    // Win32 volume discovery helper
    static std::vector<SidebarItem> detectWindowsDrives();

private:
    std::vector<SidebarSection> sections_;
};

} // namespace gx::ui
