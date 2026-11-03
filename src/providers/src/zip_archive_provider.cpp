#include "godxplr/providers/zip_archive_provider.h"
#include <filesystem>
#include <algorithm>

namespace gx::providers {

ZipArchiveProvider::~ZipArchiveProvider() {
    close();
}

void ZipArchiveProvider::close() {
    if (file_.is_open()) {
        file_.close();
    }
    items_.clear();
    localOffsets_.clear();
    archivePath_.clear();
}

bool ZipArchiveProvider::open(const std::wstring& containerPath) {
    close();
    archivePath_ = containerPath;

    file_.open(archivePath_, std::ios::binary);
    if (!file_) {
        return false;
    }

    if (!parseCentralDirectory()) {
        close();
        return false;
    }

    return true;
}

bool ZipArchiveProvider::parseCentralDirectory() {
    file_.seekg(0, std::ios::end);
    std::streamoff fileSize = file_.tellg();
    if (fileSize < sizeof(ZipEndOfCentralDirectory)) {
        return false;
    }

    // Search backward up to 64KB for End Of Central Directory record
    const size_t searchLen = std::min<size_t>(static_cast<size_t>(fileSize), 65536);
    std::vector<char> searchBuf(searchLen);

    file_.seekg(fileSize - searchLen, std::ios::beg);
    file_.read(searchBuf.data(), searchLen);

    std::streamoff eocdPos = -1;
    for (size_t i = searchLen - sizeof(ZipEndOfCentralDirectory); i > 0; --i) {
        uint32_t sig = *reinterpret_cast<const uint32_t*>(&searchBuf[i]);
        if (sig == 0x06054b50) {
            eocdPos = (fileSize - searchLen) + static_cast<std::streamoff>(i);
            break;
        }
    }

    if (eocdPos == -1) {
        return false;
    }

    file_.seekg(eocdPos, std::ios::beg);
    ZipEndOfCentralDirectory eocd{};
    file_.read(reinterpret_cast<char*>(&eocd), sizeof(eocd));

    if (eocd.signature != 0x06054b50 || eocd.totalEntries == 0) {
        return false;
    }

    // Seek to central directory offset
    file_.seekg(eocd.centralDirOffset, std::ios::beg);

    for (uint16_t i = 0; i < eocd.totalEntries; ++i) {
        ZipCentralDirectoryHeader cdh{};
        file_.read(reinterpret_cast<char*>(&cdh), sizeof(cdh));
        if (cdh.signature != 0x02014b50) {
            break;
        }

        std::string rawName(cdh.filenameLength, '\0');
        file_.read(rawName.data(), cdh.filenameLength);

        // Skip extra field and comment
        if (cdh.extraFieldLength > 0) file_.seekg(cdh.extraFieldLength, std::ios::cur);
        if (cdh.commentLength > 0) file_.seekg(cdh.commentLength, std::ios::cur);

        // Security check: reject malicious path traversal attacks
        if (!PathSanitizer::isSafePath(rawName)) {
            continue;
        }

        std::wstring sanitizedRel = PathSanitizer::sanitizeRelativePath(rawName);
        if (sanitizedRel.empty()) {
            continue;
        }

        VirtualItem item;
        item.relativePath = sanitizedRel;
        std::filesystem::path p(sanitizedRel);
        item.name = p.filename().wstring();
        item.uncompressedSize = cdh.uncompressedSize;
        item.compressedSize = cdh.compressedSize;
        item.crc32 = cdh.crc32;
        item.isEncrypted = (cdh.generalFlags & 1) != 0;
        item.isDirectory = rawName.ends_with('/') || (cdh.externalAttrs & 0x10) != 0;

        localOffsets_[sanitizedRel] = cdh.localHeaderOffset;
        items_.push_back(std::move(item));
    }

    return !items_.empty();
}

std::vector<VirtualItem> ZipArchiveProvider::enumerateDirectory(const std::wstring& virtualSubDir) {
    std::vector<VirtualItem> results;

    std::wstring normalizedDir = virtualSubDir;
    while (!normalizedDir.empty() && normalizedDir.back() == L'\\') {
        normalizedDir.pop_back();
    }

    for (const auto& it : items_) {
        std::filesystem::path itemPath(it.relativePath);
        auto parentPath = itemPath.parent_path().wstring();

        if (parentPath == normalizedDir) {
            results.push_back(it);
        }
    }

    return results;
}

bool ZipArchiveProvider::extractItem(const std::wstring& relativePath, const std::wstring& destDiskPath) {
    auto it = localOffsets_.find(relativePath);
    if (it == localOffsets_.end()) {
        return false;
    }

    file_.seekg(it->second, std::ios::beg);
    ZipLocalHeader lh{};
    file_.read(reinterpret_cast<char*>(&lh), sizeof(lh));

    if (lh.signature != 0x04034b50) {
        return false;
    }

    // Skip local filename and extra fields
    file_.seekg(lh.filenameLength + lh.extraFieldLength, std::ios::cur);

    // Decompression bomb guard
    if (lh.uncompressedSize > 500 * 1024 * 1024) { // 500 MB absolute limit
        return false;
    }
    if (lh.compressedSize > 0 && (lh.uncompressedSize / lh.compressedSize > 100)) { // 100:1 ratio cap
        return false;
    }

    std::filesystem::path outPath(destDiskPath);
    if (outPath.has_parent_path()) {
        std::error_code ec;
        std::filesystem::create_directories(outPath.parent_path(), ec);
    }

    std::ofstream out(destDiskPath, std::ios::binary | std::ios::trunc);
    if (!out) {
        return false;
    }

    // For Method 0 (Stored) or raw stream extract
    std::vector<char> buffer(64 * 1024);
    uint32_t remaining = lh.compressedSize;

    while (remaining > 0) {
        uint32_t toRead = std::min<uint32_t>(remaining, static_cast<uint32_t>(buffer.size()));
        file_.read(buffer.data(), toRead);
        out.write(buffer.data(), toRead);
        remaining -= toRead;
    }

    return out.good();
}

} // namespace gx::providers
