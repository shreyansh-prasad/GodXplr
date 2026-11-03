#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <shobjidl.h>
#include <shellapi.h>
#include <thumbcache.h>
#include "godxplr/platform/shell_thumbnail_provider.h"
#include <cmath>

namespace gx::platform::win32 {

namespace {

core::PixelBuffer hbitmapToPixelBuffer(HBITMAP hBitmap) {
    if (!hBitmap) {
        return {};
    }

    BITMAP bm{};
    if (::GetObjectW(hBitmap, sizeof(BITMAP), &bm) == 0) {
        return {};
    }

    uint32_t w = static_cast<uint32_t>(bm.bmWidth);
    uint32_t h = static_cast<uint32_t>(std::abs(bm.bmHeight));

    if (w == 0 || h == 0 || w > core::kMaxImageDimension || h > core::kMaxImageDimension) {
        return {};
    }

    core::PixelBuffer buffer;
    if (!buffer.allocate(w, h)) {
        return {};
    }

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = static_cast<LONG>(w);
    bmi.bmiHeader.biHeight = -static_cast<LONG>(h); // top-down DIB
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    HDC screenDc = ::GetDC(nullptr);
    if (!screenDc) {
        return {};
    }
    HDC memDc = ::CreateCompatibleDC(screenDc);
    if (!memDc) {
        ::ReleaseDC(nullptr, screenDc);
        return {};
    }

    int lines = ::GetDIBits(memDc, hBitmap, 0, h, buffer.data(), &bmi, DIB_RGB_COLORS);
    ::DeleteDC(memDc);
    ::ReleaseDC(nullptr, screenDc);

    if (lines <= 0) {
        return {};
    }

    // Windows Shell and GDI icons frequently produce 32-bit RGB bitmaps where alpha is 0x00.
    // If every pixel has alpha == 0, mark all pixels fully opaque (0xFF).
    uint32_t* px = reinterpret_cast<uint32_t*>(buffer.data());
    size_t count = static_cast<size_t>(w) * h;
    bool hasNonZeroAlpha = false;
    for (size_t i = 0; i < count; ++i) {
        if ((px[i] & 0xFF000000) != 0) {
            hasNonZeroAlpha = true;
            break;
        }
    }
    if (!hasNonZeroAlpha) {
        for (size_t i = 0; i < count; ++i) {
            px[i] |= 0xFF000000;
        }
    }

    return buffer;
}

} // namespace

std::optional<core::PixelBuffer> ShellThumbnailProvider::extractThumbnail(
    const std::wstring& filePath,
    uint32_t targetWidth,
    uint32_t targetHeight,
    ThumbnailExtractionFlags flags)
{
    if (filePath.empty() || targetWidth == 0 || targetHeight == 0) {
        return std::nullopt;
    }

    SIIGBF siigbf = SIIGBF_RESIZETOFIT;
    if ((static_cast<uint32_t>(flags) & static_cast<uint32_t>(ThumbnailExtractionFlags::MemoryOnly)) != 0) {
        siigbf |= SIIGBF_MEMORYONLY;
    }
    if ((static_cast<uint32_t>(flags) & static_cast<uint32_t>(ThumbnailExtractionFlags::InCacheOnly)) != 0) {
        siigbf |= SIIGBF_INCACHEONLY;
    }
    if ((static_cast<uint32_t>(flags) & static_cast<uint32_t>(ThumbnailExtractionFlags::ThumbnailOnly)) != 0) {
        siigbf |= SIIGBF_THUMBNAILONLY;
    }
    if ((static_cast<uint32_t>(flags) & static_cast<uint32_t>(ThumbnailExtractionFlags::BiggerSizeOk)) != 0) {
        siigbf |= SIIGBF_BIGGERSIZEOK;
    }

    IShellItem* pShellItem = nullptr;
    HRESULT hr = ::SHCreateItemFromParsingName(filePath.c_str(), nullptr, IID_PPV_ARGS(&pShellItem));
    if (FAILED(hr) || !pShellItem) {
        return std::nullopt;
    }

    IShellItemImageFactory* pFactory = nullptr;
    hr = pShellItem->QueryInterface(IID_PPV_ARGS(&pFactory));
    pShellItem->Release();

    if (FAILED(hr) || !pFactory) {
        return std::nullopt;
    }

    SIZE size{ static_cast<LONG>(targetWidth), static_cast<LONG>(targetHeight) };
    HBITMAP hBitmap = nullptr;
    hr = pFactory->GetImage(size, siigbf, &hBitmap);
    pFactory->Release();

    if (FAILED(hr) || !hBitmap) {
        return std::nullopt;
    }

    core::PixelBuffer result = hbitmapToPixelBuffer(hBitmap);
    ::DeleteObject(hBitmap);

    if (!result.isValid()) {
        return std::nullopt;
    }
    return result;
}

std::optional<core::PixelBuffer> ShellThumbnailProvider::extractIcon(
    const std::wstring& extensionOrPath,
    bool isDirectory,
    uint32_t sizePx)
{
    SHFILEINFOW sfi{};
    UINT flags = SHGFI_ICON | SHGFI_USEFILEATTRIBUTES;
    if (sizePx <= 16) {
        flags |= SHGFI_SMALLICON;
    } else {
        flags |= SHGFI_LARGEICON;
    }

    DWORD dwAttr = isDirectory ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL;
    DWORD_PTR res = ::SHGetFileInfoW(extensionOrPath.c_str(), dwAttr, &sfi, sizeof(sfi), flags);
    if (res == 0 || sfi.hIcon == nullptr) {
        return std::nullopt;
    }

    ICONINFO iconInfo{};
    if (!::GetIconInfo(sfi.hIcon, &iconInfo)) {
        ::DestroyIcon(sfi.hIcon);
        return std::nullopt;
    }

    HBITMAP hBmp = iconInfo.hbmColor ? iconInfo.hbmColor : iconInfo.hbmMask;
    core::PixelBuffer buf = hbitmapToPixelBuffer(hBmp);

    if (iconInfo.hbmColor) ::DeleteObject(iconInfo.hbmColor);
    if (iconInfo.hbmMask) ::DeleteObject(iconInfo.hbmMask);
    ::DestroyIcon(sfi.hIcon);

    if (!buf.isValid()) {
        return std::nullopt;
    }
    return buf;
}

} // namespace gx::platform::win32

#endif // _WIN32
