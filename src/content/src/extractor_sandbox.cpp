#include "godxplr/content/extractor_sandbox.h"
#include <vector>

namespace gx::content {

ExtractorSandbox::ExtractorSandbox(SandboxLimits limits)
    : limits_(limits)
{
    initializeJob();
}

ExtractorSandbox::~ExtractorSandbox() {
    if (jobHandle_) {
        CloseHandle(jobHandle_);
        jobHandle_ = nullptr;
    }
}

bool ExtractorSandbox::initializeJob() {
    if (jobHandle_) {
        return true;
    }

    jobHandle_ = CreateJobObjectW(nullptr, nullptr);
    if (!jobHandle_) {
        return false;
    }

    // Configure memory and lifecycle limits
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli{};
    jeli.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_PROCESS_MEMORY |
                                           JOB_OBJECT_LIMIT_DIE_ON_UNHANDLED_EXCEPTION;
    if (limits_.killOnClose) {
        jeli.BasicLimitInformation.LimitFlags |= JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    }
    jeli.ProcessMemoryLimit = limits_.maxMemoryBytes;

    if (!SetInformationJobObject(
            jobHandle_,
            JobObjectExtendedLimitInformation,
            &jeli,
            sizeof(jeli)))
    {
        CloseHandle(jobHandle_);
        jobHandle_ = nullptr;
        return false;
    }

    // Configure UI restrictions (no clipboard, desktop, or window handles)
    if (limits_.restrictUi) {
        JOBOBJECT_BASIC_UI_RESTRICTIONS uiLimits{};
        uiLimits.UIRestrictionsClass = JOB_OBJECT_UILIMIT_HANDLES |
                                       JOB_OBJECT_UILIMIT_READCLIPBOARD |
                                       JOB_OBJECT_UILIMIT_WRITECLIPBOARD |
                                       JOB_OBJECT_UILIMIT_SYSTEMPARAMETERS |
                                       JOB_OBJECT_UILIMIT_DISPLAYSETTINGS;

        SetInformationJobObject(
            jobHandle_,
            JobObjectBasicUIRestrictions,
            &uiLimits,
            sizeof(uiLimits));
    }

    return true;
}

bool ExtractorSandbox::assignProcess(HANDLE processHandle) {
    if (!jobHandle_ || !processHandle) {
        return false;
    }
    return AssignProcessToJobObject(jobHandle_, processHandle) != FALSE;
}

SandboxResult ExtractorSandbox::runProcess(
    const std::wstring& commandLine,
    uint32_t* outExitCode)
{
    if (!jobHandle_ && !initializeJob()) {
        return SandboxResult::FailedToLaunch;
    }

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};

    // Create process in suspended state to assign to job before any code executes
    std::vector<wchar_t> cmdBuf(commandLine.begin(), commandLine.end());
    cmdBuf.push_back(L'\0');

    BOOL created = CreateProcessW(
        nullptr,
        cmdBuf.data(),
        nullptr,
        nullptr,
        FALSE,
        CREATE_SUSPENDED | BELOW_NORMAL_PRIORITY_CLASS,
        nullptr,
        nullptr,
        &si,
        &pi);

    if (!created) {
        return SandboxResult::FailedToLaunch;
    }

    if (!assignProcess(pi.hProcess)) {
        TerminateProcess(pi.hProcess, 1);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        return SandboxResult::FailedToLaunch;
    }

    ResumeThread(pi.hThread);

    DWORD waitResult = WaitForSingleObject(pi.hProcess, limits_.timeoutSeconds * 1000);

    if (waitResult == WAIT_TIMEOUT) {
        TerminateJobObject(jobHandle_, 1);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        return SandboxResult::Timeout;
    }

    DWORD exitCode = 0;
    if (GetExitCodeProcess(pi.hProcess, &exitCode)) {
        if (outExitCode) {
            *outExitCode = exitCode;
        }
    }

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    if (exitCode != 0 && (exitCode >= 0xC0000000 || exitCode == 1)) {
        // Abnormal termination or crash
        return SandboxResult::Crashed;
    }

    return SandboxResult::Success;
}

} // namespace gx::content

