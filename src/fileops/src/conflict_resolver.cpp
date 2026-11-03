#include "godxplr/fileops/conflict_resolver.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <filesystem>

namespace gx::fileops {

bool ConflictResolver::checkCollision(
    const std::wstring& targetPath,
    uint64_t& outFileSize,
    uint64_t& outMtime)
{
    WIN32_FILE_ATTRIBUTE_DATA fad{};
    if (GetFileAttributesExW(targetPath.c_str(), GetFileExInfoStandard, &fad)) {
        outFileSize = (static_cast<uint64_t>(fad.nFileSizeHigh) << 32) | fad.nFileSizeLow;
        outMtime = (static_cast<uint64_t>(fad.ftLastWriteTime.dwHighDateTime) << 32) | fad.ftLastWriteTime.dwLowDateTime;
        return true;
    }
    return false;
}

std::wstring ConflictResolver::generateUniqueName(const std::wstring& targetPath) {
    std::filesystem::path p(targetPath);
    auto parent = p.parent_path();
    auto stem = p.stem().wstring();
    auto ext = p.extension().wstring();

    for (int i = 2; i < 10000; ++i) {
        std::wstring candidateName = stem + L" (" + std::to_wstring(i) + L")" + ext;
        std::filesystem::path candidatePath = parent.empty() ? std::filesystem::path(candidateName) : (parent / candidateName);

        DWORD attrs = GetFileAttributesW(candidatePath.c_str());
        if (attrs == INVALID_FILE_ATTRIBUTES) {
            return candidatePath.wstring();
        }
    }

    return targetPath;
}

ConflictAction ConflictResolver::resolve(
    const std::wstring& sourcePath,
    const std::wstring& targetPath,
    ConflictPolicy policy,
    std::wstring& outResolvedDestPath)
{
    uint64_t dstSize = 0;
    uint64_t dstMtime = 0;
    if (!checkCollision(targetPath, dstSize, dstMtime)) {
        outResolvedDestPath = targetPath;
        return ConflictAction::ExecuteReplace;
    }

    switch (policy) {
    case ConflictPolicy::Skip:
        return ConflictAction::SkipFile;

    case ConflictPolicy::Replace:
        outResolvedDestPath = targetPath;
        return ConflictAction::ExecuteReplace;

    case ConflictPolicy::KeepBoth:
        outResolvedDestPath = generateUniqueName(targetPath);
        return ConflictAction::ExecuteKeepBoth;

    case ConflictPolicy::ReplaceIfNewer: {
        uint64_t srcSize = 0;
        uint64_t srcMtime = 0;
        if (checkCollision(sourcePath, srcSize, srcMtime) && srcMtime > dstMtime) {
            outResolvedDestPath = targetPath;
            return ConflictAction::ExecuteReplace;
        }
        return ConflictAction::SkipFile;
    }

    case ConflictPolicy::ReplaceIfDifferent: {
        uint64_t srcSize = 0;
        uint64_t srcMtime = 0;
        if (checkCollision(sourcePath, srcSize, srcMtime) && (srcSize != dstSize || srcMtime != dstMtime)) {
            outResolvedDestPath = targetPath;
            return ConflictAction::ExecuteReplace;
        }
        return ConflictAction::SkipFile;
    }

    case ConflictPolicy::AskUser:
    default:
        return ConflictAction::PromptUser;
    }
}

} // namespace gx::fileops

