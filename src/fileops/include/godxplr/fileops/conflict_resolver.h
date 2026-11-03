#pragma once

#include <string>
#include <cstdint>

namespace gx::fileops {

enum class ConflictPolicy : uint8_t {
    AskUser,
    Replace,
    Skip,
    KeepBoth,
    ReplaceIfNewer,
    ReplaceIfDifferent
};

enum class ConflictAction : uint8_t {
    ExecuteReplace,
    ExecuteKeepBoth,
    SkipFile,
    PromptUser
};

class ConflictResolver {
public:
    // Checks if the destination file exists, querying its size and mtime
    static bool checkCollision(
        const std::wstring& targetPath,
        uint64_t& outFileSize,
        uint64_t& outMtime);

    // Auto-generates an unused unique filename (e.g. "Report (2).pdf")
    static std::wstring generateUniqueName(const std::wstring& targetPath);

    // Resolves what action to take given source, target, and policy
    static ConflictAction resolve(
        const std::wstring& sourcePath,
        const std::wstring& targetPath,
        ConflictPolicy policy,
        std::wstring& outResolvedDestPath);
};

} // namespace gx::fileops

