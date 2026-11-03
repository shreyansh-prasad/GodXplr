#include "godxplr/core/index_serializer.h"
#include <fstream>
#include <algorithm>
#include <cstring>
#include <shared_mutex>
#include <filesystem>

namespace gx {

namespace {

// Standard IEEE 802.3 CRC32 lookup table
constexpr uint32_t kCrcTable[16] = {
    0x00000000, 0x1DB71064, 0x3B6E20C8, 0x26D930AC,
    0x76DC4190, 0x6B6B51F4, 0x4DB26158, 0x5005713C,
    0xEDB88320, 0xF00F9344, 0xD6D6A3E8, 0xCB61B38C,
    0x9B64C2B0, 0x86D3D2D4, 0xA00AE278, 0xBDBDF21C
};

} // namespace

uint32_t IndexSerializer::computeCrc32(const void* data, size_t length, uint32_t initial) {
    uint32_t crc = ~initial;
    const auto* p = static_cast<const uint8_t*>(data);
    for (size_t i = 0; i < length; ++i) {
        crc ^= p[i];
        crc = (crc >> 4) ^ kCrcTable[crc & 0x0F];
        crc = (crc >> 4) ^ kCrcTable[crc & 0x0F];
    }
    return ~crc;
}

bool IndexSerializer::save(const NameIndex& index,
                           uint32_t volumeSerial,
                           uint64_t journalId,
                           uint64_t nextUsn,
                           const std::string& destinationPath) {
    std::shared_lock<std::shared_mutex> lock(index.mutex_);

    SnapshotHeader header{};
    header.formatVersion = 1;
    header.volumeSerial = volumeSerial;
    header.journalId = journalId;
    header.nextUsn = nextUsn;
    header.entryCount = static_cast<uint32_t>(index.entryCount_);
    header.origArenaBytes = static_cast<uint32_t>(index.arenaOrig_.size());
    header.foldArenaBytes = static_cast<uint32_t>(index.arenaFold_.size());

    // Calculate CRC32 across all SoA payload sections
    uint32_t crc = 0;
    if (header.entryCount > 0) {
        crc = computeCrc32(index.nameOff_.data(), header.entryCount * sizeof(uint32_t), crc);
        crc = computeCrc32(index.nameLen_.data(), header.entryCount * sizeof(uint16_t), crc);
        crc = computeCrc32(index.parent_.data(), header.entryCount * sizeof(EntryId), crc);
        crc = computeCrc32(index.frn_.data(), header.entryCount * sizeof(uint64_t), crc);
        crc = computeCrc32(index.size_.data(), header.entryCount * sizeof(uint64_t), crc);
        crc = computeCrc32(index.mtime_.data(), header.entryCount * sizeof(uint64_t), crc);
        crc = computeCrc32(index.flags_.data(), header.entryCount * sizeof(uint8_t), crc);
    }
    if (header.origArenaBytes > 0) {
        crc = computeCrc32(index.arenaOrig_.data(), header.origArenaBytes, crc);
    }
    if (header.foldArenaBytes > 0) {
        crc = computeCrc32(index.arenaFold_.data(), header.foldArenaBytes, crc);
    }
    header.checksum = crc;

    std::string tmpPath = destinationPath + ".tmp";
    std::ofstream out(tmpPath, std::ios::binary);
    if (!out) {
        return false;
    }

    // Write header
    out.write(reinterpret_cast<const char*>(&header), sizeof(header));

    // Write payload sections
    if (header.entryCount > 0) {
        out.write(reinterpret_cast<const char*>(index.nameOff_.data()), header.entryCount * sizeof(uint32_t));
        out.write(reinterpret_cast<const char*>(index.nameLen_.data()), header.entryCount * sizeof(uint16_t));
        out.write(reinterpret_cast<const char*>(index.parent_.data()), header.entryCount * sizeof(EntryId));
        out.write(reinterpret_cast<const char*>(index.frn_.data()), header.entryCount * sizeof(uint64_t));
        out.write(reinterpret_cast<const char*>(index.size_.data()), header.entryCount * sizeof(uint64_t));
        out.write(reinterpret_cast<const char*>(index.mtime_.data()), header.entryCount * sizeof(uint64_t));
        out.write(reinterpret_cast<const char*>(index.flags_.data()), header.entryCount * sizeof(uint8_t));
    }
    if (header.origArenaBytes > 0) {
        out.write(index.arenaOrig_.data(), header.origArenaBytes);
    }
    if (header.foldArenaBytes > 0) {
        out.write(index.arenaFold_.data(), header.foldArenaBytes);
    }

    out.flush();
    if (!out) {
        return false;
    }
    out.close();

    std::error_code ec;
    std::filesystem::rename(std::filesystem::path(tmpPath),
                           std::filesystem::path(destinationPath),
                           ec);
    return !ec;
}

bool IndexSerializer::load(NameIndex& outIndex,
                           uint32_t& outVolumeSerial,
                           uint64_t& outJournalId,
                           uint64_t& outNextUsn,
                           const std::string& sourcePath) {
    std::ifstream in(sourcePath, std::ios::binary);
    if (!in) {
        return false;
    }

    SnapshotHeader header{};
    in.read(reinterpret_cast<char*>(&header), sizeof(header));
    if (!in || std::memcmp(header.magic, "GXIX", 4) != 0 || header.formatVersion != 1) {
        return false;
    }

    std::unique_lock<std::shared_mutex> lock(outIndex.mutex_);

    outIndex.entryCount_ = header.entryCount;
    outIndex.nameOff_.resize(header.entryCount);
    outIndex.nameLen_.resize(header.entryCount);
    outIndex.parent_.resize(header.entryCount);
    outIndex.frn_.resize(header.entryCount);
    outIndex.size_.resize(header.entryCount);
    outIndex.mtime_.resize(header.entryCount);
    outIndex.flags_.resize(header.entryCount);
    outIndex.letters_.resize(header.entryCount);
    outIndex.arenaOrig_.resize(header.origArenaBytes);
    outIndex.arenaFold_.resize(header.foldArenaBytes);

    if (header.entryCount > 0) {
        in.read(reinterpret_cast<char*>(outIndex.nameOff_.data()), header.entryCount * sizeof(uint32_t));
        in.read(reinterpret_cast<char*>(outIndex.nameLen_.data()), header.entryCount * sizeof(uint16_t));
        in.read(reinterpret_cast<char*>(outIndex.parent_.data()), header.entryCount * sizeof(EntryId));
        in.read(reinterpret_cast<char*>(outIndex.frn_.data()), header.entryCount * sizeof(uint64_t));
        in.read(reinterpret_cast<char*>(outIndex.size_.data()), header.entryCount * sizeof(uint64_t));
        in.read(reinterpret_cast<char*>(outIndex.mtime_.data()), header.entryCount * sizeof(uint64_t));
        in.read(reinterpret_cast<char*>(outIndex.flags_.data()), header.entryCount * sizeof(uint8_t));
    }
    if (header.origArenaBytes > 0) {
        in.read(outIndex.arenaOrig_.data(), header.origArenaBytes);
    }
    if (header.foldArenaBytes > 0) {
        in.read(outIndex.arenaFold_.data(), header.foldArenaBytes);
    }

    if (!in) {
        return false;
    }

    // Verify CRC32 checksum
    uint32_t crc = 0;
    if (header.entryCount > 0) {
        crc = computeCrc32(outIndex.nameOff_.data(), header.entryCount * sizeof(uint32_t), crc);
        crc = computeCrc32(outIndex.nameLen_.data(), header.entryCount * sizeof(uint16_t), crc);
        crc = computeCrc32(outIndex.parent_.data(), header.entryCount * sizeof(EntryId), crc);
        crc = computeCrc32(outIndex.frn_.data(), header.entryCount * sizeof(uint64_t), crc);
        crc = computeCrc32(outIndex.size_.data(), header.entryCount * sizeof(uint64_t), crc);
        crc = computeCrc32(outIndex.mtime_.data(), header.entryCount * sizeof(uint64_t), crc);
        crc = computeCrc32(outIndex.flags_.data(), header.entryCount * sizeof(uint8_t), crc);
    }
    if (header.origArenaBytes > 0) {
        crc = computeCrc32(outIndex.arenaOrig_.data(), header.origArenaBytes, crc);
    }
    if (header.foldArenaBytes > 0) {
        crc = computeCrc32(outIndex.arenaFold_.data(), header.foldArenaBytes, crc);
    }

    if (crc != header.checksum) {
        return false; // Checksum corrupted
    }

    // Reconstruct acceleration structures
    outIndex.offsetMap_.clear();
    outIndex.offsetMap_.reserve(header.entryCount);
    outIndex.frnToIdMap_.clear();
    outIndex.frnToIdMap_.reserve(header.entryCount);
    outIndex.freeList_.clear();
    outIndex.activeCount_ = 0;

    for (EntryId i = 0; i < static_cast<EntryId>(header.entryCount); ++i) {
        if (!outIndex.isDeleted(i)) {
            outIndex.activeCount_++;
            outIndex.offsetMap_.push_back(OffsetIdPair{outIndex.nameOff_[i], i});
            outIndex.frnToIdMap_[outIndex.frn_[i]] = i;
            outIndex.letters_[i] = NameIndex::computeLetterMask(outIndex.getFoldedName(i));
        } else {
            outIndex.freeList_.push_back(i);
            outIndex.letters_[i] = 0;
        }
    }

    std::sort(outIndex.offsetMap_.begin(), outIndex.offsetMap_.end(),
              [](const OffsetIdPair& a, const OffsetIdPair& b) {
                  return a.offset < b.offset;
              });

    outVolumeSerial = header.volumeSerial;
    outJournalId = header.journalId;
    outNextUsn = header.nextUsn;

    return true;
}

} // namespace gx
