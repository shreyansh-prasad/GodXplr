#include "godxplr/distribution/registry_manager.h"

#include <fstream>
#include <sstream>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace gx::distribution {

namespace {

const wchar_t* kDirCmdKey = L"Software\\Classes\\Directory\\shell\\open\\command";
const wchar_t* kDriveCmdKey = L"Software\\Classes\\Drive\\shell\\open\\command";
const wchar_t* kFolderCmdKey = L"Software\\Classes\\Folder\\shell\\open\\command";

std::string toUtf8(const std::wstring& w) {
    if (w.empty()) return "";
    int size = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), static_cast<int>(w.size()), nullptr, 0, nullptr, nullptr);
    if (size <= 0) return "";
    std::string s(static_cast<size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), static_cast<int>(w.size()), s.data(), size, nullptr, nullptr);
    return s;
}

std::wstring toWide(const std::string& s) {
    if (s.empty()) return L"";
    int size = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), nullptr, 0);
    if (size <= 0) return L"";
    std::wstring w(static_cast<size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), w.data(), size);
    return w;
}

} // namespace

std::optional<std::wstring> RegistryManager::readHkcuString(
    const std::wstring& subKey,
    const std::wstring& valueName)
{
    HKEY hKey = nullptr;
    LSTATUS status = RegOpenKeyExW(HKEY_CURRENT_USER, subKey.c_str(), 0, KEY_READ, &hKey);
    if (status != ERROR_SUCCESS) {
        return std::nullopt;
    }

    DWORD type = 0;
    DWORD bytes = 0;
    const wchar_t* vName = valueName.empty() ? nullptr : valueName.c_str();

    status = RegQueryValueExW(hKey, vName, nullptr, &type, nullptr, &bytes);
    if (status != ERROR_SUCCESS || (type != REG_SZ && type != REG_EXPAND_SZ)) {
        RegCloseKey(hKey);
        return std::nullopt;
    }

    std::wstring result(bytes / sizeof(wchar_t), L'\0');
    status = RegQueryValueExW(hKey, vName, nullptr, &type, reinterpret_cast<LPBYTE>(result.data()), &bytes);
    RegCloseKey(hKey);

    if (status == ERROR_SUCCESS) {
        while (!result.empty() && result.back() == L'\0') {
            result.pop_back();
        }
        return result;
    }

    return std::nullopt;
}

bool RegistryManager::writeHkcuString(
    const std::wstring& subKey,
    const std::wstring& valueName,
    const std::wstring& data)
{
    HKEY hKey = nullptr;
    DWORD disp = 0;
    LSTATUS status = RegCreateKeyExW(
        HKEY_CURRENT_USER,
        subKey.c_str(),
        0,
        nullptr,
        REG_OPTION_NON_VOLATILE,
        KEY_WRITE,
        nullptr,
        &hKey,
        &disp);

    if (status != ERROR_SUCCESS) {
        return false;
    }

    const wchar_t* vName = valueName.empty() ? nullptr : valueName.c_str();
    DWORD bytes = static_cast<DWORD>((data.size() + 1) * sizeof(wchar_t));

    status = RegSetValueExW(
        hKey,
        vName,
        0,
        REG_SZ,
        reinterpret_cast<const BYTE*>(data.c_str()),
        bytes);

    RegCloseKey(hKey);
    return status == ERROR_SUCCESS;
}

bool RegistryManager::deleteHkcuKey(const std::wstring& subKey) {
    return RegDeleteKeyW(HKEY_CURRENT_USER, subKey.c_str()) == ERROR_SUCCESS;
}

bool RegistryManager::isDefaultOpener(const std::wstring& godxplrExePath) {
    auto val = readHkcuString(kDirCmdKey, L"");
    if (!val.has_value()) {
        return false;
    }
    return val->find(godxplrExePath) != std::wstring::npos;
}

bool RegistryManager::setDefaultOpener(
    const std::wstring& godxplrExePath,
    const std::filesystem::path& backupFilePath)
{
    // 1. Read existing values to backup
    auto oldDir = readHkcuString(kDirCmdKey, L"").value_or(L"");
    auto oldDrive = readHkcuString(kDriveCmdKey, L"").value_or(L"");
    auto oldFolder = readHkcuString(kFolderCmdKey, L"").value_or(L"");

    // Write backup file if it doesn't already exist
    std::error_code ec;
    if (!std::filesystem::exists(backupFilePath, ec)) {
        std::ofstream out(backupFilePath);
        if (out.is_open()) {
            std::string dirUtf8 = toUtf8(oldDir);
            std::string driveUtf8 = toUtf8(oldDrive);
            std::string folderUtf8 = toUtf8(oldFolder);

            out << "DIR=" << dirUtf8 << "\n";
            out << "DRIVE=" << driveUtf8 << "\n";
            out << "FOLDER=" << folderUtf8 << "\n";
        }
    }

    std::wstring commandLine = L"\"" + godxplrExePath + L"\" \"%1\"";

    bool ok1 = writeHkcuString(kDirCmdKey, L"", commandLine);
    bool ok2 = writeHkcuString(kDriveCmdKey, L"", commandLine);
    bool ok3 = writeHkcuString(kFolderCmdKey, L"", commandLine);

    return ok1 && ok2 && ok3;
}

bool RegistryManager::restoreDefaultExplorer(const std::filesystem::path& backupFilePath) {
    std::error_code ec;
    std::wstring dirRestore = L"";
    std::wstring driveRestore = L"";
    std::wstring folderRestore = L"";
    bool parsedBackup = false;

    if (std::filesystem::exists(backupFilePath, ec)) {
        std::ifstream in(backupFilePath);
        std::string line;
        while (std::getline(in, line)) {
            if (line.rfind("DIR=", 0) == 0) {
                dirRestore = toWide(line.substr(4));
                parsedBackup = true;
            } else if (line.rfind("DRIVE=", 0) == 0) {
                driveRestore = toWide(line.substr(6));
            } else if (line.rfind("FOLDER=", 0) == 0) {
                folderRestore = toWide(line.substr(7));
            }
        }
    }

    // If backup had non-empty values, restore them; otherwise clean HKCU override keys so Windows defaults take over
    if (parsedBackup && !dirRestore.empty()) {
        writeHkcuString(kDirCmdKey, L"", dirRestore);
    } else {
        deleteHkcuKey(kDirCmdKey);
    }

    if (parsedBackup && !driveRestore.empty()) {
        writeHkcuString(kDriveCmdKey, L"", driveRestore);
    } else {
        deleteHkcuKey(kDriveCmdKey);
    }

    if (parsedBackup && !folderRestore.empty()) {
        writeHkcuString(kFolderCmdKey, L"", folderRestore);
    } else {
        deleteHkcuKey(kFolderCmdKey);
    }

    std::filesystem::remove(backupFilePath, ec);
    return true;
}

std::string toUtf8(const std::wstring& w) {
    if (w.empty()) return "";
    int size = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), static_cast<int>(w.size()), nullptr, 0, nullptr, nullptr);
    if (size <= 0) return "";
    std::string s(static_cast<size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), static_cast<int>(w.size()), s.data(), size, nullptr, nullptr);
    return s;
}

std::wstring toWide(const std::string& s) {
    if (s.empty()) return L"";
    int size = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), nullptr, 0);
    if (size <= 0) return L"";
    std::wstring w(static_cast<size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), w.data(), size);
    return w;
}

} // namespace gx::distribution
