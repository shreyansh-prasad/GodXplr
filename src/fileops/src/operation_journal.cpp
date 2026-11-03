#include "godxplr/fileops/operation_journal.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <fstream>
#include <filesystem>

namespace gx::fileops {

static constexpr uint32_t kJournalMagic = 0x504F5847; // 'GXOP' in little-endian
static constexpr uint16_t kJournalVersion = 1;

OperationJournal::OperationJournal(std::wstring journalPath)
    : journalPath_(std::move(journalPath))
{
    loadFromDisk();
}

OperationJournal::~OperationJournal() {
    persistToDisk();
}

bool OperationJournal::logIntent(JournalEntry entry) {
    std::lock_guard<std::mutex> lock(mutex_);
    inMemoryLog_.push_back(std::move(entry));
    return persistToDisk();
}

bool OperationJournal::updateState(uint64_t opId, OpState newState) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& entry : inMemoryLog_) {
        if (entry.opId == opId) {
            entry.state = newState;
            return persistToDisk();
        }
    }
    return false;
}

RecoveryResult OperationJournal::recoverInterruptedOperations() {
    std::lock_guard<std::mutex> lock(mutex_);
    RecoveryResult res;

    bool modified = false;
    for (auto& entry : inMemoryLog_) {
        if (entry.state == OpState::InProgress) {
            res.inProgressFound++;

            // Clean up temporary .gxpartial file if left on disk
            if (!entry.tempPath.empty()) {
                DWORD attrs = GetFileAttributesW(entry.tempPath.c_str());
                if (attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY)) {
                    if (DeleteFileW(entry.tempPath.c_str())) {
                        res.tempFilesCleaned++;
                    }
                }
            }

            entry.state = OpState::RolledBack;
            res.rolledBackCount++;
            modified = true;
        }
    }

    if (modified) {
        persistToDisk();
    }

    return res;
}

std::optional<JournalEntry> OperationJournal::getEntry(uint64_t opId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& entry : inMemoryLog_) {
        if (entry.opId == opId) {
            return entry;
        }
    }
    return std::nullopt;
}

std::vector<JournalEntry> OperationJournal::getAllEntries() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return inMemoryLog_;
}

void OperationJournal::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    inMemoryLog_.clear();
    DeleteFileW(journalPath_.c_str());
}

bool OperationJournal::persistToDisk() {
    if (journalPath_.empty()) {
        return false;
    }

    // Ensure parent directory exists
    std::filesystem::path p(journalPath_);
    if (p.has_parent_path()) {
        std::error_code ec;
        std::filesystem::create_directories(p.parent_path(), ec);
    }

    std::ofstream out(journalPath_, std::ios::binary | std::ios::trunc);
    if (!out) {
        return false;
    }

    // Write header
    out.write(reinterpret_cast<const char*>(&kJournalMagic), sizeof(kJournalMagic));
    out.write(reinterpret_cast<const char*>(&kJournalVersion), sizeof(kJournalVersion));
    uint16_t reserved = 0;
    out.write(reinterpret_cast<const char*>(&reserved), sizeof(reserved));

    // Write entry count
    uint32_t count = static_cast<uint32_t>(inMemoryLog_.size());
    out.write(reinterpret_cast<const char*>(&count), sizeof(count));

    for (const auto& e : inMemoryLog_) {
        out.write(reinterpret_cast<const char*>(&e.opId), sizeof(e.opId));
        out.write(reinterpret_cast<const char*>(&e.timestamp), sizeof(e.timestamp));
        uint8_t opType = static_cast<uint8_t>(e.type);
        out.write(reinterpret_cast<const char*>(&opType), sizeof(opType));
        uint8_t opState = static_cast<uint8_t>(e.state);
        out.write(reinterpret_cast<const char*>(&opState), sizeof(opState));
        out.write(reinterpret_cast<const char*>(&e.bytesTransferred), sizeof(e.bytesTransferred));
        out.write(reinterpret_cast<const char*>(&e.totalBytes), sizeof(e.totalBytes));

        // UTF-16 source path
        uint32_t srcLen = static_cast<uint32_t>(e.sourcePath.size());
        out.write(reinterpret_cast<const char*>(&srcLen), sizeof(srcLen));
        if (srcLen > 0) {
            out.write(reinterpret_cast<const char*>(e.sourcePath.data()), srcLen * sizeof(wchar_t));
        }

        // UTF-16 dest path
        uint32_t dstLen = static_cast<uint32_t>(e.destPath.size());
        out.write(reinterpret_cast<const char*>(&dstLen), sizeof(dstLen));
        if (dstLen > 0) {
            out.write(reinterpret_cast<const char*>(e.destPath.data()), dstLen * sizeof(wchar_t));
        }

        // UTF-16 temp path
        uint32_t tmpLen = static_cast<uint32_t>(e.tempPath.size());
        out.write(reinterpret_cast<const char*>(&tmpLen), sizeof(tmpLen));
        if (tmpLen > 0) {
            out.write(reinterpret_cast<const char*>(e.tempPath.data()), tmpLen * sizeof(wchar_t));
        }
    }

    return out.good();
}

bool OperationJournal::loadFromDisk() {
    if (journalPath_.empty()) {
        return false;
    }

    std::ifstream in(journalPath_, std::ios::binary);
    if (!in) {
        return false;
    }

    uint32_t magic = 0;
    uint16_t version = 0;
    uint16_t reserved = 0;
    in.read(reinterpret_cast<char*>(&magic), sizeof(magic));
    in.read(reinterpret_cast<char*>(&version), sizeof(version));
    in.read(reinterpret_cast<char*>(&reserved), sizeof(reserved));

    if (magic != kJournalMagic || version != kJournalVersion) {
        return false;
    }

    uint32_t count = 0;
    in.read(reinterpret_cast<char*>(&count), sizeof(count));

    std::lock_guard<std::mutex> lock(mutex_);
    inMemoryLog_.clear();
    inMemoryLog_.reserve(count);

    for (uint32_t i = 0; i < count; ++i) {
        JournalEntry e;
        in.read(reinterpret_cast<char*>(&e.opId), sizeof(e.opId));
        in.read(reinterpret_cast<char*>(&e.timestamp), sizeof(e.timestamp));
        uint8_t opType = 0;
        in.read(reinterpret_cast<char*>(&opType), sizeof(opType));
        e.type = static_cast<OpType>(opType);
        uint8_t opState = 0;
        in.read(reinterpret_cast<char*>(&opState), sizeof(opState));
        e.state = static_cast<OpState>(opState);
        in.read(reinterpret_cast<char*>(&e.bytesTransferred), sizeof(e.bytesTransferred));
        in.read(reinterpret_cast<char*>(&e.totalBytes), sizeof(e.totalBytes));

        // Source path
        uint32_t srcLen = 0;
        in.read(reinterpret_cast<char*>(&srcLen), sizeof(srcLen));
        if (srcLen > 0) {
            e.sourcePath.resize(srcLen);
            in.read(reinterpret_cast<char*>(e.sourcePath.data()), srcLen * sizeof(wchar_t));
        }

        // Dest path
        uint32_t dstLen = 0;
        in.read(reinterpret_cast<char*>(&dstLen), sizeof(dstLen));
        if (dstLen > 0) {
            e.destPath.resize(dstLen);
            in.read(reinterpret_cast<char*>(e.destPath.data()), dstLen * sizeof(wchar_t));
        }

        // Temp path
        uint32_t tmpLen = 0;
        in.read(reinterpret_cast<char*>(&tmpLen), sizeof(tmpLen));
        if (tmpLen > 0) {
            e.tempPath.resize(tmpLen);
            in.read(reinterpret_cast<char*>(e.tempPath.data()), tmpLen * sizeof(wchar_t));
        }

        if (!in) {
            break;
        }

        inMemoryLog_.push_back(std::move(e));
    }

    return true;
}

} // namespace gx::fileops

