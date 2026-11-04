#include "godxplr/distribution/update_verifier.h"

#include <fstream>
#include <iomanip>
#include <sstream>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <wincrypt.h>

namespace gx::distribution {

namespace {

std::string bytesToHex(const uint8_t* data, size_t len) {
    std::ostringstream oss;
    for (size_t i = 0; i < len; ++i) {
        oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(data[i]);
    }
    return oss.str();
}

} // namespace

std::string UpdateVerifier::computeFileSha256(const std::filesystem::path& filePath) {
    HCRYPTPROV hProv = 0;
    HCRYPTHASH hHash = 0;

    if (!CryptAcquireContextW(&hProv, nullptr, nullptr, PROV_RSA_AES, CRYPT_VERIFYCONTEXT)) {
        return "";
    }

    if (!CryptCreateHash(hProv, CALG_SHA_256, 0, 0, &hHash)) {
        CryptReleaseContext(hProv, 0);
        return "";
    }

    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
        CryptDestroyHash(hHash);
        CryptReleaseContext(hProv, 0);
        return "";
    }

    std::vector<char> buffer(64 * 1024);
    while (file.read(buffer.data(), static_cast<std::streamsize>(buffer.size())) || file.gcount() > 0) {
        if (!CryptHashData(hHash, reinterpret_cast<const BYTE*>(buffer.data()), static_cast<DWORD>(file.gcount()), 0)) {
            CryptDestroyHash(hHash);
            CryptReleaseContext(hProv, 0);
            return "";
        }
    }

    BYTE hash[32];
    DWORD hashLen = sizeof(hash);
    std::string hex = "";

    if (CryptGetHashParam(hHash, HP_HASHVAL, hash, &hashLen, 0)) {
        hex = bytesToHex(hash, hashLen);
    }

    CryptDestroyHash(hHash);
    CryptReleaseContext(hProv, 0);
    return hex;
}

bool UpdateVerifier::verifyPackage(
    const std::filesystem::path& packagePath,
    const UpdateManifest& manifest)
{
    std::error_code ec;
    if (!std::filesystem::exists(packagePath, ec)) {
        return false;
    }

    if (manifest.packageSizeBytes > 0) {
        uint64_t actualSize = std::filesystem::file_size(packagePath, ec);
        if (actualSize != manifest.packageSizeBytes) {
            return false;
        }
    }

    if (!manifest.sha256Hex.empty()) {
        std::string actualHash = computeFileSha256(packagePath);
        if (actualHash != manifest.sha256Hex) {
            return false;
        }
    }

    return true;
}

bool UpdateVerifier::stageUpdate(
    const std::filesystem::path& packagePath,
    const std::filesystem::path& targetDir)
{
    std::error_code ec;
    std::filesystem::path stageDir = targetDir / ".gxupdate";
    std::filesystem::create_directories(stageDir, ec);

    std::filesystem::path stagedTarget = stageDir / packagePath.filename();
    std::filesystem::copy_file(
        packagePath,
        stagedTarget,
        std::filesystem::copy_options::overwrite_existing,
        ec);

    return !ec;
}

bool UpdateVerifier::cleanStagedUpdate(const std::filesystem::path& targetDir) {
    std::error_code ec;
    std::filesystem::path stageDir = targetDir / ".gxupdate";
    if (std::filesystem::exists(stageDir, ec)) {
        std::filesystem::remove_all(stageDir, ec);
        return !ec;
    }
    return true;
}

} // namespace gx::distribution

