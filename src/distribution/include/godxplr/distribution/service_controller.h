#pragma once

#include <string>
#include <cstdint>

namespace gx::distribution {

enum class ServiceStatus {
    NotFound,
    Stopped,
    StartPending,
    StopPending,
    Running,
    ContinuePending,
    PausePending,
    Paused,
    Unknown
};

class ServiceController {
public:
    static constexpr const wchar_t* kDefaultServiceName = L"GodXplrIndexService";
    static constexpr const wchar_t* kDefaultDisplayName = L"GodXplr Background Index Service";

    // Query current service status
    [[nodiscard]] static ServiceStatus queryStatus(const std::wstring& serviceName = kDefaultServiceName);

    // Install service
    static bool installService(
        const std::wstring& binaryPath,
        const std::wstring& serviceName = kDefaultServiceName,
        const std::wstring& displayName = kDefaultDisplayName);

    // Start service
    static bool startService(
        const std::wstring& serviceName = kDefaultServiceName,
        uint32_t timeoutMs = 5000);

    // Stop service
    static bool stopService(
        const std::wstring& serviceName = kDefaultServiceName,
        uint32_t timeoutMs = 5000);

    // Uninstall service
    static bool uninstallService(const std::wstring& serviceName = kDefaultServiceName);
};

} // namespace gx::distribution

