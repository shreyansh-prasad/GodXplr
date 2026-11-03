#pragma once

#include "godxplr/core/entry_key.h"
#include "godxplr/core/pixel_buffer.h"
#include "godxplr/platform/com_apartment.h"
#include "godxplr/platform/shell_thumbnail_provider.h"

#include <string>
#include <cstdint>
#include <optional>
#include <functional>
#include <atomic>
#include <mutex>

namespace gx::ui {

class PreviewModel {
public:
    using UpdateCallback = std::function<void(const PreviewModel&)>;

    PreviewModel() = default;
    ~PreviewModel() = default;

    PreviewModel(const PreviewModel&) = delete;
    PreviewModel& operator=(const PreviewModel&) = delete;

    void setUpdateCallback(UpdateCallback cb);

    // Asynchronously loads inspector metadata and high-resolution thumbnail preview
    void loadEntry(
        const std::wstring& path,
        const EntryKey& key,
        uint64_t generation,
        platform::win32::StaWorkerThread* worker = nullptr);

    void clear();

    [[nodiscard]] bool hasEntry() const noexcept { return !filePath_.empty(); }
    [[nodiscard]] const EntryKey& entryKey() const noexcept { return entryKey_; }
    [[nodiscard]] const std::wstring& filePath() const noexcept { return filePath_; }
    [[nodiscard]] const std::wstring& fileName() const noexcept { return fileName_; }
    [[nodiscard]] const std::string& extension() const noexcept { return extension_; }
    [[nodiscard]] const std::string& category() const noexcept { return category_; }

    [[nodiscard]] bool isDirectory() const noexcept { return isDirectory_; }
    [[nodiscard]] uint64_t fileSize() const noexcept { return fileSize_; }
    [[nodiscard]] const std::wstring& formattedSize() const noexcept { return formattedSize_; }

    [[nodiscard]] uint64_t creationTime() const noexcept { return creationTime_; }
    [[nodiscard]] uint64_t lastWriteTime() const noexcept { return lastWriteTime_; }
    [[nodiscard]] uint64_t lastAccessTime() const noexcept { return lastAccessTime_; }
    [[nodiscard]] const std::wstring& formattedCreated() const noexcept { return formattedCreated_; }
    [[nodiscard]] const std::wstring& formattedModified() const noexcept { return formattedModified_; }
    [[nodiscard]] const std::wstring& formattedAccessed() const noexcept { return formattedAccessed_; }

    [[nodiscard]] uint32_t attributes() const noexcept { return attributes_; }
    [[nodiscard]] const std::wstring& formattedAttributes() const noexcept { return formattedAttributes_; }

    [[nodiscard]] uint32_t imageWidth() const noexcept { return imageWidth_; }
    [[nodiscard]] uint32_t imageHeight() const noexcept { return imageHeight_; }
    [[nodiscard]] const std::wstring& formattedDimensions() const noexcept { return formattedDimensions_; }

    [[nodiscard]] bool isLoading() const noexcept { return isLoading_.load(std::memory_order_relaxed); }
    [[nodiscard]] uint64_t generation() const noexcept { return generation_; }
    [[nodiscard]] const std::optional<core::PixelBuffer>& previewImage() const noexcept { return previewImage_; }

    // Pure helpers for formatting and categorization
    static std::string detectCategory(const std::string& ext, bool isDir);
    static std::wstring formatFileSize(uint64_t bytes);
    static std::wstring formatFileTime(uint64_t filetime);
    static std::wstring formatAttributes(uint32_t attr);

private:
    void notifyUpdated();

    mutable std::mutex mutex_;
    UpdateCallback callback_;

    EntryKey entryKey_{};
    std::wstring filePath_;
    std::wstring fileName_;
    std::string extension_;
    std::string category_;

    bool isDirectory_{false};
    uint64_t fileSize_{0};
    std::wstring formattedSize_;

    uint64_t creationTime_{0};
    uint64_t lastWriteTime_{0};
    uint64_t lastAccessTime_{0};
    std::wstring formattedCreated_;
    std::wstring formattedModified_;
    std::wstring formattedAccessed_;

    uint32_t attributes_{0};
    std::wstring formattedAttributes_;

    uint32_t imageWidth_{0};
    uint32_t imageHeight_{0};
    std::wstring formattedDimensions_;

    std::atomic<bool> isLoading_{false};
    uint64_t generation_{0};
    std::optional<core::PixelBuffer> previewImage_;
};

} // namespace gx::ui

