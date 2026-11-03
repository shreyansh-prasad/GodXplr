#ifdef _WIN32

#include "godxplr/platform/named_pipe_channel.h"
#include <sddl.h>

namespace gx::platform::win32 {

bool NamedPipeClient::connect(const std::wstring& pipeName, uint32_t timeoutMs) {
    disconnect();

    const auto start = std::chrono::steady_clock::now();
    while (true) {
        HANDLE hPipe = ::CreateFileW(
            pipeName.c_str(),
            GENERIC_READ | GENERIC_WRITE,
            0,
            nullptr,
            OPEN_EXISTING,
            0,
            nullptr
        );

        if (hPipe != INVALID_HANDLE_VALUE) {
            pipeHandle_.reset(hPipe);
            DWORD mode = PIPE_READMODE_MESSAGE;
            ::SetNamedPipeHandleState(hPipe, &mode, nullptr, nullptr);
            return true;
        }

        DWORD err = ::GetLastError();
        if (err != ERROR_PIPE_BUSY) {
            return false;
        }

        if (!::WaitNamedPipeW(pipeName.c_str(), 100)) {
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start).count();
            if (elapsed >= timeoutMs) {
                return false;
            }
        }
    }
}

void NamedPipeClient::disconnect() {
    pipeHandle_.reset();
}

bool NamedPipeClient::sendFrame(PipeMessageType type, uint64_t reqId, const void* payload, uint32_t payloadLen) {
    if (!pipeHandle_.isValid() || payloadLen > kMaxPipeFrameSize) {
        return false;
    }

    PipeFrameHeader hdr{};
    hdr.length = payloadLen;
    hdr.type = static_cast<uint16_t>(type);
    hdr.flags = 0;
    hdr.reqId = reqId;

    DWORD bytesWritten = 0;
    if (!::WriteFile(pipeHandle_.get(), &hdr, sizeof(hdr), &bytesWritten, nullptr) || bytesWritten != sizeof(hdr)) {
        return false;
    }

    if (payloadLen > 0 && payload != nullptr) {
        if (!::WriteFile(pipeHandle_.get(), payload, payloadLen, &bytesWritten, nullptr) || bytesWritten != payloadLen) {
            return false;
        }
    }

    return true;
}

bool NamedPipeClient::readFrame(PipeFrameHeader& outHeader, std::vector<uint8_t>& outPayload) {
    if (!pipeHandle_.isValid()) {
        return false;
    }

    DWORD bytesRead = 0;
    if (!::ReadFile(pipeHandle_.get(), &outHeader, sizeof(outHeader), &bytesRead, nullptr) || bytesRead != sizeof(outHeader)) {
        return false;
    }

    if (outHeader.length > kMaxPipeFrameSize) {
        return false; // Oversized frame protection
    }

    outPayload.resize(outHeader.length);
    if (outHeader.length > 0) {
        DWORD payloadRead = 0;
        if (!::ReadFile(pipeHandle_.get(), outPayload.data(), outHeader.length, &payloadRead, nullptr) || payloadRead != outHeader.length) {
            return false;
        }
    }

    return true;
}

bool NamedPipeServer::start(const std::wstring& pipeName, FrameHandler handler) {
    stop();

    stopRequested_.store(false, std::memory_order_relaxed);
    isRunning_.store(false, std::memory_order_relaxed);
    handler_ = std::move(handler);

    stopEvent_.reset(::CreateEventW(nullptr, TRUE, FALSE, nullptr));
    if (!stopEvent_.isValid()) {
        return false;
    }

    HANDLE hStarted = ::CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!hStarted) {
        return false;
    }
    ScopedHandle startedEvent(hStarted);

    workerThread_ = std::thread([this, pipeName, hStarted]() {
        serverLoop(pipeName, hStarted);
    });

    DWORD waitRes = ::WaitForSingleObject(startedEvent.get(), 2000);
    return (waitRes == WAIT_OBJECT_0) && isRunning_.load(std::memory_order_acquire);
}

void NamedPipeServer::stop() {
    bool expected = false;
    if (stopRequested_.compare_exchange_strong(expected, true)) {
        if (stopEvent_.isValid()) {
            ::SetEvent(stopEvent_.get());
        }
        if (workerThread_.joinable()) {
            workerThread_.join();
        }
        stopEvent_.reset();
        isRunning_.store(false, std::memory_order_release);
    }
}

void NamedPipeServer::serverLoop(std::wstring pipeName, HANDLE hStarted) {
    isRunning_.store(true, std::memory_order_release);
    if (hStarted) {
        ::SetEvent(hStarted);
    }

    ScopedHandle connectEvent(::CreateEventW(nullptr, TRUE, FALSE, nullptr));
    OVERLAPPED ol{};
    ol.hEvent = connectEvent.get();

    while (!stopRequested_.load(std::memory_order_acquire)) {
        // PIPE_REJECT_REMOTE_CLIENTS prevents remote network clients from connecting to search daemon
        HANDLE hPipe = ::CreateNamedPipeW(
            pipeName.c_str(),
            PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED | FILE_FLAG_FIRST_PIPE_INSTANCE,
            PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS,
            1,
            65536,
            65536,
            0,
            nullptr
        );

        if (hPipe == INVALID_HANDLE_VALUE) {
            // First pipe instance or permission issue; wait briefly before retrying
            ::Sleep(50);
            continue;
        }

        ScopedHandle pipe(hPipe);
        ::ResetEvent(connectEvent.get());

        BOOL connected = ::ConnectNamedPipe(hPipe, &ol);
        if (!connected) {
            DWORD err = ::GetLastError();
            if (err == ERROR_IO_PENDING) {
                HANDLE waitHandles[2] = { connectEvent.get(), stopEvent_.get() };
                DWORD waitRes = ::WaitForMultipleObjects(2, waitHandles, FALSE, INFINITE);
                if (waitRes != WAIT_OBJECT_0) {
                    ::CancelIo(hPipe);
                    break; // Stop requested
                }
            } else if (err != ERROR_PIPE_CONNECTED) {
                continue;
            }
        }

        // Process incoming client frames
        while (!stopRequested_.load(std::memory_order_relaxed)) {
            PipeFrameHeader hdr{};
            DWORD bytesRead = 0;
            BOOL readHdrOk = ::ReadFile(hPipe, &hdr, sizeof(hdr), &bytesRead, nullptr);
            if (!readHdrOk || bytesRead != sizeof(hdr)) {
                break; // Client disconnected or closed pipe
            }

            if (hdr.length > kMaxPipeFrameSize) {
                break; // Reject oversized frame
            }

            std::vector<uint8_t> payload(hdr.length);
            if (hdr.length > 0) {
                DWORD payloadRead = 0;
                BOOL readPayloadOk = ::ReadFile(hPipe, payload.data(), hdr.length, &payloadRead, nullptr);
                if (!readPayloadOk || payloadRead != hdr.length) {
                    break;
                }
            }

            if (handler_) {
                handler_(hPipe, hdr, payload.data(), hdr.length);
            }
        }

        ::FlushFileBuffers(hPipe);
        ::DisconnectNamedPipe(hPipe);
    }

    isRunning_.store(false, std::memory_order_release);
}

bool NamedPipeServer::checkClientAccess(
    HANDLE clientToken,
    PSECURITY_DESCRIPTOR pSD,
    DWORD desiredAccess)
{
    if (!clientToken || !pSD) {
        return true;
    }

    GENERIC_MAPPING genericMapping{};
    genericMapping.GenericRead = FILE_GENERIC_READ;
    genericMapping.GenericWrite = FILE_GENERIC_WRITE;
    genericMapping.GenericExecute = FILE_GENERIC_EXECUTE;
    genericMapping.GenericAll = FILE_ALL_ACCESS;

    PRIVILEGE_SET privilegeSet{};
    DWORD privSetLen = sizeof(privilegeSet);
    DWORD grantedAccess = 0;
    BOOL accessStatus = FALSE;

    BOOL ok = ::AccessCheck(
        pSD,
        clientToken,
        desiredAccess,
        &genericMapping,
        &privilegeSet,
        &privSetLen,
        &grantedAccess,
        &accessStatus
    );

    return ok && accessStatus;
}

} // namespace gx::platform::win32

#endif // _WIN32
