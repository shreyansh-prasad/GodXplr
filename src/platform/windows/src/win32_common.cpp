#include "godxplr/platform/win32_common.h"

#ifdef _WIN32

namespace gx::platform::win32 {

std::wstring utf8_to_utf16(std::string_view utf8) {
    if (utf8.empty()) {
        return {};
    }
    int len = ::MultiByteToWideChar(
        CP_UTF8,
        MB_ERR_INVALID_CHARS,
        utf8.data(),
        static_cast<int>(utf8.size()),
        nullptr,
        0
    );
    if (len <= 0) {
        return {};
    }
    std::wstring result(static_cast<size_t>(len), L'\0');
    ::MultiByteToWideChar(
        CP_UTF8,
        0,
        utf8.data(),
        static_cast<int>(utf8.size()),
        result.data(),
        len
    );
    return result;
}

std::string utf16_to_utf8(std::wstring_view utf16) {
    if (utf16.empty()) {
        return {};
    }
    int len = ::WideCharToMultiByte(
        CP_UTF8,
        0,
        utf16.data(),
        static_cast<int>(utf16.size()),
        nullptr,
        0,
        nullptr,
        nullptr
    );
    if (len <= 0) {
        return {};
    }
    std::string result(static_cast<size_t>(len), '\0');
    ::WideCharToMultiByte(
        CP_UTF8,
        0,
        utf16.data(),
        static_cast<int>(utf16.size()),
        result.data(),
        len,
        nullptr,
        nullptr
    );
    return result;
}

} // namespace gx::platform::win32

#endif // _WIN32
