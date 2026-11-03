#include "godxplr/ui/shell/preview_model.h"
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <unordered_set>

namespace gx::ui {

void PreviewModel::setUpdateCallback(UpdateCallback cb) {
    std::lock_guard<std::mutex> lock(mutex_);
    callback_ = std::move(cb);
}

void PreviewModel::loadEntry(
    const std::wstring& path,
    const EntryKey& key,
    uint64_t gen,
    platform::win32::StaWorkerThread* worker)
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        entryKey_ = key;
        filePath_ = path;
        generation_ = gen;
        previewImage_.reset();
        imageWidth_ = 0;
        imageHeight_ = 0;
        formattedDimensions_.clear();

        std::filesystem::path fsPath(path);
        fileName_ = fsPath.filename().wstring();

        std::string ext = fsPath.extension().string();
        if (!ext.empty() && ext.front() == '.') {
            ext.erase(0, 1);
        }
        std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        extension_ = ext;

#ifdef _WIN32
        WIN32_FILE_ATTRIBUTE_DATA attrData{};
        if (::GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &attrData)) {
            attributes_ = attrData.dwFileAttributes;
            isDirectory_ = (attributes_ & FILE_ATTRIBUTE_DIRECTORY) != 0;

            ULARGE_INTEGER ulSize{};
            ulSize.LowPart = attrData.nFileSizeLow;
            ulSize.HighPart = attrData.nFileSizeHigh;
            fileSize_ = ulSize.QuadPart;

            ULARGE_INTEGER ulCreated{};
            ulCreated.LowPart = attrData.ftCreationTime.dwLowDateTime;
            ulCreated.HighPart = attrData.ftCreationTime.dwHighDateTime;
            creationTime_ = ulCreated.QuadPart;

            ULARGE_INTEGER ulModified{};
            ulModified.LowPart = attrData.ftLastWriteTime.dwLowDateTime;
            ulModified.HighPart = attrData.ftLastWriteTime.dwHighDateTime;
            lastWriteTime_ = ulModified.QuadPart;

            ULARGE_INTEGER ulAccessed{};
            ulAccessed.LowPart = attrData.ftLastAccessTime.dwLowDateTime;
            ulAccessed.HighPart = attrData.ftLastAccessTime.dwHighDateTime;
            lastAccessTime_ = ulAccessed.QuadPart;
        } else {
            attributes_ = 0;
            isDirectory_ = false;
            fileSize_ = 0;
            creationTime_ = 0;
            lastWriteTime_ = 0;
            lastAccessTime_ = 0;
        }
#else
        attributes_ = 0;
        isDirectory_ = false;
        fileSize_ = 0;
        creationTime_ = 0;
        lastWriteTime_ = 0;
        lastAccessTime_ = 0;
#endif

        category_ = detectCategory(extension_, isDirectory_);
        formattedSize_ = formatFileSize(fileSize_);
        formattedCreated_ = formatFileTime(creationTime_);
        formattedModified_ = formatFileTime(lastWriteTime_);
        formattedAccessed_ = formatFileTime(lastAccessTime_);
        formattedAttributes_ = formatAttributes(attributes_);

        isLoading_.store(worker != nullptr, std::memory_order_release);
    }

    notifyUpdated();

#ifdef _WIN32
    if (worker) {
        worker->post([this, path, gen]() {
            if (generation_ != gen) {
                return; // Selection or navigation changed
            }

            auto extracted = platform::win32::ShellThumbnailProvider::extractThumbnail(
                path,
                512,
                512,
                platform::win32::ThumbnailExtractionFlags::Default);

            {
                std::lock_guard<std::mutex> lock(mutex_);
                if (generation_ != gen) {
                    return; // Dropped stale result
                }

                previewImage_ = extracted;
                if (previewImage_ && previewImage_->isValid()) {
                    imageWidth_ = previewImage_->width();
                    imageHeight_ = previewImage_->height();
                    formattedDimensions_ = std::to_wstring(imageWidth_) + L" \u00d7 " +
                                           std::to_wstring(imageHeight_) + L" px";
                }
                isLoading_.store(false, std::memory_order_release);
            }

            notifyUpdated();
        });
    }
#endif
}

void PreviewModel::clear() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        entryKey_ = EntryKey{};
        filePath_.clear();
        fileName_.clear();
        extension_.clear();
        category_.clear();
        isDirectory_ = false;
        fileSize_ = 0;
        formattedSize_.clear();
        creationTime_ = 0;
        lastWriteTime_ = 0;
        lastAccessTime_ = 0;
        formattedCreated_.clear();
        formattedModified_.clear();
        formattedAccessed_.clear();
        attributes_ = 0;
        formattedAttributes_.clear();
        imageWidth_ = 0;
        imageHeight_ = 0;
        formattedDimensions_.clear();
        isLoading_.store(false, std::memory_order_release);
        previewImage_.reset();
    }
    notifyUpdated();
}

void PreviewModel::notifyUpdated() {
    UpdateCallback cb;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        cb = callback_;
    }
    if (cb) {
        cb(*this);
    }
}

std::string PreviewModel::detectCategory(const std::string& ext, bool isDir) {
    if (isDir) return "Folder";
    if (ext.empty()) return "File";

    static const std::unordered_set<std::string> images{"png", "jpg", "jpeg", "bmp", "gif", "webp", "ico", "tiff", "svg"};
    static const std::unordered_set<std::string> videos{"mp4", "mkv", "avi", "mov", "wmv", "flv", "webm"};
    static const std::unordered_set<std::string> audio{"mp3", "wav", "flac", "aac", "ogg", "wma", "m4a"};
    static const std::unordered_set<std::string> docs{"pdf", "doc", "docx", "xls", "xlsx", "ppt", "pptx", "txt", "rtf", "csv", "md"};
    static const std::unordered_set<std::string> code{"cpp", "c", "h", "hpp", "py", "rs", "go", "js", "ts", "html", "css", "json", "xml", "yaml", "yml"};
    static const std::unordered_set<std::string> archives{"zip", "7z", "tar", "gz", "rar", "bz2", "xz"};
    static const std::unordered_set<std::string> apps{"exe", "dll", "msi", "sys", "bat", "cmd", "ps1"};

    if (images.contains(ext)) return "Image";
    if (videos.contains(ext)) return "Video";
    if (audio.contains(ext)) return "Audio";
    if (docs.contains(ext)) return "Document";
    if (code.contains(ext)) return "Code";
    if (archives.contains(ext)) return "Archive";
    if (apps.contains(ext)) return "Application";

    return "File";
}

std::wstring PreviewModel::formatFileSize(uint64_t bytes) {
    if (bytes == 0) {
        return L"0 bytes";
    }

    const double kb = static_cast<double>(bytes) / 1024.0;
    const double mb = kb / 1024.0;
    const double gb = mb / 1024.0;

    std::wostringstream oss;
    oss << std::fixed;

    if (bytes < 1024) {
        oss << bytes << L" bytes";
    } else if (kb < 1024.0) {
        oss << std::setprecision(1) << kb << L" KB (" << bytes << L" bytes)";
    } else if (mb < 1024.0) {
        oss << std::setprecision(1) << mb << L" MB (" << bytes << L" bytes)";
    } else {
        oss << std::setprecision(2) << gb << L" GB (" << bytes << L" bytes)";
    }

    return oss.str();
}

std::wstring PreviewModel::formatFileTime(uint64_t filetime) {
    if (filetime == 0) {
        return L"\u2014";
    }

#ifdef _WIN32
    FILETIME ft;
    ft.dwLowDateTime = static_cast<DWORD>(filetime & 0xFFFFFFFF);
    ft.dwHighDateTime = static_cast<DWORD>(filetime >> 32);

    FILETIME localFt;
    if (!::FileTimeToLocalFileTime(&ft, &localFt)) {
        return L"\u2014";
    }

    SYSTEMTIME st;
    if (!::FileTimeToSystemTime(&localFt, &st)) {
        return L"\u2014";
    }

    std::wostringstream oss;
    oss << std::setfill(L'0')
        << std::setw(4) << st.wYear << L'-'
        << std::setw(2) << st.wMonth << L'-'
        << std::setw(2) << st.wDay << L' '
        << std::setw(2) << st.wHour << L':'
        << std::setw(2) << st.wMinute << L':'
        << std::setw(2) << st.wSecond;
    return oss.str();
#else
    return L"\u2014";
#endif
}

std::wstring PreviewModel::formatAttributes(uint32_t attr) {
    if (attr == 0) {
        return L"Normal";
    }

    std::wstring result;
#ifdef _WIN32
    if (attr & FILE_ATTRIBUTE_READONLY) result += L'R';
    if (attr & FILE_ATTRIBUTE_HIDDEN) result += L'H';
    if (attr & FILE_ATTRIBUTE_SYSTEM) result += L'S';
    if (attr & FILE_ATTRIBUTE_ARCHIVE) result += L'A';
    if (attr & FILE_ATTRIBUTE_COMPRESSED) result += L'C';
    if (attr & FILE_ATTRIBUTE_ENCRYPTED) result += L'E';
#endif
    return result.empty() ? L"Normal" : result;
}

} // namespace gx::ui
