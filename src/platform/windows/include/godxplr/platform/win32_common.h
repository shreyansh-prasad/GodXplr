#pragma once

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <string>
#include <string_view>

namespace gx::platform::win32 {

// Safe RAII wrapper for Windows HANDLE (closes via CloseHandle)
class ScopedHandle {
public:
    constexpr ScopedHandle() noexcept : handle_(INVALID_HANDLE_VALUE) {}
    explicit ScopedHandle(HANDLE h) noexcept : handle_(h) {}
    ~ScopedHandle() { reset(); }

    ScopedHandle(const ScopedHandle&) = delete;
    ScopedHandle& operator=(const ScopedHandle&) = delete;

    ScopedHandle(ScopedHandle&& other) noexcept : handle_(other.handle_) {
        other.handle_ = INVALID_HANDLE_VALUE;
    }

    ScopedHandle& operator=(ScopedHandle&& other) noexcept {
        if (this != &other) {
            reset();
            handle_ = other.handle_;
            other.handle_ = INVALID_HANDLE_VALUE;
        }
        return *this;
    }

    [[nodiscard]] bool isValid() const noexcept {
        return handle_ != INVALID_HANDLE_VALUE && handle_ != nullptr;
    }

    [[nodiscard]] HANDLE get() const noexcept { return handle_; }

    void reset(HANDLE h = INVALID_HANDLE_VALUE) noexcept {
        if (isValid()) {
            ::CloseHandle(handle_);
        }
        handle_ = h;
    }

    HANDLE release() noexcept {
        HANDLE temp = handle_;
        handle_ = INVALID_HANDLE_VALUE;
        return temp;
    }

private:
    HANDLE handle_;
};

// UTF-8 <-> UTF-16 conversions
std::wstring utf8_to_utf16(std::string_view utf8);
std::string utf16_to_utf8(std::wstring_view utf16);

} // namespace gx::platform::win32

#endif // _WIN32
