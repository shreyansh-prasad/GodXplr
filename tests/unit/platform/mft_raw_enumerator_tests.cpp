#include "godxplr/platform/mft_raw_enumerator.h"
#include <iostream>
#include <vector>
#include <cstring>
#include <cstdlib>

#define TEST_ASSERT(cond) do { \
    if (!(cond)) { \
        std::cerr << "Assertion failed: " #cond " at " << __FILE__ << ":" << __LINE__ << std::endl; \
        std::abort(); \
    } \
} while (0)

using namespace gx;
using namespace gx::platform::win32;

// Helper to construct a synthetic 1024-byte MFT record for deterministic testing
std::vector<uint8_t> createSyntheticMftRecord(
    const std::wstring& name,
    uint64_t parentFrn,
    bool isDirectory,
    uint64_t fileSize,
    uint64_t mtime,
    bool isNonResidentData = false)
{
    std::vector<uint8_t> buf(1024, 0);

    // 1. MFT Record Header
    auto* header = reinterpret_cast<MftRecordHeader*>(buf.data());
    std::memcpy(header->signature, "FILE", 4);
    header->fixupOffset = 48;
    header->fixupCount = 3; // 1 update sequence number + 2 sectors
    header->sequenceNumber = 1;
    header->linkCount = 1;
    header->flags = 0x0001 | (isDirectory ? 0x0002 : 0x0000); // InUse (+ Directory)
    header->bytesInUse = 1024;
    header->bytesAllocated = 1024;
    header->baseRecordFrn = 0;
    header->recordNumber = 12345;

    // Fixup setup
    const uint16_t updateSeqNum = 0x55AA;
    uint16_t* fixupArray = reinterpret_cast<uint16_t*>(buf.data() + header->fixupOffset);
    fixupArray[0] = updateSeqNum;
    fixupArray[1] = 0x1111; // Original sector 0 end
    fixupArray[2] = 0x2222; // Original sector 1 end

    // Write update sequence number to sector ends (simulating raw on-disk state)
    *reinterpret_cast<uint16_t*>(buf.data() + 510) = updateSeqNum;
    *reinterpret_cast<uint16_t*>(buf.data() + 1022) = updateSeqNum;

    uint16_t currentOffset = 64; // Attributes start after fixups
    header->attributeOffset = currentOffset;

    // 2. $STANDARD_INFORMATION attribute (0x10)
    {
        auto* attr = reinterpret_cast<MftAttributeHeader*>(buf.data() + currentOffset);
        attr->type = 0x10;
        attr->length = sizeof(MftResidentAttribute) + sizeof(MftStandardInformationAttribute);
        attr->nonResident = 0;
        attr->attributeId = 1;

        auto* res = reinterpret_cast<MftResidentAttribute*>(attr);
        res->valueOffset = sizeof(MftResidentAttribute);
        res->valueLength = sizeof(MftStandardInformationAttribute);

        auto* stdInfo = reinterpret_cast<MftStandardInformationAttribute*>(buf.data() + currentOffset + res->valueOffset);
        stdInfo->lastWriteTime = mtime;
        stdInfo->fileAttributes = isDirectory ? 0x10 : 0x20; // Directory or Archive

        currentOffset = static_cast<uint16_t>(currentOffset + attr->length);
    }

    // 3. $FILE_NAME attribute (0x30)
    {
        uint16_t fnSize = static_cast<uint16_t>(sizeof(MftFileNameAttribute) + name.size() * sizeof(wchar_t));
        // Align to 8 bytes
        fnSize = (fnSize + 7) & ~7;

        auto* attr = reinterpret_cast<MftAttributeHeader*>(buf.data() + currentOffset);
        attr->type = 0x30;
        attr->length = sizeof(MftResidentAttribute) + fnSize;
        attr->nonResident = 0;
        attr->attributeId = 2;

        auto* res = reinterpret_cast<MftResidentAttribute*>(attr);
        res->valueOffset = sizeof(MftResidentAttribute);
        res->valueLength = fnSize;

        auto* fnAttr = reinterpret_cast<MftFileNameAttribute*>(buf.data() + currentOffset + res->valueOffset);
        fnAttr->parentDirectoryFrn = parentFrn;
        fnAttr->lastWriteTime = mtime;
        fnAttr->realSize = fileSize;
        fnAttr->fileNameLength = static_cast<uint8_t>(name.size());
        fnAttr->namespaceType = 3; // Win32 & DOS
        std::memcpy(fnAttr->fileName, name.data(), name.size() * sizeof(wchar_t));

        currentOffset = static_cast<uint16_t>(currentOffset + attr->length);
    }

    // 4. $DATA attribute (0x80)
    if (!isDirectory) {
        if (!isNonResidentData) {
            // Resident data
            auto* attr = reinterpret_cast<MftAttributeHeader*>(buf.data() + currentOffset);
            attr->type = 0x80;
            attr->length = sizeof(MftResidentAttribute) + 64;
            attr->nonResident = 0;
            attr->attributeId = 3;

            auto* res = reinterpret_cast<MftResidentAttribute*>(attr);
            res->valueOffset = sizeof(MftResidentAttribute);
            res->valueLength = static_cast<uint32_t>(fileSize);

            currentOffset = static_cast<uint16_t>(currentOffset + attr->length);
        } else {
            // Non-resident data
            auto* attr = reinterpret_cast<MftAttributeHeader*>(buf.data() + currentOffset);
            attr->type = 0x80;
            attr->length = sizeof(MftNonResidentAttribute);
            attr->nonResident = 1;
            attr->attributeId = 3;

            auto* nonRes = reinterpret_cast<MftNonResidentAttribute*>(attr);
            nonRes->realSize = fileSize;
            nonRes->allocatedSize = (fileSize + 4095) & ~4095;

            currentOffset = static_cast<uint16_t>(currentOffset + attr->length);
        }
    }

    // 5. End marker (0xFFFFFFFF)
    auto* endAttr = reinterpret_cast<MftAttributeHeader*>(buf.data() + currentOffset);
    endAttr->type = 0xFFFFFFFF;
    endAttr->length = 0;

    return buf;
}

void testFixupValidation() {
    std::cout << "[Test] MFT Multi-Sector Fixup Validation..." << std::endl;

    auto record = createSyntheticMftRecord(L"sample.txt", 5, false, 1024, 133000000000000000ULL);

    // Apply fixup
    TEST_ASSERT(MftRawEnumerator::applyFixups(record.data(), record.size()));

    // Verify sector ends were restored to original values
    uint16_t sector0End = *reinterpret_cast<const uint16_t*>(record.data() + 510);
    uint16_t sector1End = *reinterpret_cast<const uint16_t*>(record.data() + 1022);
    TEST_ASSERT(sector0End == 0x1111);
    TEST_ASSERT(sector1End == 0x2222);

    // Corrupt one sector end and verify detection
    auto corruptRecord = createSyntheticMftRecord(L"sample.txt", 5, false, 1024, 133000000000000000ULL);
    *reinterpret_cast<uint16_t*>(corruptRecord.data() + 510) = 0xDEAD; // Corrupted
    TEST_ASSERT(!MftRawEnumerator::applyFixups(corruptRecord.data(), corruptRecord.size()));
}

void testParseResidentFileRecord() {
    std::cout << "[Test] Parse Resident MFT Record..." << std::endl;

    const uint64_t expectedMtime = 133123456780000000ULL;
    auto record = createSyntheticMftRecord(L"GodXplr_Architecture.pdf", 42, false, 2048, expectedMtime, false);

    MftParsedEntry entry{};
    TEST_ASSERT(MftRawEnumerator::parseRecord(record.data(), record.size(), entry));

    TEST_ASSERT(entry.name == "GodXplr_Architecture.pdf");
    TEST_ASSERT(entry.parentFrn == 42);
    TEST_ASSERT((entry.flags & kDir) == 0);
    TEST_ASSERT(entry.size == 2048);
    TEST_ASSERT(entry.mtime == expectedMtime);
    TEST_ASSERT(entry.isInUse);
    TEST_ASSERT(entry.isBaseRecord);
}

void testParseNonResidentFileRecord() {
    std::cout << "[Test] Parse Non-Resident MFT Record..." << std::endl;

    const uint64_t largeSize = 104857600ULL; // 100 MB
    const uint64_t expectedMtime = 133987654320000000ULL;
    auto record = createSyntheticMftRecord(L"LargeVideo.mp4", 100, false, largeSize, expectedMtime, true);

    MftParsedEntry entry{};
    TEST_ASSERT(MftRawEnumerator::parseRecord(record.data(), record.size(), entry));

    TEST_ASSERT(entry.name == "LargeVideo.mp4");
    TEST_ASSERT(entry.parentFrn == 100);
    TEST_ASSERT((entry.flags & kDir) == 0);
    TEST_ASSERT(entry.size == largeSize);
    TEST_ASSERT(entry.mtime == expectedMtime);
}

void testParseDirectoryRecord() {
    std::cout << "[Test] Parse Directory MFT Record..." << std::endl;

    auto record = createSyntheticMftRecord(L"Projects", 5, true, 0, 133000000000000000ULL);

    MftParsedEntry entry{};
    TEST_ASSERT(MftRawEnumerator::parseRecord(record.data(), record.size(), entry));

    TEST_ASSERT(entry.name == "Projects");
    TEST_ASSERT(entry.parentFrn == 5);
    TEST_ASSERT((entry.flags & kDir) != 0);
    TEST_ASSERT(entry.size == 0);
}

void testLiveVolumeEnumerateSafe() {
    std::cout << "[Test] Live Volume Enumeration Safety..." << std::endl;

#ifdef _WIN32
    MftRawEnumerator enumerator(L'C');
    TEST_ASSERT(enumerator.getVolumeLetter() == L'C');

    NameIndex index;
    MftEnumerationStats stats{};

    // If not elevated, enumerateVolume must return AccessDenied cleanly without crash or hang
    MftStatus status = enumerator.enumerateVolume(index, stats);
    if (status == MftStatus::AccessDenied) {
        std::cout << "  Volume C: requires administrator elevation for raw MFT access (AccessDenied handled cleanly)\n";
    } else if (status == MftStatus::Success) {
        std::cout << "  Enumerated " << stats.inUseRecords << " records in " << stats.elapsedMs << " ms\n";
    }
#endif
}

int main() {
    std::cout << "=== GodXplr MFT Raw Enumerator Tests (Spec 01 S9) ===" << std::endl;

    testFixupValidation();
    testParseResidentFileRecord();
    testParseNonResidentFileRecord();
    testParseDirectoryRecord();
    testLiveVolumeEnumerateSafe();

    std::cout << "All MFT Raw Enumerator tests PASSED!" << std::endl;
    return 0;
}

