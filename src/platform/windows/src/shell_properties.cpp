#include "godxplr/platform/shell_properties.h"
#include "godxplr/platform/com_apartment.h"
#include <shlobj.h>
#include <thread>

namespace gx::platform {

bool ShellProperties::showFileProperties(HWND hwndParent, const std::wstring& filePath) {
    if (filePath.empty()) {
        return false;
    }

    std::thread([hwndParent, filePath]() {
        win32::ComInitializer comInit(COINIT_APARTMENTTHREADED);
        SHObjectProperties(hwndParent, SHOP_FILEPATH, filePath.c_str(), nullptr);
    }).detach();

    return true;
}

} // namespace gx::platform
