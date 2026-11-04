#include "godxplr/distribution/portable_mode.h"
#include "godxplr/distribution/registry_manager.h"
#include "godxplr/distribution/service_controller.h"
#include "godxplr/distribution/update_verifier.h"

#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <cstdlib>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#define TEST_ASSERT(cond) \
    do { \
        if (!(cond)) { \
            std::cerr << "Assertion failed at line " << __LINE__ << ": " #cond "\n"; \
            std::abort(); \
        } \
    } while (0)

using namespace gx::distribution;
namespace fs = std::filesystem;

class DistTestFixture {
public:
    DistTestFixture() {
        wchar_t tempPath[MAX_PATH];
        GetTempPathW(MAX_PATH, tempPath);
        root_ = fs::path(tempPath) / (L"GodXplr_DistTests_" + std::to_wstring(GetTickCount64()));
        std::error_code ec;
        fs::create_directories(root_, ec);
    }

    ~DistTestFixture() {
        std::error_code ec;
        fs::remove_all(root_, ec);
    }

    [[nodiscard]] fs::path path(const std::string& name) const {
        return root_ / name;
    }

private:
    fs::path root_;
};

void testPortableMode(DistTestFixture& fix) {
    std::cout << "[Test] PortableMode Resolution...\n";

    fs::path appDir = fix.path("app_bin");
    fs::create_directories(appDir);

    // 1. Installed mode by default (no sentinel)
    TEST_ASSERT(!PortableMode::isPortable(appDir));
    auto dataDirInstalled = PortableMode::getDataDirectory(appDir);
    TEST_ASSERT(dataDirInstalled.string().find("GodXplr") != std::string::npos);

    // 2. Portable mode with sentinel
    fs::path sentinel = appDir / "godxplr.portable";
    {
        std::ofstream sfile(sentinel);
        sfile << "portable mode active";
    }
    TEST_ASSERT(PortableMode::isPortable(appDir));

    auto dataDirPortable = PortableMode::getDataDirectory(appDir);
    TEST_ASSERT(dataDirPortable == (appDir / "data"));

    auto logDirPortable = PortableMode::getLogDirectory(appDir);
    TEST_ASSERT(logDirPortable == (appDir / "logs"));

    std::cout << "  -> PASS\n";
}

void testUpdateVerifier(DistTestFixture& fix) {
    std::cout << "[Test] UpdateVerifier SHA-256 & Staging...\n";

    fs::path pkg = fix.path("godxplr_v1.0.1_update.bin");
    const std::string payload = "GodXplr Production Update Binary Payload - High Performance";
    {
        std::ofstream out(pkg, std::ios::binary);
        out << payload;
    }

    std::string sha = UpdateVerifier::computeFileSha256(pkg);
    TEST_ASSERT(!sha.empty());
    std::cout << "  Computed SHA-256: " << sha << "\n";

    UpdateManifest validManifest;
    validManifest.version = "1.0.1";
    validManifest.packageSizeBytes = payload.size();
    validManifest.sha256Hex = sha;

    TEST_ASSERT(UpdateVerifier::verifyPackage(pkg, validManifest));

    // Tampered checksum test
    UpdateManifest tamperedManifest = validManifest;
    tamperedManifest.sha256Hex = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
    TEST_ASSERT(!UpdateVerifier::verifyPackage(pkg, tamperedManifest));

    // Staging test
    fs::path targetDir = fix.path("install_root");
    fs::create_directories(targetDir);

    bool staged = UpdateVerifier::stageUpdate(pkg, targetDir);
    TEST_ASSERT(staged);
    TEST_ASSERT(fs::exists(targetDir / ".gxupdate" / pkg.filename()));

    // Cleanup test
    bool cleaned = UpdateVerifier::cleanStagedUpdate(targetDir);
    TEST_ASSERT(cleaned);
    TEST_ASSERT(!fs::exists(targetDir / ".gxupdate"));

    std::cout << "  -> PASS\n";
}

void testRegistryManager(DistTestFixture& fix) {
    std::cout << "[Test] RegistryManager Reversible Operations...\n";

    const std::wstring testKey = L"Software\\Classes\\GodXplr_TestVerb\\shell\\open\\command";
    const std::wstring originalVal = L"explorer.exe /separate,\"%1\"";
    const std::wstring godxplrVal = L"\"C:\\Program Files\\GodXplr\\godxplr.exe\" \"%1\"";

    // 1. Write initial state
    bool okWrite = RegistryManager::writeHkcuString(testKey, L"", originalVal);
    TEST_ASSERT(okWrite);

    auto readVal = RegistryManager::readHkcuString(testKey, L"");
    TEST_ASSERT(readVal.has_value() && *readVal == originalVal);

    // 2. Set default opener with backup
    fs::path backupFile = fix.path("registry_backup.txt");
    bool okDefault = RegistryManager::setDefaultOpener(L"C:\\Program Files\\GodXplr\\godxplr.exe", backupFile);
    TEST_ASSERT(okDefault);
    TEST_ASSERT(fs::exists(backupFile));
    TEST_ASSERT(RegistryManager::isDefaultOpener(L"C:\\Program Files\\GodXplr\\godxplr.exe"));

    // 3. Restore default explorer
    bool okRestore = RegistryManager::restoreDefaultExplorer(backupFile);
    TEST_ASSERT(okRestore);
    TEST_ASSERT(!fs::exists(backupFile)); // Backup file cleaned up

    // Clean test key
    RegistryManager::deleteHkcuKey(testKey);
    RegistryManager::deleteHkcuKey(L"Software\\Classes\\GodXplr_TestVerb\\shell\\open");
    RegistryManager::deleteHkcuKey(L"Software\\Classes\\GodXplr_TestVerb\\shell");
    RegistryManager::deleteHkcuKey(L"Software\\Classes\\GodXplr_TestVerb");

    std::cout << "  -> PASS\n";
}

void testServiceController() {
    std::cout << "[Test] ServiceController Query...\n";

    auto status = ServiceController::queryStatus(L"GodXplrNonExistentService9999");
    TEST_ASSERT(status == ServiceStatus::NotFound);

    std::cout << "  -> PASS\n";
}

int main() {
    std::cout << "==============================================\n";
    std::cout << "  GodXplr Spec 09 Distribution & Lifecycle   \n";
    std::cout << "==============================================\n";

    DistTestFixture fix;

    testPortableMode(fix);
    testUpdateVerifier(fix);
    testRegistryManager(fix);
    testServiceController();

    std::cout << "==============================================\n";
    std::cout << "  ALL DISTRIBUTION TESTS PASSED (100%)        \n";
    std::cout << "==============================================\n";
    return 0;
}

