#pragma once

#ifdef _WIN32

#include "godxplr/core/provider_contract.h"
#include "godxplr/platform/win32_common.h"
#include <memory>
#include <string>
#include <thread>
#include <atomic>

namespace gx::platform::win32 {

class Win32DirectoryWatcher final : public IDirectoryWatcher {
public:
    Win32DirectoryWatcher(std::wstring directoryPath, std::shared_ptr<IWatchListener> listener);
    ~Win32DirectoryWatcher() override;

    [[nodiscard]] bool isActive() const noexcept override;
    void stop() override;

private:
    void watchWorker();

    std::wstring path_;
    std::shared_ptr<IWatchListener> listener_;
    ScopedHandle dirHandle_;
    ScopedHandle stopEvent_;
    std::thread workerThread_;
    std::atomic<bool> active_{false};
};

class Win32DirectoryProvider final : public IDirectoryProvider {
public:
    Win32DirectoryProvider();
    ~Win32DirectoryProvider() override = default;

    [[nodiscard]] ProviderKind kind() const noexcept override {
        return ProviderKind::Win32Directory;
    }

    [[nodiscard]] ProviderCapabilities capabilities() const noexcept override {
        return ProviderCapabilities::CanEnumerate |
               ProviderCapabilities::CanWatch |
               ProviderCapabilities::CanStat |
               ProviderCapabilities::IsLocalFs;
    }

    [[nodiscard]] bool canHandleUri(std::string_view uri) const noexcept override;

    ProviderStatus enumerate(
        std::string_view uri,
        uint64_t requestId,
        uint64_t generation,
        std::shared_ptr<CancellationToken> cancelToken,
        std::function<bool(DirectoryBatch&&)> batchCallback) override;

    std::unique_ptr<IDirectoryWatcher> watch(
        std::string_view uri,
        std::shared_ptr<IWatchListener> listener) override;

    // Translates URI (file:///C:/path or C:\path) into normalized Win32 native path
    static std::wstring uriToNativePath(std::string_view uri);

    // Resolves volume serial number for stable EntryKey generation
    static uint64_t getVolumeSerial(const std::wstring& nativePath);
};

} // namespace gx::platform::win32

#endif // _WIN32
