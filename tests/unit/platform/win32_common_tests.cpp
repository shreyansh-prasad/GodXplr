#include "godxplr/platform/win32_common.h"
#include <cassert>
#include <iostream>

#ifdef _WIN32

void test_utf_conversion() {
    std::string originalUtf8 = "Hello, World! 🚀 Привет мир! 日本語";
    std::wstring utf16 = gx::platform::win32::utf8_to_utf16(originalUtf8);
    assert(!utf16.empty());

    std::string roundTrip = gx::platform::win32::utf16_to_utf8(utf16);
    assert(originalUtf8 == roundTrip);
    std::cout << "[PASS] test_utf_conversion\n";
}

void test_scoped_handle() {
    gx::platform::win32::ScopedHandle handle(
        ::CreateEventW(nullptr, TRUE, FALSE, nullptr)
    );
    assert(handle.isValid());
    HANDLE raw = handle.get();
    assert(raw != INVALID_HANDLE_VALUE);

    handle.reset();
    assert(!handle.isValid());
    std::cout << "[PASS] test_scoped_handle\n";
}

int main() {
    std::cout << "Running Win32 platform common tests...\n";
    test_utf_conversion();
    test_scoped_handle();
    std::cout << "All Win32 platform common tests passed.\n";
    return 0;
}

#else

int main() {
    std::cout << "Skipping Win32 platform tests on non-Windows.\n";
    return 0;
}

#endif
