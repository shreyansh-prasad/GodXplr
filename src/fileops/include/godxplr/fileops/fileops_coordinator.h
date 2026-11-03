#pragma once

#include "godxplr/fileops/operation_journal.h"
#include "godxplr/fileops/conflict_resolver.h"
#include "godxplr/fileops/streaming_file_executor.h"

#include <string>
#include <vector>
#include <atomic>
#include <mutex>
#include <memory>
#include <optional>

namespace gx::fileops {

enum class OperationStatus : uint8_t {
    Idle,
    Running,
    Paused,
    Completed,
    Cancelled,
    Failed
};

struct OperationProgress {
    uint64_t totalBytes{0};
    uint64_t completedBytes{0};
    uint64_t totalFiles{0};
    uint64_t completedFiles{0};
    std::wstring currentFile;
    OperationStatus status{OperationStatus::Idle};
};

struct UndoItem {
    uint64_t opId{0};
    OpType type{OpType::Copy};
    std::wstring sourcePath;
    std::wstring destPath;
    uint64_t fileSize{0};
    uint64_t mtime{0};
};

class FileOpsCoordinator {
public:
    explicit FileOpsCoordinator(std::shared_ptr<OperationJournal> journal = nullptr);
    ~FileOpsCoordinator();

    FileOpsCoordinator(const FileOpsCoordinator&) = delete;
    FileOpsCoordinator& operator=(const FileOpsCoordinator&) = delete;

    // Executes a copy operation on a file
    FileOpResult copyFile(
        const std::wstring& source,
        const std::wstring& dest,
        ConflictPolicy policy = ConflictPolicy::Replace);

    // Executes a move operation on a file (same-volume atomic or cross-volume copy-verify-delete)
    FileOpResult moveFile(
        const std::wstring& source,
        const std::wstring& dest,
        ConflictPolicy policy = ConflictPolicy::Replace);

    // Executes a rename operation on a file
    FileOpResult renameFile(
        const std::wstring& source,
        const std::wstring& newName);

    // Executes a file deletion (permanent)
    FileOpResult deleteFilePermanent(const std::wstring& targetPath);

    // Progress, Pause, Resume, Cancel controls
    [[nodiscard]] OperationProgress progress() const;
    void pause();
    void resume();
    void cancel();
    void resetProgress();

    // LIFO Undo / Redo
    [[nodiscard]] bool canUndo() const;
    [[nodiscard]] bool canRedo() const;
    FileOpResult undo();
    FileOpResult redo();

    [[nodiscard]] std::shared_ptr<OperationJournal> journal() const noexcept { return journal_; }

private:
    std::shared_ptr<OperationJournal> journal_;
    mutable std::mutex mutex_;

    OperationProgress progress_;
    std::atomic<bool> isCancelled_{false};
    HANDLE resumeEvent_{nullptr};

    std::vector<UndoItem> undoStack_;
    std::vector<UndoItem> redoStack_;

    uint64_t nextOpId_{1};
};

} // namespace gx::fileops

