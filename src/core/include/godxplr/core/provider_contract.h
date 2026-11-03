#pragma once

#include "godxplr/core/entry_key.h"
#include "godxplr/core/directory_entry.h"
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <memory>
#include <functional>
#include <atomic>

namespace gx {

enum class ProviderCapabilities : uint32_t {
    None             = 0,
    CanEnumerate     = 1 << 0,
    CanWatch         = 1 << 1,
    CanStat          = 1 << 2,
    IsLocalFs        = 1 << 3,
    IsSlowNetwork    = 1 << 4,
    IsReadOnly       = 1 << 5,
    SupportsFastMft  = 1 << 6
};

inline constexpr ProviderCapabilities operator|(ProviderCapabilities a, ProviderCapabilities b) noexcept {
    return static_cast<ProviderCapabilities>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline constexpr ProviderCapabilities operator&(ProviderCapabilities a, ProviderCapabilities b) noexcept {
    return static_cast<ProviderCapabilities>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}

inline constexpr bool hasCapability(ProviderCapabilities caps, ProviderCapabilities flag) noexcept {
    return (caps & flag) == flag;
}

enum class ProviderStatus : uint8_t {
    Ready,
    Loading,
    Empty,
    NotFound,
    PermissionDenied,
    DeviceNotReady,
    Offline,
    Cancelled,
    Error
};

inline const char* to_string(ProviderStatus status) noexcept {
    switch (status) {
        case ProviderStatus::Ready: return "Ready";
        case ProviderStatus::Loading: return "Loading";
        case ProviderStatus::Empty: return "Empty";
        case ProviderStatus::NotFound: return "NotFound";
        case ProviderStatus::PermissionDenied: return "PermissionDenied";
        case ProviderStatus::DeviceNotReady: return "DeviceNotReady";
        case ProviderStatus::Offline: return "Offline";
        case ProviderStatus::Cancelled: return "Cancelled";
        case ProviderStatus::Error: return "Error";
    }
    return "Unknown";
}

class CancellationToken {
public:
    explicit CancellationToken() = default;

    void cancel() noexcept {
        cancelled_.store(true, std::memory_order_relaxed);
    }

    [[nodiscard]] bool isCancelled() const noexcept {
        return cancelled_.load(std::memory_order_relaxed);
    }

    void reset() noexcept {
        cancelled_.store(false, std::memory_order_relaxed);
    }

private:
    std::atomic<bool> cancelled_{false};
};

struct DirectoryBatch {
    uint64_t requestId{0};
    uint64_t generation{0};
    std::vector<DirectoryEntry> entries;
    std::string nameArena;
    bool isFinal{false};
    ProviderStatus status{ProviderStatus::Ready};
    std::string errorMessage;

    [[nodiscard]] std::string_view getName(const DirectoryEntry& entry) const noexcept {
        if (entry.name.offset + entry.name.length <= nameArena.size()) {
            return std::string_view(nameArena.data() + entry.name.offset, entry.name.length);
        }
        return {};
    }
};

struct WatchEvent {
    enum class Kind : uint8_t {
        Added,
        Removed,
        Modified,
        RenamedOld,
        RenamedNew,
        RescanRequired
    };

    Kind kind{Kind::Modified};
    std::string relativePath;
    std::string newRelativePath;
    bool overflowRequiresRescan{false};
};

class IWatchListener {
public:
    virtual ~IWatchListener() = default;
    virtual void onWatchEvent(const WatchEvent& event) = 0;
};

class IDirectoryWatcher {
public:
    virtual ~IDirectoryWatcher() = default;
    [[nodiscard]] virtual bool isActive() const noexcept = 0;
    virtual void stop() = 0;
};

class IDirectoryProvider {
public:
    virtual ~IDirectoryProvider() = default;

    [[nodiscard]] virtual ProviderKind kind() const noexcept = 0;
    [[nodiscard]] virtual ProviderCapabilities capabilities() const noexcept = 0;
    [[nodiscard]] virtual bool canHandleUri(std::string_view uri) const noexcept = 0;

    // Synchronous worker execution that emits batches through batchCallback.
    // Callback returns false to request early termination of enumeration.
    virtual ProviderStatus enumerate(
        std::string_view uri,
        uint64_t requestId,
        uint64_t generation,
        std::shared_ptr<CancellationToken> cancelToken,
        std::function<bool(DirectoryBatch&&)> batchCallback) = 0;

    virtual std::unique_ptr<IDirectoryWatcher> watch(
        std::string_view uri,
        std::shared_ptr<IWatchListener> listener) = 0;
};

} // namespace gx
