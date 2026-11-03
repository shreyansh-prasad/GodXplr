#pragma once

#ifdef _WIN32

#include "godxplr/core/pixel_buffer.h"
#include <string>
#include <cstdint>
#include <optional>

namespace gx::platform::win32 {

enum class ThumbnailExtractionFlags : uint32_t {
    Default = 0,
    MemoryOnly = 1 << 0,     // SIIGBF_MEMORYONLY: Return only if already cached in memory
    InCacheOnly = 1 << 1,    // SIIGBF_INCACHEONLY: Return only if cached in memory or disk cache
    ThumbnailOnly = 1 << 2,  // SIIGBF_THUMBNAILONLY: Do not fallback to file-type icon
    BiggerSizeOk = 1 << 3,   // SIIGBF_BIGGERSIZEOK: Allow returning larger image without downscale
};

class ShellThumbnailProvider {
public:
    ShellThumbnailProvider() = delete;

    // Extracts a content thumbnail or high-quality file icon for the given file path.
    // MUST be called on an STA thread (such as via StaWorkerThread), NEVER on GUI thread!
    static std::optional<core::PixelBuffer> extractThumbnail(
        const std::wstring& filePath,
        uint32_t targetWidth,
        uint32_t targetHeight,
        ThumbnailExtractionFlags flags = ThumbnailExtractionFlags::Default);

    // Extracts a stock file or directory icon via SHGetFileInfoW without reading file content.
    static std::optional<core::PixelBuffer> extractIcon(
        const std::wstring& extensionOrPath,
        bool isDirectory,
        uint32_t sizePx);
};

} // namespace gx::platform::win32

#endif // _WIN32
