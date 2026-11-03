#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <mutex>
#include <optional>

namespace gx::fileops {

enum class OpType : uint8_t {
    Copy = 1,
    Move = 2,
    Rename = 3,
    Delete = 4,
    Recycle = 5
};

enum class OpState : uint8_t {
    Planned = 1,
    InProgress = 2,
    Committed = 3,
    RolledBack = 4
};

struct JournalEntry {
    uint64_t opId{0};
    uint64_t timestamp{0};
    OpType type{OpType::Copy};
    OpState state{OpState::Planned};
    std::wstring sourcePath;
    std::wstring destPath;
    std::wstring tempPath; // e.g. .gxpartial path
    uint64_t bytesTransferred{0};
    uint64_t totalBytes{0};
};

struct RecoveryResult {
    size_t inProgressFound{0};
    size_t rolledBackCount{0};
    size_t tempFilesCleaned{0};
};

class OperationJournal {
public:
    explicit OperationJournal(std::wstring journalPath);
    ~OperationJournal();

    OperationJournal(const OperationJournal&) = delete;
    OperationJournal& operator=(const OperationJournal&) = delete;

    // Appends a new operation intent record
    bool logIntent(JournalEntry entry);

    // Updates the state of an existing record (e.g., InProgress, Committed, RolledBack)
    bool updateState(uint64_t opId, OpState newState);

    // Scans the journal on startup and cleans up partial or incomplete operations
    RecoveryResult recoverInterruptedOperations();

    [[nodiscard]] std::optional<JournalEntry> getEntry(uint64_t opId) const;
    [[nodiscard]] std::vector<JournalEntry> getAllEntries() const;
    [[nodiscard]] const std::wstring& journalPath() const noexcept { return journalPath_; }

    void clear();

private:
    std::wstring journalPath_;
    mutable std::mutex mutex_;
    std::vector<JournalEntry> inMemoryLog_;

    bool persistToDisk();
    bool loadFromDisk();
};

} // namespace gx::fileops

