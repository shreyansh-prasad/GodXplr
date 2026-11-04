#pragma once

#include "godxplr/providers/virtual_provider.h"
#include <fstream>
#include <unordered_map>

namespace gx::providers {

#pragma pack(push, 1)
struct ZipLocalHeader {
    uint32_t signature{0};      // 0x04034b50
    uint16_t versionNeeded{0};
    uint16_t generalFlags{0};
    uint16_t compressionMethod{0};
    uint16_t lastModTime{0};
    uint16_t lastModDate{0};
    uint32_t crc32{0};
    uint32_t compressedSize{0};
    uint32_t uncompressedSize{0};
    uint16_t filenameLength{0};
    uint16_t extraFieldLength{0};
};

struct ZipCentralDirectoryHeader {
    uint32_t signature{0};      // 0x02014b50
    uint16_t versionMadeBy{0};
    uint16_t versionNeeded{0};
    uint16_t generalFlags{0};
    uint16_t compressionMethod{0};
    uint16_t lastModTime{0};
    uint16_t lastModDate{0};
    uint32_t crc32{0};
    uint32_t compressedSize{0};
    uint32_t uncompressedSize{0};
    uint16_t filenameLength{0};
    uint16_t extraFieldLength{0};
    uint16_t commentLength{0};
    uint16_t diskNumberStart{0};
    uint16_t internalAttrs{0};
    uint32_t externalAttrs{0};
    uint32_t localHeaderOffset{0};
};

struct ZipEndOfCentralDirectory {
    uint32_t signature{0};      // 0x06054b50
    uint16_t diskNumber{0};
    uint16_t startDiskNumber{0};
    uint16_t totalEntriesDisk{0};
    uint16_t totalEntries{0};
    uint32_t centralDirSize{0};
    uint32_t centralDirOffset{0};
    uint16_t commentLength{0};
};
#pragma pack(pop)

class ZipArchiveProvider : public IVirtualProvider {
public:
    ZipArchiveProvider() = default;
    ~ZipArchiveProvider() override;

    ZipArchiveProvider(const ZipArchiveProvider&) = delete;
    ZipArchiveProvider& operator=(const ZipArchiveProvider&) = delete;

    [[nodiscard]] ProviderCapability capabilities() const noexcept override {
        return ProviderCapability::Enumerate | ProviderCapability::ReadStream;
    }

    bool open(const std::wstring& containerPath) override;
    [[nodiscard]] std::vector<VirtualItem> enumerateDirectory(const std::wstring& virtualSubDir = L"") override;
    bool extractItem(const std::wstring& relativePath, const std::wstring& destDiskPath) override;
    void close() override;

    [[nodiscard]] size_t totalItemsCount() const noexcept { return items_.size(); }

private:
    std::wstring archivePath_;
    std::ifstream file_;
    std::vector<VirtualItem> items_;
    std::unordered_map<std::wstring, uint32_t> localOffsets_;

    bool parseCentralDirectory();
};

} // namespace gx::providers

