#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <list>
#include <mutex>
#include <optional>

namespace gx::search {

struct UsageEntry {
    uint64_t pathHash{0};
    uint32_t openCount{0};
    uint64_t lastOpenTime{0};
    std::string lastQueryPrefix;
};

class UsageTracker {
public:
    explicit UsageTracker(size_t maxLruEntries = 16384);
    ~UsageTracker() = default;

    UsageTracker(const UsageTracker&) = delete;
    UsageTracker& operator=(const UsageTracker&) = delete;

    void recordOpen(uint64_t pathHash, std::string_view queryPrefix = "");
    std::optional<UsageEntry> getUsage(uint64_t pathHash);

    void clear();
    void setEnabled(bool enabled) noexcept { enabled_ = enabled; }
    [[nodiscard]] bool isEnabled() const noexcept { return enabled_; }
    [[nodiscard]] size_t size() const;

    // Stable 64-bit FNV-1a path hash (independent of runtime memory address or FRN)
    static uint64_t hashPath(std::string_view fullPath) noexcept;

private:
    const size_t maxEntries_;
    bool enabled_{true};
    mutable std::mutex mutex_;
    std::list<UsageEntry> lruList_;
    std::unordered_map<uint64_t, std::list<UsageEntry>::iterator> map_;
};

} // namespace gx::search

