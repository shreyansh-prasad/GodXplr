#include "godxplr/providers/virtual_provider.h"
#include "godxplr/providers/zip_archive_provider.h"
#include "godxplr/fileops/streaming_file_executor.h"

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

using namespace gx::providers;

class ArchiveTestFixture {
public:
    ArchiveTestFixture() {
        wchar_t tempPath[MAX_PATH];
        GetTempPathW(MAX_PATH, tempPath);
        root_ = std::filesystem::path(tempPath) / (L"GodXplr_ArchiveTests_" + std::to_wstring(GetTickCount64()));
        std::error_code ec;
        std::filesystem::create_directories(root_, ec);
    }

    ~ArchiveTestFixture() {
        std::error_code ec;
        std::filesystem::remove_all(root_, ec);
    }

    [[nodiscard]] std::wstring getPath(const std::wstring& rel) const {
        return (root_ / rel).wstring();
    }

    // Creates a valid synthetic uncompressed (Stored) ZIP file
    std::wstring createSyntheticZip(
        const std::wstring& zipName,
        const std::string& entryName,
        const std::string& entryData)
    {
        std::wstring zipPath = getPath(zipName);
        std::ofstream out(zipPath, std::ios::binary);

        uint32_t crc = gx::fileops::StreamingFileExecutor::computeCrc32(
            reinterpret_cast<const uint8_t*>(entryData.data()), entryData.size());

        // 1. Local File Header
        ZipLocalHeader lh{};
        lh.signature = 0x04034b50;
        lh.versionNeeded = 20;
        lh.generalFlags = 0;
        lh.compressionMethod = 0; // Stored
        lh.lastModTime = 0x4800;
        lh.lastModDate = 0x5821;
        lh.crc32 = crc;
        lh.compressedSize = static_cast<uint32_t>(entryData.size());
        lh.uncompressedSize = static_cast<uint32_t>(entryData.size());
        lh.filenameLength = static_cast<uint16_t>(entryName.size());
        lh.extraFieldLength = 0;

        uint32_t localOffset = static_cast<uint32_t>(out.tellp());
        out.write(reinterpret_cast<const char*>(&lh), sizeof(lh));
        out.write(entryName.data(), entryName.size());
        out.write(entryData.data(), entryData.size());

        // 2. Central Directory Header
        uint32_t cdOffset = static_cast<uint32_t>(out.tellp());
        ZipCentralDirectoryHeader cdh{};
        cdh.signature = 0x02014b50;
        cdh.versionMadeBy = 20;
        cdh.versionNeeded = 20;
        cdh.generalFlags = 0;
        cdh.compressionMethod = 0;
        cdh.lastModTime = lh.lastModTime;
        cdh.lastModDate = lh.lastModDate;
        cdh.crc32 = crc;
        cdh.compressedSize = lh.compressedSize;
        cdh.uncompressedSize = lh.uncompressedSize;
        cdh.filenameLength = static_cast<uint16_t>(entryName.size());
        cdh.extraFieldLength = 0;
        cdh.commentLength = 0;
        cdh.diskNumberStart = 0;
        cdh.internalAttrs = 0;
        cdh.externalAttrs = 0x20; // Archive file
        cdh.localHeaderOffset = localOffset;

        out.write(reinterpret_cast<const char*>(&cdh), sizeof(cdh));
        out.write(entryName.data(), entryName.size());

        uint32_t cdSize = static_cast<uint32_t>(out.tellp()) - cdOffset;

        // 3. End of Central Directory
        ZipEndOfCentralDirectory eocd{};
        eocd.signature = 0x06054b50;
        eocd.diskNumber = 0;
        eocd.startDiskNumber = 0;
        eocd.totalEntriesDisk = 1;
        eocd.totalEntries = 1;
        eocd.centralDirSize = cdSize;
        eocd.centralDirOffset = cdOffset;
        eocd.commentLength = 0;

        out.write(reinterpret_cast<const char*>(&eocd), sizeof(eocd));
        out.close();

        return zipPath;
    }

private:
    std::filesystem::path root_;
};

void testPathSanitizer() {
    std::cout << "[Test] PathSanitizer Security & Traversal Defense...\n";

    // Safe relative paths
    TEST_ASSERT(PathSanitizer::isSafePath("readme.txt"));
    TEST_ASSERT(PathSanitizer::isSafePath("docs/architecture/spec.md"));
    TEST_ASSERT(PathSanitizer::sanitizeRelativePath("docs/architecture/spec.md") == L"docs\\architecture\\spec.md");

    // Malicious traversal attacks (must be rejected!)
    TEST_ASSERT(!PathSanitizer::isSafePath("../secret.txt"));
    TEST_ASSERT(!PathSanitizer::isSafePath("docs/../../secret.txt"));
    TEST_ASSERT(!PathSanitizer::isSafePath("/etc/passwd"));
    TEST_ASSERT(!PathSanitizer::isSafePath("\\Windows\\System32\\cmd.exe"));
    TEST_ASSERT(!PathSanitizer::isSafePath("C:\\autoexec.bat"));
    TEST_ASSERT(!PathSanitizer::isSafePath("D:file.txt"));
    TEST_ASSERT(!PathSanitizer::isSafePath(std::string("file\0.txt", 9)));

    std::cout << "  -> PASS\n";
}

void testZipArchiveProvider(ArchiveTestFixture& fix) {
    std::cout << "[Test] ZipArchiveProvider Parsing & Extraction...\n";

    const std::string entryName = "inner_folder/sample.txt";
    const std::string entryData = "GodXplr Virtual Filesystem Content from inside Archive!";

    std::wstring zipPath = fix.createSyntheticZip(L"archive.zip", entryName, entryData);

    ZipArchiveProvider provider;
    TEST_ASSERT((provider.capabilities() & ProviderCapability::Enumerate) != ProviderCapability::None);

    bool opened = provider.open(zipPath);
    TEST_ASSERT(opened);
    TEST_ASSERT(provider.totalItemsCount() == 1);

    // Enumerate inside inner_folder
    auto items = provider.enumerateDirectory(L"inner_folder");
    TEST_ASSERT(items.size() == 1);
    TEST_ASSERT(items[0].name == L"sample.txt");
    TEST_ASSERT(items[0].uncompressedSize == entryData.size());
    TEST_ASSERT(!items[0].isEncrypted);

    // Extract item
    std::wstring outPath = fix.getPath(L"extracted_sample.txt");
    bool extracted = provider.extractItem(items[0].relativePath, outPath);
    TEST_ASSERT(extracted);

    std::ifstream in(outPath, std::ios::binary);
    std::string readBack((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    TEST_ASSERT(readBack == entryData);

    provider.close();
    std::cout << "  -> PASS\n";
}

int main() {
    std::cout << "========================================\n";
    std::cout << "  GodXplr Spec 07 Archive Provider Tests\n";
    std::cout << "========================================\n";

    ArchiveTestFixture fix;

    testPathSanitizer();
    testZipArchiveProvider(fix);

    std::cout << "========================================\n";
    std::cout << "  ALL ARCHIVE TESTS PASSED (100%)       \n";
    std::cout << "========================================\n";
    return 0;
}

