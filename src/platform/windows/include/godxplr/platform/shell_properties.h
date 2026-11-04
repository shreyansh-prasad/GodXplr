#pragma once

#include <string>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace gx::platform {

class ShellProperties {
public:
    // Opens the genuine Windows File Properties dialog on a background STA thread
    static bool showFileProperties(HWND hwndParent, const std::wstring& filePath);
};

} // namespace gx::platform

