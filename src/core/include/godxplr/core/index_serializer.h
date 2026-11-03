#pragma once

#include "godxplr/core/name_index.h"
#include <cstdint>
#include <string>

namespace gx {

#pragma pack(push, 1)
struct SnapshotHeader {
    char magic[4]{'G', 'X', 'I', 'X'};
    uint32_t formatVersion{1};
    uint32_t volumeSerial{0};
    uint64_t journalId{0};
    uint64_t nextUsn{0};
    uint32_t entryCount{0};
    uint32_t origArenaBytes{0};
    uint32_t foldArenaBytes{0};
    uint32_t checksum{0};
};
#pragma pack(pop)

class IndexSerializer {
public:
    // Saves in-memory NameIndex snapshot to disk atomically
    static bool save(const NameIndex& index,
                     uint32_t volumeSerial,
                     uint64_t journalId,
                     uint64_t nextUsn,
                     const std::string& destinationPath);

    // Loads NameIndex from disk snapshot and reconstructs acceleration indices
    static bool load(NameIndex& outIndex,
                     uint32_t& outVolumeSerial,
                     uint64_t& outJournalId,
                     uint64_t& outNextUsn,
                     const std::string& sourcePath);

    // Fast IEEE 802.3 CRC32 checksum helper
    static uint32_t computeCrc32(const void* data, size_t length, uint32_t initial = 0);
};

} // namespace gx

