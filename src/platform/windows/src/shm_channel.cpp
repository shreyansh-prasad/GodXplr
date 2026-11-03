#include "godxplr/platform/shm_channel.h"

#ifdef _WIN32

#include <sddl.h>
#include <algorithm>
#include <chrono>

namespace gx::platform::win32 {

namespace {

std::wstring buildName(std::wstring_view prefix, uint32_t sessionId, bool global) {
    std::wstring s = global ? L"Global\\" : L"Local\\";
    s.append(prefix);
    s.push_back(L'_');
    s.append(std::to_wstring(sessionId));
    return s;
}

struct SecurityHelper {
    PSECURITY_DESCRIPTOR pSd{nullptr};
    SECURITY_ATTRIBUTES sa{};

    SecurityHelper() {
        sa.nLength = sizeof(sa);
        sa.bInheritHandle = FALSE;
        // Permissive DACL allowing interactive and service access
        if (::ConvertStringSecurityDescriptorToSecurityDescriptorW(
                L"D:(A;;GA;;;WD)", SDDL_REVISION_1, &pSd, nullptr)) {
            sa.lpSecurityDescriptor = pSd;
        }
    }

    ~SecurityHelper() {
        if (pSd) {
            ::LocalFree(pSd);
        }
    }

    SECURITY_ATTRIBUTES* get() noexcept {
        return pSd ? &sa : nullptr;
    }
};

} // namespace

// --- ShmServer ---

ShmServer::~ShmServer() {
    close();
}

bool ShmServer::create(uint32_t sessionId, bool global) {
    close();
    sessionId_ = sessionId;

    SecurityHelper sec;
    std::wstring shmName = buildName(L"GodXplr_Search_Shm", sessionId, global);
    std::wstring queryEvName = buildName(L"GodXplr_Search_QueryEv", sessionId, global);
    std::wstring resultEvName = buildName(L"GodXplr_Search_ResultEv", sessionId, global);

    // Create file mapping
    HANDLE hMap = ::CreateFileMappingW(
        INVALID_HANDLE_VALUE,
        sec.get(),
        PAGE_READWRITE,
        0,
        static_cast<DWORD>(sizeof(ipc::ShmSearchBuffer)),
        shmName.c_str()
    );

    if (!hMap) {
        return false;
    }
    hMap_.reset(hMap);

    void* pView = ::MapViewOfFile(hMap_.get(), FILE_MAP_ALL_ACCESS, 0, 0, sizeof(ipc::ShmSearchBuffer));
    if (!pView) {
        close();
        return false;
    }

    pBuffer_ = static_cast<ipc::ShmSearchBuffer*>(pView);
    std::memset(pBuffer_, 0, sizeof(ipc::ShmSearchBuffer));

    // Create synchronization events (Auto-reset for queries/results)
    HANDLE hQuery = ::CreateEventW(sec.get(), FALSE, FALSE, queryEvName.c_str());
    if (!hQuery) {
        close();
        return false;
    }
    hQueryEvent_.reset(hQuery);

    HANDLE hResult = ::CreateEventW(sec.get(), FALSE, FALSE, resultEvName.c_str());
    if (!hResult) {
        close();
        return false;
    }
    hResultEvent_.reset(hResult);

    // Stop event is internal to the server process (Manual-reset)
    HANDLE hStop = ::CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!hStop) {
        close();
        return false;
    }
    hStopEvent_.reset(hStop);

    return true;
}

void ShmServer::close() {
    requestStop();

    if (pBuffer_) {
        ::UnmapViewOfFile(pBuffer_);
        pBuffer_ = nullptr;
    }

    hMap_.reset();
    hQueryEvent_.reset();
    hResultEvent_.reset();
    hStopEvent_.reset();
}

bool ShmServer::waitForQuery(uint32_t timeoutMs) {
    if (!hQueryEvent_.isValid() || !hStopEvent_.isValid()) {
        return false;
    }

    HANDLE handles[2] = { hStopEvent_.get(), hQueryEvent_.get() };
    DWORD res = ::WaitForMultipleObjects(2, handles, FALSE, timeoutMs);

    if (res == WAIT_OBJECT_0) {
        // Stop requested
        return false;
    }
    if (res == WAIT_OBJECT_0 + 1) {
        // Query pending
        return true;
    }

    return false;
}

void ShmServer::notifyResultsReady() {
    if (hResultEvent_.isValid()) {
        ::SetEvent(hResultEvent_.get());
    }
}

void ShmServer::requestStop() {
    if (hStopEvent_.isValid()) {
        ::SetEvent(hStopEvent_.get());
    }
}

// --- ShmClient ---

ShmClient::~ShmClient() {
    close();
}

bool ShmClient::open(uint32_t sessionId, bool global) {
    close();
    sessionId_ = sessionId;

    std::wstring shmName = buildName(L"GodXplr_Search_Shm", sessionId, global);
    std::wstring queryEvName = buildName(L"GodXplr_Search_QueryEv", sessionId, global);
    std::wstring resultEvName = buildName(L"GodXplr_Search_ResultEv", sessionId, global);

    HANDLE hMap = ::OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, shmName.c_str());
    if (!hMap) {
        return false;
    }
    hMap_.reset(hMap);

    void* pView = ::MapViewOfFile(hMap_.get(), FILE_MAP_ALL_ACCESS, 0, 0, sizeof(ipc::ShmSearchBuffer));
    if (!pView) {
        close();
        return false;
    }
    pBuffer_ = static_cast<ipc::ShmSearchBuffer*>(pView);

    HANDLE hQuery = ::OpenEventW(EVENT_MODIFY_STATE | SYNCHRONIZE, FALSE, queryEvName.c_str());
    if (!hQuery) {
        close();
        return false;
    }
    hQueryEvent_.reset(hQuery);

    HANDLE hResult = ::OpenEventW(EVENT_MODIFY_STATE | SYNCHRONIZE, FALSE, resultEvName.c_str());
    if (!hResult) {
        close();
        return false;
    }
    hResultEvent_.reset(hResult);

    return true;
}

void ShmClient::close() {
    if (pBuffer_) {
        ::UnmapViewOfFile(pBuffer_);
        pBuffer_ = nullptr;
    }
    hMap_.reset();
    hQueryEvent_.reset();
    hResultEvent_.reset();
}

uint64_t ShmClient::submitQuery(std::string_view query, uint32_t options) {
    if (!pBuffer_ || !hQueryEvent_.isValid()) {
        return 0;
    }

    uint64_t gen = pBuffer_->queryGeneration.fetch_add(1, std::memory_order_relaxed) + 1;

    size_t copyLen = std::min(query.size(), ipc::kMaxQueryLen - 1);
    if (copyLen > 0) {
        std::memcpy(pBuffer_->queryText, query.data(), copyLen);
    }
    pBuffer_->queryText[copyLen] = '\0';
    pBuffer_->queryLen = static_cast<uint32_t>(copyLen);
    pBuffer_->optionsFlags = options;

    ::SetEvent(hQueryEvent_.get());
    return gen;
}

bool ShmClient::waitForResults(uint64_t expectedGen, uint32_t timeoutMs) {
    if (!pBuffer_ || !hResultEvent_.isValid()) {
        return false;
    }

    auto start = std::chrono::steady_clock::now();
    while (true) {
        if (pBuffer_->resultGeneration.load(std::memory_order_acquire) >= expectedGen) {
            return true;
        }

        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start
        ).count();

        if (elapsed >= timeoutMs) {
            break;
        }

        uint32_t remaining = static_cast<uint32_t>(timeoutMs - elapsed);
        DWORD res = ::WaitForSingleObject(hResultEvent_.get(), remaining);
        if (res == WAIT_OBJECT_0) {
            if (pBuffer_->resultGeneration.load(std::memory_order_acquire) >= expectedGen) {
                return true;
            }
        } else {
            break;
        }
    }

    return pBuffer_->resultGeneration.load(std::memory_order_acquire) >= expectedGen;
}

} // namespace gx::platform::win32

#endif // _WIN32
