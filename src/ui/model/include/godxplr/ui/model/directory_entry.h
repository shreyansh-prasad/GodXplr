#pragma once

#include "godxplr/core/entry_key.h"
#include <cstdint>
#include <string_view>

namespace gx::ui {

struct NameSlice {
    uint32_t offset{0};
    uint32_t length{0};

    auto operator<=>(const NameSlice&) const = default;
};

enum class MetaState : uint8_t {
    Unknown,
    Queued,
    Known,
    Denied,
    Failed
};

constexpr uint32_t kEntryDir     = 0x0001;
constexpr uint32_t kEntryHidden  = 0x0002;
constexpr uint32_t kEntrySystem  = 0x0004;
constexpr uint32_t kEntryReparse = 0x0008;
constexpr uint32_t kEntryOffline = 0x0010;

#pragma pack(push, 8)
struct DirectoryEntry {
    EntryKey key{};
    NameSlice name{};
    uint64_t size{0};
    uint64_t mtime{0};
    uint32_t attributes{0};
    uint32_t flags{0};
    MetaState sizeState{MetaState::Unknown};
    MetaState timeState{MetaState::Unknown};
    uint8_t pad[2]{0};
    uint32_t iconKey{0};

    [[nodiscard]] bool isDirectory() const noexcept { return (flags & kEntryDir) != 0; }
    [[nodiscard]] bool isHidden() const noexcept { return (flags & kEntryHidden) != 0; }
    [[nodiscard]] bool isSystem() const noexcept { return (flags & kEntrySystem) != 0; }
};
#pragma pack(pop)

static_assert(sizeof(DirectoryEntry) <= 72, "DirectoryEntry exceeds 72-byte compact limit");

} // namespace gx::ui
