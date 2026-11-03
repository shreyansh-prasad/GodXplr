#pragma once

#include "godxplr/core/name_index.h"
#include <cstdint>
#include <string>
#include <vector>
#include <optional>
#include <functional>

namespace gx::platform::win32 {

#pragma pack(push, 1)

// NTFS File Record Segment Multi-Sector Header
struct MftRecordHeader {
    char signature[4];          // "FILE"
    uint16_t fixupOffset;       // Offset to Update Sequence Array
    uint16_t fixupCount;        // Size of Update Sequence Array (in words)
    uint64_t lsn;               // $LogFile Sequence Number
    uint16_t sequenceNumber;    // Sequence Number
    uint16_t linkCount;         // Hard link count
    uint16_t attributeOffset;   // Offset to first attribute
    uint16_t flags;             // 0x01: InUse, 0x02: Directory
    uint32_t bytesInUse;        // Real size of record
    uint32_t bytesAllocated;    // Allocated size of record (1024)
    uint64_t baseRecordFrn;     // Base record FRN (0 if base record)
    uint16_t nextAttributeId;   // Next attribute instance ID
    uint16_t padding;
    uint32_t recordNumber;      // MFT Record Number
};

// NTFS Attribute Common Header
struct MftAttributeHeader {
    uint32_t type;              // 0x10: $STANDARD_INFORMATION, 0x30: $FILE_NAME, 0x80: $DATA
    uint32_t length;            // Total length including header
    uint8_t nonResident;        // 0: Resident, 1: Non-Resident
    uint8_t nameLength;         // Name length in characters
    uint16_t nameOffset;        // Offset to attribute name
    uint16_t flags;             // 0x01: Compressed, 0x4000: Encrypted, 0x8000: Sparse
    uint16_t attributeId;       // Instance ID
};

struct MftResidentAttribute {
    MftAttributeHeader header;
    uint32_t valueLength;
    uint16_t valueOffset;
    uint8_t indexedFlag;
    uint8_t padding;
};

struct MftNonResidentAttribute {
    MftAttributeHeader header;
    uint64_t startVcn;
    uint64_t endVcn;
    uint16_t dataRunsOffset;
    uint16_t compressionUnit;
    uint32_t padding;
    uint64_t allocatedSize;
    uint64_t realSize;
    uint64_t initializedSize;
};

struct MftFileNameAttribute {
    uint64_t parentDirectoryFrn;
    uint64_t creationTime;
    uint64_t lastWriteTime;
    uint64_t mtime;
    uint64_t lastAccessTime;
    uint64_t allocatedSize;
    uint64_t realSize;
    uint32_t flags;
    uint32_t reparseTag;
    uint8_t fileNameLength;     // in characters
    uint8_t namespaceType;      // 0: POSIX, 1: Win32, 2: DOS, 3: Win32 & DOS
    wchar_t fileName[1];
};

struct MftStandardInformationAttribute {
    uint64_t creationTime;
    uint64_t lastWriteTime;
    uint64_t mtime;
    uint64_t lastAccessTime;
    uint32_t fileAttributes;
    uint32_t maxVersions;
    uint32_t versionNumber;
    uint32_t classId;
};

#pragma pack(pop)

struct MftParsedEntry {
    uint64_t frn{0};
    uint64_t parentFrn{0};
    std::string name;
    uint8_t flags{0};
    uint64_t size{0};
    uint64_t mtime{0};
    bool isInUse{false};
    bool isBaseRecord{true};
};

struct MftEnumerationStats {
    size_t totalRecordsRead{0};
    size_t inUseRecords{0};
    size_t filesCount{0};
    size_t directoriesCount{0};
    double elapsedMs{0.0};
};

enum class MftStatus : uint8_t {
    Success,
    AccessDenied,
    NotNtfsVolume,
    InvalidRecord,
    IoError
};

class MftRawEnumerator {
public:
    explicit MftRawEnumerator(wchar_t volumeLetter = L'C');
    ~MftRawEnumerator() = default;

    // Pure record parser (independent of disk, testable with synthetic bytes or real MFT records)
    static bool parseRecord(const uint8_t* recordData, size_t recordSize, MftParsedEntry& outEntry);

    // Applies NTFS multi-sector fixup array to 1024-byte record buffer
    static bool applyFixups(uint8_t* recordData, size_t recordSize);

#ifdef _WIN32
    // Enumerates all MFT records on the specified NTFS volume in one pass (extracting sizes and times)
    MftStatus enumerateVolume(
        NameIndex& outIndex,
        MftEnumerationStats& outStats,
        const std::function<void(size_t recordsSoFar)>& progressCallback = nullptr);
#endif

    [[nodiscard]] wchar_t getVolumeLetter() const noexcept { return volumeLetter_; }

private:
    wchar_t volumeLetter_{L'C'};
};

} // namespace gx::platform::win32
