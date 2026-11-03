#include "godxplr/platform/shell_drag_drop.h"
#include <shlobj.h>

namespace gx::platform {

HGLOBAL ShellDragDrop::createHdropPayload(const std::vector<std::wstring>& paths) {
    if (paths.empty()) {
        return nullptr;
    }

    size_t charsTotal = 0;
    for (const auto& p : paths) {
        charsTotal += p.size() + 1; // including null terminator
    }
    charsTotal += 1; // double null terminator

    size_t totalBytes = sizeof(DROPFILES) + charsTotal * sizeof(wchar_t);

    HGLOBAL hGlobal = GlobalAlloc(GHND, totalBytes);
    if (!hGlobal) {
        return nullptr;
    }

    auto* df = reinterpret_cast<DROPFILES*>(GlobalLock(hGlobal));
    if (!df) {
        GlobalFree(hGlobal);
        return nullptr;
    }

    df->pFiles = sizeof(DROPFILES);
    df->pt = { 0, 0 };
    df->fNC = FALSE;
    df->fWide = TRUE; // Unicode

    auto* dest = reinterpret_cast<wchar_t*>(reinterpret_cast<uint8_t*>(df) + sizeof(DROPFILES));
    for (const auto& p : paths) {
        memcpy(dest, p.data(), p.size() * sizeof(wchar_t));
        dest += p.size();
        *dest++ = L'\0';
    }
    *dest = L'\0'; // Final double null

    GlobalUnlock(hGlobal);
    return hGlobal;
}

std::vector<std::wstring> ShellDragDrop::extractHdropPayload(HGLOBAL hDrop) {
    if (!hDrop) {
        return {};
    }

    HDROP hDropHandle = reinterpret_cast<HDROP>(GlobalLock(hDrop));
    if (!hDropHandle) {
        return {};
    }

    UINT count = DragQueryFileW(hDropHandle, 0xFFFFFFFF, nullptr, 0);
    std::vector<std::wstring> result;
    result.reserve(count);

    for (UINT i = 0; i < count; ++i) {
        UINT len = DragQueryFileW(hDropHandle, i, nullptr, 0);
        if (len > 0) {
            std::vector<wchar_t> buf(len + 1, 0);
            DragQueryFileW(hDropHandle, i, buf.data(), len + 1);
            result.emplace_back(buf.data());
        }
    }

    GlobalUnlock(hDrop);
    return result;
}

DragDropEffect ShellDragDrop::calculateDropEffect(
    DWORD keyState,
    bool isSameVolume)
{
    // Ctrl + Shift or Alt indicates Link / Shortcut creation
    if (((keyState & MK_CONTROL) && (keyState & MK_SHIFT)) || (keyState & MK_ALT)) {
        return DragDropEffect::Link;
    }

    // Ctrl forces Copy
    if (keyState & MK_CONTROL) {
        return DragDropEffect::Copy;
    }

    // Shift forces Move
    if (keyState & MK_SHIFT) {
        return DragDropEffect::Move;
    }

    // Default: same volume moves, cross volume copies
    return isSameVolume ? DragDropEffect::Move : DragDropEffect::Copy;
}

} // namespace gx::platform
