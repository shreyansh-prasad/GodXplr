#include "godxplr/fileops/fileops_coordinator.h"
#include <filesystem>

namespace gx::fileops {

FileOpsCoordinator::FileOpsCoordinator(std::shared_ptr<OperationJournal> journal)
    : journal_(std::move(journal))
{
    resumeEvent_ = CreateEventW(nullptr, TRUE, TRUE, nullptr); // Manual reset, initially signaled
}

FileOpsCoordinator::~FileOpsCoordinator() {
    if (resumeEvent_) {
        CloseHandle(resumeEvent_);
        resumeEvent_ = nullptr;
    }
}

FileOpResult FileOpsCoordinator::copyFile(
    const std::wstring& source,
    const std::wstring& dest,
    ConflictPolicy policy)
{
    isCancelled_.store(false);

    std::wstring resolvedDest;
    ConflictAction action = ConflictResolver::resolve(source, dest, policy, resolvedDest);

    if (action == ConflictAction::SkipFile) {
        return FileOpResult::Success;
    }

    uint64_t opId = nextOpId_++;
    uint64_t srcSize = 0, srcMtime = 0;
    ConflictResolver::checkCollision(source, srcSize, srcMtime);

    if (journal_) {
        JournalEntry entry;
        entry.opId = opId;
        entry.type = OpType::Copy;
        entry.state = OpState::InProgress;
        entry.sourcePath = source;
        entry.destPath = resolvedDest;
        entry.tempPath = resolvedDest + L".gxpartial";
        entry.totalBytes = srcSize;
        journal_->logIntent(std::move(entry));
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        progress_.totalBytes += srcSize;
        progress_.totalFiles += 1;
        progress_.currentFile = source;
        progress_.status = OperationStatus::Running;
    }

    auto onProgress = [this](uint64_t bytesWritten, uint64_t /*total*/, uint64_t /*size*/) {
        std::lock_guard<std::mutex> lock(mutex_);
        progress_.completedBytes += bytesWritten;
    };

    FileOpResult res = StreamingFileExecutor::streamingCopy(
        source,
        resolvedDest,
        &isCancelled_,
        resumeEvent_,
        onProgress);

    if (res == FileOpResult::Success) {
        if (journal_) {
            journal_->updateState(opId, OpState::Committed);
        }

        std::lock_guard<std::mutex> lock(mutex_);
        progress_.completedFiles += 1;
        progress_.status = OperationStatus::Completed;

        UndoItem undoItem;
        undoItem.opId = opId;
        undoItem.type = OpType::Copy;
        undoItem.sourcePath = source;
        undoItem.destPath = resolvedDest;
        undoItem.fileSize = srcSize;
        undoItem.mtime = srcMtime;

        undoStack_.push_back(std::move(undoItem));
        redoStack_.clear();
    } else {
        if (journal_) {
            journal_->updateState(opId, OpState::RolledBack);
        }
        std::lock_guard<std::mutex> lock(mutex_);
        progress_.status = (res == FileOpResult::Cancelled) ? OperationStatus::Cancelled : OperationStatus::Failed;
    }

    return res;
}

FileOpResult FileOpsCoordinator::moveFile(
    const std::wstring& source,
    const std::wstring& dest,
    ConflictPolicy policy)
{
    isCancelled_.store(false);

    std::wstring resolvedDest;
    ConflictAction action = ConflictResolver::resolve(source, dest, policy, resolvedDest);

    if (action == ConflictAction::SkipFile) {
        return FileOpResult::Success;
    }

    uint64_t opId = nextOpId_++;
    uint64_t srcSize = 0, srcMtime = 0;
    ConflictResolver::checkCollision(source, srcSize, srcMtime);

    if (journal_) {
        JournalEntry entry;
        entry.opId = opId;
        entry.type = OpType::Move;
        entry.state = OpState::InProgress;
        entry.sourcePath = source;
        entry.destPath = resolvedDest;
        entry.tempPath = resolvedDest + L".gxpartial";
        entry.totalBytes = srcSize;
        journal_->logIntent(std::move(entry));
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        progress_.totalBytes += srcSize;
        progress_.totalFiles += 1;
        progress_.currentFile = source;
        progress_.status = OperationStatus::Running;
    }

    FileOpResult res = FileOpResult::Success;

    if (StreamingFileExecutor::isSameVolume(source, resolvedDest)) {
        res = StreamingFileExecutor::atomicSameVolumeMove(source, resolvedDest, true);
        if (res == FileOpResult::Success) {
            std::lock_guard<std::mutex> lock(mutex_);
            progress_.completedBytes += srcSize;
        }
    } else {
        auto onProgress = [this](uint64_t bytesWritten, uint64_t /*total*/, uint64_t /*size*/) {
            std::lock_guard<std::mutex> lock(mutex_);
            progress_.completedBytes += bytesWritten;
        };

        res = StreamingFileExecutor::crossVolumeMove(
            source,
            resolvedDest,
            &isCancelled_,
            resumeEvent_,
            onProgress);
    }

    if (res == FileOpResult::Success) {
        if (journal_) {
            journal_->updateState(opId, OpState::Committed);
        }

        std::lock_guard<std::mutex> lock(mutex_);
        progress_.completedFiles += 1;
        progress_.status = OperationStatus::Completed;

        UndoItem undoItem;
        undoItem.opId = opId;
        undoItem.type = OpType::Move;
        undoItem.sourcePath = source;
        undoItem.destPath = resolvedDest;
        undoItem.fileSize = srcSize;
        undoItem.mtime = srcMtime;

        undoStack_.push_back(std::move(undoItem));
        redoStack_.clear();
    } else {
        if (journal_) {
            journal_->updateState(opId, OpState::RolledBack);
        }
        std::lock_guard<std::mutex> lock(mutex_);
        progress_.status = (res == FileOpResult::Cancelled) ? OperationStatus::Cancelled : OperationStatus::Failed;
    }

    return res;
}

FileOpResult FileOpsCoordinator::renameFile(
    const std::wstring& source,
    const std::wstring& newName)
{
    std::filesystem::path p(source);
    std::wstring targetPath = (p.parent_path() / newName).wstring();

    return moveFile(source, targetPath, ConflictPolicy::Replace);
}

FileOpResult FileOpsCoordinator::deleteFilePermanent(const std::wstring& targetPath) {
    SetFileAttributesW(targetPath.c_str(), FILE_ATTRIBUTE_NORMAL);
    if (!DeleteFileW(targetPath.c_str())) {
        DWORD err = GetLastError();
        if (err == ERROR_FILE_NOT_FOUND) {
            return FileOpResult::SourceNotFound;
        }
        if (err == ERROR_ACCESS_DENIED) {
            return FileOpResult::AccessDenied;
        }
        return FileOpResult::FailedDeleteSource;
    }
    return FileOpResult::Success;
}

OperationProgress FileOpsCoordinator::progress() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return progress_;
}

void FileOpsCoordinator::pause() {
    if (resumeEvent_) {
        ResetEvent(resumeEvent_);
    }
    std::lock_guard<std::mutex> lock(mutex_);
    progress_.status = OperationStatus::Paused;
}

void FileOpsCoordinator::resume() {
    if (resumeEvent_) {
        SetEvent(resumeEvent_);
    }
    std::lock_guard<std::mutex> lock(mutex_);
    progress_.status = OperationStatus::Running;
}

void FileOpsCoordinator::cancel() {
    isCancelled_.store(true);
    if (resumeEvent_) {
        SetEvent(resumeEvent_); // Unblock in case thread is paused
    }
}

void FileOpsCoordinator::resetProgress() {
    std::lock_guard<std::mutex> lock(mutex_);
    progress_ = OperationProgress{};
}

bool FileOpsCoordinator::canUndo() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return !undoStack_.empty();
}

bool FileOpsCoordinator::canRedo() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return !redoStack_.empty();
}

FileOpResult FileOpsCoordinator::undo() {
    std::unique_lock<std::mutex> lock(mutex_);
    if (undoStack_.empty()) {
        return FileOpResult::Success;
    }

    UndoItem item = std::move(undoStack_.back());
    undoStack_.pop_back();
    lock.unlock();

    FileOpResult res = FileOpResult::Success;
    if (item.type == OpType::Copy) {
        // Undo copy: remove the copied file
        SetFileAttributesW(item.destPath.c_str(), FILE_ATTRIBUTE_NORMAL);
        if (DeleteFileW(item.destPath.c_str())) {
            std::lock_guard<std::mutex> lk(mutex_);
            redoStack_.push_back(std::move(item));
        } else {
            res = FileOpResult::FailedDeleteSource;
        }
    } else if (item.type == OpType::Move || item.type == OpType::Rename) {
        // Undo move: move back dest -> source
        res = StreamingFileExecutor::atomicSameVolumeMove(item.destPath, item.sourcePath, true);
        if (res == FileOpResult::Success) {
            std::lock_guard<std::mutex> lk(mutex_);
            redoStack_.push_back(std::move(item));
        }
    }

    return res;
}

FileOpResult FileOpsCoordinator::redo() {
    std::unique_lock<std::mutex> lock(mutex_);
    if (redoStack_.empty()) {
        return FileOpResult::Success;
    }

    UndoItem item = std::move(redoStack_.back());
    redoStack_.pop_back();
    lock.unlock();

    FileOpResult res = FileOpResult::Success;
    if (item.type == OpType::Copy) {
        // Redo copy: re-copy source -> dest
        res = StreamingFileExecutor::streamingCopy(item.sourcePath, item.destPath);
        if (res == FileOpResult::Success) {
            std::lock_guard<std::mutex> lk(mutex_);
            undoStack_.push_back(std::move(item));
        }
    } else if (item.type == OpType::Move || item.type == OpType::Rename) {
        // Redo move: move source -> dest
        res = StreamingFileExecutor::atomicSameVolumeMove(item.sourcePath, item.destPath, true);
        if (res == FileOpResult::Success) {
            std::lock_guard<std::mutex> lk(mutex_);
            undoStack_.push_back(std::move(item));
        }
    }

    return res;
}

} // namespace gx::fileops

