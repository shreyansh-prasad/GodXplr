#include "godxplr/distribution/service_controller.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winsvc.h>

namespace gx::distribution {

ServiceStatus ServiceController::queryStatus(const std::wstring& serviceName) {
    SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!scm) {
        return ServiceStatus::Unknown;
    }

    SC_HANDLE service = OpenServiceW(scm, serviceName.c_str(), SERVICE_QUERY_STATUS);
    if (!service) {
        DWORD err = GetLastError();
        CloseServiceHandle(scm);
        if (err == ERROR_SERVICE_DOES_NOT_EXIST) {
            return ServiceStatus::NotFound;
        }
        return ServiceStatus::Unknown;
    }

    SERVICE_STATUS_PROCESS ssp{};
    DWORD bytesNeeded = 0;
    BOOL ok = QueryServiceStatusEx(
        service,
        SC_STATUS_PROCESS_INFO,
        reinterpret_cast<LPBYTE>(&ssp),
        sizeof(ssp),
        &bytesNeeded);

    CloseServiceHandle(service);
    CloseServiceHandle(scm);

    if (!ok) {
        return ServiceStatus::Unknown;
    }

    switch (ssp.dwCurrentState) {
    case SERVICE_STOPPED: return ServiceStatus::Stopped;
    case SERVICE_START_PENDING: return ServiceStatus::StartPending;
    case SERVICE_STOP_PENDING: return ServiceStatus::StopPending;
    case SERVICE_RUNNING: return ServiceStatus::Running;
    case SERVICE_CONTINUE_PENDING: return ServiceStatus::ContinuePending;
    case SERVICE_PAUSE_PENDING: return ServiceStatus::PausePending;
    case SERVICE_PAUSED: return ServiceStatus::Paused;
    default: return ServiceStatus::Unknown;
    }
}

bool ServiceController::installService(
    const std::wstring& binaryPath,
    const std::wstring& serviceName,
    const std::wstring& displayName)
{
    SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CREATE_SERVICE);
    if (!scm) {
        return false;
    }

    std::wstring quotedBin = L"\"" + binaryPath + L"\"";

    SC_HANDLE service = CreateServiceW(
        scm,
        serviceName.c_str(),
        displayName.c_str(),
        SERVICE_ALL_ACCESS,
        SERVICE_WIN32_OWN_PROCESS,
        SERVICE_DEMAND_START,
        SERVICE_ERROR_NORMAL,
        quotedBin.c_str(),
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr);

    if (!service) {
        CloseServiceHandle(scm);
        return false;
    }

    CloseServiceHandle(service);
    CloseServiceHandle(scm);
    return true;
}

bool ServiceController::startService(
    const std::wstring& serviceName,
    uint32_t timeoutMs)
{
    SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!scm) return false;

    SC_HANDLE service = OpenServiceW(scm, serviceName.c_str(), SERVICE_START | SERVICE_QUERY_STATUS);
    if (!service) {
        CloseServiceHandle(scm);
        return false;
    }

    if (!StartServiceW(service, 0, nullptr)) {
        DWORD err = GetLastError();
        if (err != ERROR_SERVICE_ALREADY_RUNNING) {
            CloseServiceHandle(service);
            CloseServiceHandle(scm);
            return false;
        }
    }

    DWORD startTime = GetTickCount();
    while (GetTickCount() - startTime < timeoutMs) {
        SERVICE_STATUS_PROCESS ssp{};
        DWORD bytesNeeded = 0;
        if (QueryServiceStatusEx(service, SC_STATUS_PROCESS_INFO, reinterpret_cast<LPBYTE>(&ssp), sizeof(ssp), &bytesNeeded)) {
            if (ssp.dwCurrentState == SERVICE_RUNNING) {
                CloseServiceHandle(service);
                CloseServiceHandle(scm);
                return true;
            }
        }
        Sleep(100);
    }

    CloseServiceHandle(service);
    CloseServiceHandle(scm);
    return false;
}

bool ServiceController::stopService(
    const std::wstring& serviceName,
    uint32_t timeoutMs)
{
    SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!scm) return false;

    SC_HANDLE service = OpenServiceW(scm, serviceName.c_str(), SERVICE_STOP | SERVICE_QUERY_STATUS);
    if (!service) {
        CloseServiceHandle(scm);
        return false;
    }

    SERVICE_STATUS status{};
    if (!ControlService(service, SERVICE_CONTROL_STOP, &status)) {
        DWORD err = GetLastError();
        if (err != ERROR_SERVICE_NOT_ACTIVE) {
            CloseServiceHandle(service);
            CloseServiceHandle(scm);
            return false;
        }
    }

    DWORD startTime = GetTickCount();
    while (GetTickCount() - startTime < timeoutMs) {
        SERVICE_STATUS_PROCESS ssp{};
        DWORD bytesNeeded = 0;
        if (QueryServiceStatusEx(service, SC_STATUS_PROCESS_INFO, reinterpret_cast<LPBYTE>(&ssp), sizeof(ssp), &bytesNeeded)) {
            if (ssp.dwCurrentState == SERVICE_STOPPED) {
                CloseServiceHandle(service);
                CloseServiceHandle(scm);
                return true;
            }
        }
        Sleep(100);
    }

    CloseServiceHandle(service);
    CloseServiceHandle(scm);
    return false;
}

bool ServiceController::uninstallService(const std::wstring& serviceName) {
    SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!scm) return false;

    SC_HANDLE service = OpenServiceW(scm, serviceName.c_str(), DELETE);
    if (!service) {
        CloseServiceHandle(scm);
        return false;
    }

    BOOL ok = DeleteService(service);
    CloseServiceHandle(service);
    CloseServiceHandle(scm);
    return ok == TRUE;
}

} // namespace gx::distribution

