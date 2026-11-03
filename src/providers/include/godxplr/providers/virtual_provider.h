#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <memory>

namespace gx::providers {

enum class ProviderCapability : uint32_t {
    None         = 0,
    Enumerate    = 1 << 0,
    ReadStream   = 1 << 1,
    WriteStream  = 1 << 2,
    RandomSeek   = 1 << 3
};

inline constexpr ProviderCapability operator|(ProviderCapability a, ProviderCapability b) noexcept {
    return static_cast<ProviderCapability>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline constexpr ProviderCapability operator&(ProviderCapability a, ProviderCapability b) noexcept {
    return static_cast<ProviderCapability>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}

inline constexpr ProviderCapability operator~(ProviderCapability a) noexcept {
    return static_cast<ProviderCapability>(~static_cast<uint32_t>(a));
}

struct VirtualItem {
    std::wstring name;
    std::wstring relativePath;
    uint64_t uncompressedSize{0};
    uint64_t compressedSize{0};
    uint64_t mtime{0};
    bool isDirectory{false};
    bool isEncrypted{false};
    uint32_t crc32{0};
};

class IVirtualProvider {
public:
    virtual ~IVirtualProvider() = default;

    [[nodiscard]] virtual ProviderCapability capabilities() const noexcept = 0;
    virtual bool open(const std::wstring& containerPath) = 0;
    [[nodiscard]] virtual std::vector<VirtualItem> enumerateDirectory(const std::wstring& virtualSubDir = L"") = 0;
    virtual bool extractItem(const std::wstring& relativePath, const std::wstring& destDiskPath) = 0;
    virtual void close() = 0;
};

// Security path sanitizer to prevent path traversal attacks (e.g. "../", "C:\", leading slashes)
class PathSanitizer {
public:
    static bool isSafePath(const std::string& path);
    static std::wstring sanitizeRelativePath(const std::string& path);
};

} // namespace gx::providers
