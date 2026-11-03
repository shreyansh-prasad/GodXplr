#pragma once

#ifdef _WIN32

#include "godxplr/platform/win32_common.h"
#include <cstdint>
#include <string>
#include <string_view>
#include <atomic>
#include <memory>

#pragma pack(push, 8)
namespace gx::ipc {

constexpr size_t kMaxQueryLen = 512;
constexpr size_t kMaxResultRows = 100;

struct ShmResultRow {
    uint64_t objectId{0};       // FRN
    uint64_t size{0};
    uint64_t mtime{0};          // FILETIME
    uint32_t attributes{0};
    uint16_t nameLen{0};        // UTF-8 bytes
    uint16_t dirLen{0};         // UTF-8 bytes
    char name[260]{0};          // Inline fixed buffer
    char dir[512]{0};           // Inline parent directory path
    float score{0.0f};
    uint8_t tier{0};            // 0=exact, 1=prefix, 2=boundary, 3=substring, 4=fuzzy
    uint8_t pad[3]{0};          // Explicit alignment
};

struct ShmSearchBuffer {
    // Header (UI -> Service)
    std::atomic<uint64_t> queryGeneration{0};
    uint32_t queryLen{0};
    char queryText[kMaxQueryLen]{0};
    uint32_t optionsFlags{0};   // caseSensitive, matchPath, fuzzy

    // Response (Service -> UI)
    std::atomic<uint64_t> resultGeneration{0};
    uint32_t resultCount{0};
    uint32_t totalMatches{0};
    uint32_t statusFlags{0};   // 1 = final, 2 = fuzzy pending
    ShmResultRow rows[kMaxResultRows];
};

} // namespace gx::ipc
#pragma pack(pop)

namespace gx::platform::win32 {

class ShmServer {
public:
    ShmServer() = default;
    ~ShmServer();

    // Creates shared memory section and synchronization events
    bool create(uint32_t sessionId = 0, bool global = false);
    void close();

    [[nodiscard]] bool isCreated() const noexcept { return pBuffer_ != nullptr; }
    [[nodiscard]] ipc::ShmSearchBuffer* getBuffer() noexcept { return pBuffer_; }

    // Blocks until a new query is submitted or stop is requested
    bool waitForQuery(uint32_t timeoutMs = INFINITE);

    // Signals UI that results are available
    void notifyResultsReady();

    // Signals stop event to unblock server loop
    void requestStop();

private:
    uint32_t sessionId_{0};
    ScopedHandle hMap_{};
    ScopedHandle hQueryEvent_{};
    ScopedHandle hResultEvent_{};
    ScopedHandle hStopEvent_{};
    ipc::ShmSearchBuffer* pBuffer_{nullptr};
};

class ShmClient {
public:
    ShmClient() = default;
    ~ShmClient();

    // Connects to existing shared memory section and synchronization events
    bool open(uint32_t sessionId = 0, bool global = false);
    void close();

    [[nodiscard]] bool isConnected() const noexcept { return pBuffer_ != nullptr; }
    [[nodiscard]] const ipc::ShmSearchBuffer* getBuffer() const noexcept { return pBuffer_; }

    // Submits search query and signals service; returns submitted generation
    uint64_t submitQuery(std::string_view query, uint32_t options = 0);

    // Waits until results for expectedGen are produced
    bool waitForResults(uint64_t expectedGen, uint32_t timeoutMs = 100);

private:
    uint32_t sessionId_{0};
    ScopedHandle hMap_{};
    ScopedHandle hQueryEvent_{};
    ScopedHandle hResultEvent_{};
    ipc::ShmSearchBuffer* pBuffer_{nullptr};
};

} // namespace gx::platform::win32

#endif // _WIN32

