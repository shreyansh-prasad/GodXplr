#pragma once

#include <string>
#include <vector>
#include <cstdint>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shellapi.h>

namespace gx::platform {

enum class DragDropEffect : uint32_t {
    None = 0,
    Copy = 1,
    Move = 2,
    Link = 4
};

class ShellDragDrop {
public:
    // Creates an HGLOBAL memory buffer containing a CF_HDROP DROPFILES payload
    static HGLOBAL createHdropPayload(const std::vector<std::wstring>& paths);

    // Extracts list of file paths from an HGLOBAL CF_HDROP payload
    static std::vector<std::wstring> extractHdropPayload(HGLOBAL hDrop);

    // Calculates appropriate drop effect based on key modifiers and volume comparison
    static DragDropEffect calculateDropEffect(
        DWORD keyState,
        bool isSameVolume);
};

} // namespace gx::platform
