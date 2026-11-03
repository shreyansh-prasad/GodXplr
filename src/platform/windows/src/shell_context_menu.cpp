#include "godxplr/platform/shell_context_menu.h"

namespace gx::platform {

ShellContextMenu::~ShellContextMenu() {
    clear();
}

void ShellContextMenu::clear() {
    if (hMenu_) {
        DestroyMenu(hMenu_);
        hMenu_ = nullptr;
    }
    if (pcm3_) {
        pcm3_->Release();
        pcm3_ = nullptr;
    }
    if (pcm2_) {
        pcm2_->Release();
        pcm2_ = nullptr;
    }
    if (pcm_) {
        pcm_->Release();
        pcm_ = nullptr;
    }
}

bool ShellContextMenu::populate(const std::vector<std::wstring>& paths) {
    clear();
    if (paths.empty()) {
        return false;
    }

    PIDLIST_ABSOLUTE pidl = nullptr;
    HRESULT hr = SHParseDisplayName(paths[0].c_str(), nullptr, &pidl, 0, nullptr);
    if (FAILED(hr) || !pidl) {
        return false;
    }

    IShellFolder* psfParent = nullptr;
    PCUITEMID_CHILD pidlChild = nullptr;
    hr = SHBindToParent(pidl, IID_IShellFolder, reinterpret_cast<void**>(&psfParent), &pidlChild);
    if (FAILED(hr) || !psfParent) {
        CoTaskMemFree(pidl);
        return false;
    }

    PCUITEMID_CHILD childArray[1] = { pidlChild };
    hr = psfParent->GetUIObjectOf(
        nullptr,
        1,
        childArray,
        IID_IContextMenu,
        nullptr,
        reinterpret_cast<void**>(&pcm_));

    if (FAILED(hr) || !pcm_) {
        psfParent->Release();
        CoTaskMemFree(pidl);
        return false;
    }

    pcm_->QueryInterface(IID_IContextMenu3, reinterpret_cast<void**>(&pcm3_));
    pcm_->QueryInterface(IID_IContextMenu2, reinterpret_cast<void**>(&pcm2_));

    hMenu_ = CreatePopupMenu();
    hr = pcm_->QueryContextMenu(
        hMenu_,
        0,
        1,
        0x7FFF,
        CMF_NORMAL | CMF_CANRENAME | CMF_EXPLORE);

    psfParent->Release();
    CoTaskMemFree(pidl);

    return SUCCEEDED(hr);
}

std::vector<ShellMenuItem> ShellContextMenu::getMenuItems() const {
    if (!hMenu_) {
        return {};
    }

    std::vector<ShellMenuItem> items;
    int count = GetMenuItemCount(hMenu_);
    if (count <= 0) {
        return {};
    }

    items.reserve(count);

    for (int i = 0; i < count; ++i) {
        MENUITEMINFOW mii{};
        mii.cbSize = sizeof(mii);
        mii.fMask = MIIM_ID | MIIM_STRING | MIIM_FTYPE | MIIM_SUBMENU | MIIM_STATE;

        // First probe length
        if (GetMenuItemInfoW(hMenu_, i, TRUE, &mii)) {
            ShellMenuItem item;
            item.id = mii.wID;
            item.isSeparator = (mii.fType & MFT_SEPARATOR) != 0;
            item.isSubmenu = (mii.hSubMenu != nullptr);
            item.isDefault = (mii.fState & MFS_DEFAULT) != 0;

            if (mii.cch > 0) {
                std::vector<wchar_t> textBuf(mii.cch + 1, 0);
                mii.dwTypeData = textBuf.data();
                mii.cch++;
                if (GetMenuItemInfoW(hMenu_, i, TRUE, &mii)) {
                    item.text = textBuf.data();
                }
            }

            items.push_back(std::move(item));
        }
    }

    return items;
}

bool ShellContextMenu::showAndInvoke(HWND hwndParent, int x, int y) {
    if (!hMenu_ || !pcm_) {
        return false;
    }

    UINT cmd = TrackPopupMenuEx(
        hMenu_,
        TPM_RETURNCMD | TPM_RIGHTBUTTON,
        x,
        y,
        hwndParent,
        nullptr);

    if (cmd == 0) {
        return false; // User cancelled menu
    }

    CMINVOKECOMMANDINFOEX ici{};
    ici.cbSize = sizeof(ici);
    ici.fMask = CMIC_MASK_UNICODE;
    ici.hwnd = hwndParent;
    ici.lpVerb = MAKEINTRESOURCEA(cmd - 1);
    ici.lpVerbW = MAKEINTRESOURCEW(cmd - 1);
    ici.nShow = SW_SHOWNORMAL;

    HRESULT hr = pcm_->InvokeCommand(reinterpret_cast<LPCMINVOKECOMMANDINFO>(&ici));
    return SUCCEEDED(hr);
}

} // namespace gx::platform
