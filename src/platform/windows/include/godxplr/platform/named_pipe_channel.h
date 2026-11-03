#pragma once

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include "godxplr/platform/win32_common.h"
#include <cstdint>
#include <string>
#include <vector>
#include <functional>
#include <atomic>
#include <thread>
#include <mutex>

namespace gx::platform::win32 {

constexpr wchar_t kDefaultPipeName[] = L"\\\\.\\pipe\\godxplr-index-v1";
constexpr uint32_t kMaxPipeFrameSize = 4 * 1024 * 1024; // 4 MB safety limit (reject oversized frames)
constexpr uint16_t kProtocolVersion = 1;

enum class PipeMessageType : uint16_t {
    Hello = 1,
    HelloResponse = 2,
    Query = 3,
    Page = 4,
    Cancel = 5,
    Status = 6,
    RecordOpen = 7
};

#pragma pack(push, 1)
struct PipeFrameHeader {
    uint32_t length{0};         // Total payload length following this header
    uint16_t type{0};           // PipeMessageType
    uint16_t flags{0};
    uint64_t reqId{0};
};
#pragma pack(pop)

// Client for querying godxplr-indexd over named pipe
class NamedPipeClient {
public:
    NamedPipeClient() = default;
    ~NamedPipeClient() { disconnect(); }

    NamedPipeClient(const NamedPipeClient&) = delete;
    NamedPipeClient& operator=(const NamedPipeClient&) = delete;

    bool connect(const std::wstring& pipeName = kDefaultPipeName, uint32_t timeoutMs = 2000);
    void disconnect();
    [[nodiscard]] bool isConnected() const noexcept { return pipeHandle_.isValid(); }

    bool sendFrame(PipeMessageType type, uint64_t reqId, const void* payload, uint32_t payloadLen);
    bool readFrame(PipeFrameHeader& outHeader, std::vector<uint8_t>& outPayload);

private:
    ScopedHandle pipeHandle_;
};

// Server for hosting the search engine over named pipe with security enforcement
class NamedPipeServer {
public:
    NamedPipeServer() = default;
    ~NamedPipeServer() { stop(); }

    NamedPipeServer(const NamedPipeServer&) = delete;
    NamedPipeServer& operator=(const NamedPipeServer&) = delete;

    using FrameHandler = std::function<void(
        HANDLE pipeHandle,
        const PipeFrameHeader& header,
        const uint8_t* payload,
        uint32_t payloadLen
    )>;

    bool start(
        const std::wstring& pipeName = kDefaultPipeName,
        FrameHandler handler = nullptr);
    void stop();

    [[nodiscard]] bool isRunning() const noexcept { return isRunning_.load(std::memory_order_relaxed); }

    // Performs in-memory AccessCheck under caller token against a Security Descriptor (ADR-0019)
    static bool checkClientAccess(
        HANDLE clientToken,
        PSECURITY_DESCRIPTOR pSD,
        DWORD desiredAccess = FILE_LIST_DIRECTORY);

private:
    void serverLoop(std::wstring pipeName, HANDLE hStarted);

    std::atomic<bool> isRunning_{false};
    std::atomic<bool> stopRequested_{false};
    std::thread workerThread_;
    FrameHandler handler_;
    ScopedHandle stopEvent_;
};

} // namespace gx::platform::win32

#endif // _WIN32
