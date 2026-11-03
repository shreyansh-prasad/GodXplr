#include "godxplr/platform/mft_raw_enumerator.h"
#include "godxplr/platform/win32_common.h"
#include <cstring>
#include <chrono>

#ifdef _WIN32
#include <winioctl.h>
#endif

namespace gx::platform::win32 {

MftRawEnumerator::MftRawEnumerator(wchar_t volumeLetter)
    : volumeLetter_(volumeLetter) {}

bool MftRawEnumerator::applyFixups(uint8_t* recordData, size_t recordSize) {
    if (!recordData || recordSize < sizeof(MftRecordHeader) || recordSize % 512 != 0) {
        return false;
    }

    const auto* header = reinterpret_cast<const MftRecordHeader*>(recordData);
    if (std::memcmp(header->signature, "FILE", 4) != 0) {
        return false;
    }

    if (header->fixupOffset + header->fixupCount * 2 > recordSize) {
        return false;
    }

    const uint16_t* fixupArray = reinterpret_cast<const uint16_t*>(recordData + header->fixupOffset);
    uint16_t updateSequenceNum = fixupArray[0];

    size_t sectorCount = recordSize / 512;
    if (header->fixupCount < sectorCount + 1) {
        return false;
    }

    for (size_t i = 0; i < sectorCount; ++i) {
        size_t sectorEndOffset = (i + 1) * 512 - 2;
        uint16_t* pSectorEnd = reinterpret_cast<uint16_t*>(recordData + sectorEndOffset);
        if (*pSectorEnd != updateSequenceNum) {
            return false; // Fixup validation failed: possible torn write
        }
        *pSectorEnd = fixupArray[i + 1];
    }

    return true;
}

bool MftRawEnumerator::parseRecord(const uint8_t* recordData, size_t recordSize, MftParsedEntry& outEntry) {
    if (!recordData || recordSize < 1024) {
        return false;
    }

    std::vector<uint8_t> buffer(recordData, recordData + recordSize);
    if (!applyFixups(buffer.data(), buffer.size())) {
        return false;
    }

    const auto* header = reinterpret_cast<const MftRecordHeader*>(buffer.data());
    outEntry.isInUse = (header->flags & 0x01) != 0;
    outEntry.isBaseRecord = (header->baseRecordFrn == 0);

    if (!outEntry.isInUse || !outEntry.isBaseRecord) {
        return false;
    }

    bool isDirectory = (header->flags & 0x02) != 0;
    outEntry.flags = isDirectory ? kDir : 0;
    outEntry.frn = header->recordNumber;

    uint16_t attrOffset = header->attributeOffset;
    bool foundFileName = false;

    while (attrOffset + sizeof(MftAttributeHeader) <= buffer.size()) {
        const auto* attr = reinterpret_cast<const MftAttributeHeader*>(buffer.data() + attrOffset);
        if (attr->type == 0xFFFFFFFF || attr->length == 0) {
            break;
        }

        if (attrOffset + attr->length > buffer.size()) {
            break;
        }

        // 0x10: $STANDARD_INFORMATION
        if (attr->type == 0x10 && attr->nonResident == 0) {
            const auto* res = reinterpret_cast<const MftResidentAttribute*>(attr);
            if (res->valueOffset + sizeof(MftStandardInformationAttribute) <= attr->length) {
                const auto* stdInfo = reinterpret_cast<const MftStandardInformationAttribute*>(
                    reinterpret_cast<const uint8_t*>(attr) + res->valueOffset
                );
                outEntry.mtime = stdInfo->lastWriteTime;
                if (stdInfo->fileAttributes & 0x02) outEntry.flags |= kHidden;
                if (stdInfo->fileAttributes & 0x04) outEntry.flags |= kSystem;
                if (stdInfo->fileAttributes & 0x400) outEntry.flags |= kReparse;
            }
        }
        // 0x30: $FILE_NAME
        else if (attr->type == 0x30 && attr->nonResident == 0) {
            const auto* res = reinterpret_cast<const MftResidentAttribute*>(attr);
            if (res->valueOffset + sizeof(MftFileNameAttribute) <= attr->length) {
                const auto* fnAttr = reinterpret_cast<const MftFileNameAttribute*>(
                    reinterpret_cast<const uint8_t*>(attr) + res->valueOffset
                );

                bool isBetterName = false;
                if (!foundFileName) {
                    isBetterName = true;
                } else if (fnAttr->namespaceType == 1 || fnAttr->namespaceType == 3) {
                    isBetterName = true;
                }

                if (isBetterName) {
                    foundFileName = true;
                    outEntry.parentFrn = fnAttr->parentDirectoryFrn & 0x0000FFFFFFFFFFFFULL;
                    if (outEntry.mtime == 0) {
                        outEntry.mtime = fnAttr->lastWriteTime;
                    }

                    size_t nameCharCount = fnAttr->fileNameLength;
                    std::wstring_view nameW(fnAttr->fileName, nameCharCount);
                    outEntry.name = utf16_to_utf8(nameW);

                    if (outEntry.size == 0) {
                        outEntry.size = fnAttr->realSize;
                    }
                }
            }
        }
        // 0x80: $DATA (default unnamed stream)
        else if (attr->type == 0x80 && attr->nameLength == 0) {
            if (attr->nonResident == 0) {
                const auto* res = reinterpret_cast<const MftResidentAttribute*>(attr);
                outEntry.size = res->valueLength;
            } else {
                const auto* nonRes = reinterpret_cast<const MftNonResidentAttribute*>(attr);
                outEntry.size = nonRes->realSize;
            }
        }

        attrOffset = static_cast<uint16_t>(attrOffset + attr->length);
    }

    return foundFileName && !outEntry.name.empty();
}

#ifdef _WIN32

MftStatus MftRawEnumerator::enumerateVolume(
    NameIndex& outIndex,
    MftEnumerationStats& outStats,
    const std::function<void(size_t)>& progressCallback)
{
    auto tStart = std::chrono::high_resolution_clock::now();

    std::wstring volPath = L"\\\\.\\";
    volPath.push_back(volumeLetter_);
    volPath.push_back(L':');

    ScopedHandle hVol(::CreateFileW(
        volPath.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS,
        nullptr
    ));

    if (!hVol.isValid()) {
        DWORD err = ::GetLastError();
        if (err == ERROR_ACCESS_DENIED) {
            return MftStatus::AccessDenied;
        }
        return MftStatus::IoError;
    }

    NTFS_VOLUME_DATA_BUFFER volData{};
    DWORD bytesReturned = 0;
    BOOL ok = ::DeviceIoControl(
        hVol.get(),
        FSCTL_GET_NTFS_VOLUME_DATA,
        nullptr,
        0,
        &volData,
        sizeof(volData),
        &bytesReturned,
        nullptr
    );

    if (!ok) {
        DWORD err = ::GetLastError();
        if (err == ERROR_INVALID_FUNCTION) {
            return MftStatus::NotNtfsVolume;
        }
        return MftStatus::IoError;
    }

    const DWORD bytesPerRecord = volData.BytesPerFileRecordSegment > 0 ? volData.BytesPerFileRecordSegment : 1024;
    const uint64_t totalRecords = static_cast<uint64_t>(volData.MftValidDataLength.QuadPart / bytesPerRecord);

    outIndex.reserve(250000, 250000 * 32);

    std::vector<uint8_t> outBuffer(sizeof(NTFS_FILE_RECORD_OUTPUT_BUFFER) + bytesPerRecord);
    auto* pOutRecord = reinterpret_cast<PNTFS_FILE_RECORD_OUTPUT_BUFFER>(outBuffer.data());

    for (uint64_t frn = 0; frn < totalRecords; ++frn) {
        NTFS_FILE_RECORD_INPUT_BUFFER inBuffer{};
        inBuffer.FileReferenceNumber.QuadPart = static_cast<LONGLONG>(frn);

        DWORD retBytes = 0;
        BOOL recOk = ::DeviceIoControl(
            hVol.get(),
            FSCTL_GET_NTFS_FILE_RECORD,
            &inBuffer,
            sizeof(inBuffer),
            pOutRecord,
            static_cast<DWORD>(outBuffer.size()),
            &retBytes,
            nullptr
        );

        outStats.totalRecordsRead++;

        if (recOk && pOutRecord->FileRecordLength >= 1024) {
            MftParsedEntry entry;
            if (parseRecord(pOutRecord->FileRecordBuffer, pOutRecord->FileRecordLength, entry)) {
                outStats.inUseRecords++;
                if (entry.flags & kDir) {
                    outStats.directoriesCount++;
                } else {
                    outStats.filesCount++;
                }

                outIndex.add(frn, entry.parentFrn, entry.name, entry.flags | kMetaKnown, entry.size, entry.mtime);
            }
        }

        if (progressCallback && (outStats.totalRecordsRead % 16384 == 0)) {
            progressCallback(outStats.inUseRecords);
        }
    }

    outIndex.resolveParents();

    auto tEnd = std::chrono::high_resolution_clock::now();
    outStats.elapsedMs = std::chrono::duration<double, std::milli>(tEnd - tStart).count();

    return MftStatus::Success;
}

#endif // _WIN32

} // namespace gx::platform::win32
