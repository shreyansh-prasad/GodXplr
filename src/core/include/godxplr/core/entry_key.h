#pragma once

#include <cstdint>
#include <compare>
#include <functional>
#include <string>

namespace gx {

enum class ProviderKind : uint8_t {
    NtfsVolume       = 0,
    Win32Directory   = 1,
    ArchiveVirtual   = 2,
    NetworkShare     = 3,
    CloudPlaceholder = 4,
    MtpDevice        = 5,
    SearchResult     = 6
};

struct EntryKey {
    ProviderKind provider{ProviderKind::NtfsVolume};
    uint64_t volumeOrProviderId{0};
    uint64_t objectId{0};
    uint64_t incarnation{0};

    constexpr auto operator<=>(const EntryKey&) const = default;
};

// Formats an EntryKey into a human-readable diagnostic string
std::string to_string(const EntryKey& key);

} // namespace gx

// Specialization for std::hash to allow EntryKey in unordered_map/set
template <>
struct std::hash<gx::EntryKey> {
    size_t operator()(const gx::EntryKey& k) const noexcept {
        size_t h1 = std::hash<uint8_t>{}(static_cast<uint8_t>(k.provider));
        size_t h2 = std::hash<uint64_t>{}(k.volumeOrProviderId);
        size_t h3 = std::hash<uint64_t>{}(k.objectId);
        size_t h4 = std::hash<uint64_t>{}(k.incarnation);
        // Boost-style hash combine
        size_t seed = h1;
        seed ^= h2 + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        seed ^= h3 + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        seed ^= h4 + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        return seed;
    }
};
