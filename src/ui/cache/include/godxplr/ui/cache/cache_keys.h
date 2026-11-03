#pragma once

#include "godxplr/core/entry_key.h"
#include <string>
#include <string_view>
#include <cstdint>
#include <functional>

namespace gx::ui {

enum class ThumbnailCropMode : uint8_t {
    Contain,
    Cover
};

struct IconKey {
    std::string extensionOrType; // Lowercase extension (e.g. "png", "cpp") or category ("folder", "drive")
    bool isDirectory{false};
    bool isDrive{false};
    uint16_t sizeBucket{24};     // 16, 24, 32, 48, 64
    uint8_t dpiBucket{100};      // 100, 125, 150, 175, 200
    uint8_t themeMode{0};        // 0: Light, 1: Dark, 2: HighContrast

    auto operator<=>(const IconKey&) const = default;
};

struct ThumbnailKey {
    EntryKey entryKey{};
    uint64_t mtime{0};
    uint64_t size{0};
    uint16_t targetWidth{128};
    uint16_t targetHeight{128};
    ThumbnailCropMode cropMode{ThumbnailCropMode::Contain};
    uint8_t dpiBucket{100};

    auto operator<=>(const ThumbnailKey&) const = default;
};

} // namespace gx::ui

template <>
struct std::hash<gx::ui::IconKey> {
    size_t operator()(const gx::ui::IconKey& k) const noexcept {
        size_t seed = std::hash<std::string>{}(k.extensionOrType);
        seed ^= std::hash<bool>{}(k.isDirectory) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        seed ^= std::hash<bool>{}(k.isDrive) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        seed ^= std::hash<uint16_t>{}(k.sizeBucket) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        seed ^= std::hash<uint8_t>{}(k.dpiBucket) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        seed ^= std::hash<uint8_t>{}(k.themeMode) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        return seed;
    }
};

template <>
struct std::hash<gx::ui::ThumbnailKey> {
    size_t operator()(const gx::ui::ThumbnailKey& k) const noexcept {
        size_t seed = std::hash<gx::EntryKey>{}(k.entryKey);
        seed ^= std::hash<uint64_t>{}(k.mtime) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        seed ^= std::hash<uint64_t>{}(k.size) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        seed ^= std::hash<uint16_t>{}(k.targetWidth) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        seed ^= std::hash<uint16_t>{}(k.targetHeight) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        seed ^= std::hash<uint8_t>{}(static_cast<uint8_t>(k.cropMode)) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        seed ^= std::hash<uint8_t>{}(k.dpiBucket) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        return seed;
    }
};

