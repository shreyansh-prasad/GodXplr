#include "godxplr/ui/shell/sidebar_model.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

#include <algorithm>

namespace gx::ui {

SidebarModel::SidebarModel() {
    refresh();
}

void SidebarModel::refresh() {
    std::vector<SidebarItem> pinnedItems;
    for (const auto& sec : sections_) {
        if (sec.kind == SidebarSectionKind::Pinned) {
            pinnedItems = sec.items;
            break;
        }
    }

    sections_.clear();

    // 1. Quick Access
    SidebarSection qaSec;
    qaSec.kind = SidebarSectionKind::QuickAccess;
    qaSec.title = "Quick Access";

    auto addQA = [&](std::string id, std::string name, std::string uri, uint64_t objId) {
        SidebarItem item;
        item.id = std::move(id);
        item.displayName = std::move(name);
        item.uri = std::move(uri);
        item.key = EntryKey{.provider = ProviderKind::Win32Directory, .volumeOrProviderId = 1, .objectId = objId, .incarnation = 0};
        item.section = SidebarSectionKind::QuickAccess;
        qaSec.items.push_back(std::move(item));
    };

    addQA("home", "Home", "godxplr://home", 1);
    addQA("desktop", "Desktop", "godxplr://desktop", 2);
    addQA("downloads", "Downloads", "godxplr://downloads", 3);
    addQA("documents", "Documents", "godxplr://documents", 4);
    addQA("pictures", "Pictures", "godxplr://pictures", 5);
    addQA("music", "Music", "godxplr://music", 6);
    addQA("videos", "Videos", "godxplr://videos", 7);

    sections_.push_back(std::move(qaSec));

    // 2. Pinned
    SidebarSection pinnedSec;
    pinnedSec.kind = SidebarSectionKind::Pinned;
    pinnedSec.title = "Pinned";
    pinnedSec.items = std::move(pinnedItems);
    sections_.push_back(std::move(pinnedSec));

    // 3. Drives / This PC
    SidebarSection drivesSec;
    drivesSec.kind = SidebarSectionKind::Drives;
    drivesSec.title = "This PC";
    drivesSec.items = detectWindowsDrives();
    sections_.push_back(std::move(drivesSec));

    // 4. Network
    SidebarSection netSec;
    netSec.kind = SidebarSectionKind::Network;
    netSec.title = "Network";
    SidebarItem netItem;
    netItem.id = "network_root";
    netItem.displayName = "Network";
    netItem.uri = "godxplr://network";
    netItem.key = EntryKey{.provider = ProviderKind::NetworkShare, .volumeOrProviderId = 0, .objectId = 1, .incarnation = 0};
    netItem.section = SidebarSectionKind::Network;
    netSec.items.push_back(std::move(netItem));
    sections_.push_back(std::move(netSec));
}

void SidebarModel::pinLocation(std::string displayName, std::string uri, const EntryKey& key) {
    for (auto& sec : sections_) {
        if (sec.kind == SidebarSectionKind::Pinned) {
            SidebarItem item;
            item.id = "pin_" + std::to_string(sec.items.size() + 1);
            item.displayName = std::move(displayName);
            item.uri = std::move(uri);
            item.key = key;
            item.section = SidebarSectionKind::Pinned;
            sec.items.push_back(std::move(item));
            break;
        }
    }
}

void SidebarModel::unpinLocation(const std::string& uri) {
    for (auto& sec : sections_) {
        if (sec.kind == SidebarSectionKind::Pinned) {
            auto it = std::remove_if(sec.items.begin(), sec.items.end(), [&](const SidebarItem& item) {
                return item.uri == uri;
            });
            sec.items.erase(it, sec.items.end());
            break;
        }
    }
}

const SidebarItem* SidebarModel::findItemByUri(std::string_view uri) const noexcept {
    for (const auto& sec : sections_) {
        for (const auto& item : sec.items) {
            if (item.uri == uri) {
                return &item;
            }
        }
    }
    return nullptr;
}

std::vector<SidebarItem> SidebarModel::detectWindowsDrives() {
    std::vector<SidebarItem> drives;

#ifdef _WIN32
    WCHAR driveStrings[512]{};
    DWORD len = ::GetLogicalDriveStringsW(511, driveStrings);
    if (len > 0 && len <= 511) {
        const WCHAR* ptr = driveStrings;
        while (*ptr) {
            std::wstring rootPath = ptr;
            UINT driveType = ::GetDriveTypeW(rootPath.c_str());

            WCHAR volName[MAX_PATH + 1]{};
            DWORD serial = 0;
            ::GetVolumeInformationW(
                rootPath.c_str(),
                volName,
                MAX_PATH,
                &serial,
                nullptr,
                nullptr,
                nullptr,
                0
            );

            std::string driveLetter;
            if (rootPath.size() >= 2 && rootPath[1] == L':') {
                driveLetter.push_back(static_cast<char>(rootPath[0]));
            } else {
                driveLetter = "C";
            }

            std::string label;
            if (volName[0] != L'\0') {
                int sizeNeeded = ::WideCharToMultiByte(CP_UTF8, 0, volName, -1, nullptr, 0, nullptr, nullptr);
                if (sizeNeeded > 1) {
                    label.resize(sizeNeeded - 1);
                    ::WideCharToMultiByte(CP_UTF8, 0, volName, -1, label.data(), sizeNeeded, nullptr, nullptr);
                    label += " (" + driveLetter + ":)";
                }
            }

            if (label.empty()) {
                label = "Local Disk (" + driveLetter + ":)";
            }

            SidebarItem item;
            item.id = "drive_" + driveLetter;
            item.displayName = std::move(label);
            item.uri = "file:///" + driveLetter + ":/";
            item.key = EntryKey{
                .provider = ProviderKind::Win32Directory,
                .volumeOrProviderId = static_cast<uint64_t>(serial),
                .objectId = 0,
                .incarnation = 0
            };
            item.section = SidebarSectionKind::Drives;
            item.isRemovable = (driveType == DRIVE_REMOVABLE || driveType == DRIVE_CDROM);

            drives.push_back(std::move(item));

            ptr += wcslen(ptr) + 1;
        }
    }
#else
    // Fallback for non-Windows builds
    SidebarItem root;
    root.id = "drive_root";
    root.displayName = "System Root (/)";
    root.uri = "file:///";
    root.key = EntryKey{.provider = ProviderKind::Win32Directory, .volumeOrProviderId = 1, .objectId = 0, .incarnation = 0};
    root.section = SidebarSectionKind::Drives;
    drives.push_back(std::move(root));
#endif

    return drives;
}

} // namespace gx::ui
