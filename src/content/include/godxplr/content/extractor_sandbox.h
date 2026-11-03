#pragma once

#include <string>
#include <cstdint>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace gx::content {

enum class SandboxResult : uint8_t {
    Success,
    Timeout,
    MemoryExceeded,
    Crashed,
    FailedToLaunch
};

struct SandboxLimits {
    size_t maxMemoryBytes{128 * 1024 * 1024}; // 128 MB RAM limit
    uint32_t timeoutSeconds{10};              // 10s execution cap
    bool restrictUi{true};
    bool killOnClose{true};
};

class ExtractorSandbox {
public:
    explicit ExtractorSandbox(SandboxLimits limits = {});
    ~ExtractorSandbox();

    ExtractorSandbox(const ExtractorSandbox&) = delete;
    ExtractorSandbox& operator=(const ExtractorSandbox&) = delete;

    // Initializes the Win32 Job Object with memory, UI, and lifecycle limits
    bool initializeJob();

    // Assigns a process to the sandbox Job Object
    bool assignProcess(HANDLE processHandle);

    // Runs a process within the sandbox and enforces timeout & memory limits
    SandboxResult runProcess(
        const std::wstring& commandLine,
        uint32_t* outExitCode = nullptr);

    [[nodiscard]] HANDLE jobHandle() const noexcept { return jobHandle_; }
    [[nodiscard]] const SandboxLimits& limits() const noexcept { return limits_; }

private:
    SandboxLimits limits_;
    HANDLE jobHandle_{nullptr};
};

} // namespace gx::content

