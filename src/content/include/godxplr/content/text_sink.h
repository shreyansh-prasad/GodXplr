#pragma once

#include <string>
#include <string_view>
#include <cstdint>
#include <algorithm>

namespace gx::content {

class ITextSink {
public:
    virtual ~ITextSink() = default;

    // Called when a chunk of extracted plain UTF-8 text is available
    virtual void onText(std::string_view utf8Chunk) = 0;

    // Called when extraction completes for the document
    virtual void onDocumentComplete(uint64_t totalFileBytes, uint64_t extractedTextBytes) = 0;
};

// Simple in-memory text accumulator sink
class StringTextSink : public ITextSink {
public:
    explicit StringTextSink(size_t maxBytes = 2 * 1024 * 1024)
        : maxBytes_(maxBytes) {}

    void onText(std::string_view utf8Chunk) override {
        if (text_.size() >= maxBytes_) {
            return;
        }
        size_t toAppend = std::min(utf8Chunk.size(), maxBytes_ - text_.size());
        text_.append(utf8Chunk.data(), toAppend);
    }

    void onDocumentComplete(uint64_t totalBytes, uint64_t extractedBytes) override {
        totalFileBytes_ = totalBytes;
        extractedTextBytes_ = extractedBytes;
    }

    [[nodiscard]] const std::string& text() const noexcept { return text_; }
    [[nodiscard]] uint64_t totalFileBytes() const noexcept { return totalFileBytes_; }
    [[nodiscard]] uint64_t extractedTextBytes() const noexcept { return extractedTextBytes_; }

    void clear() {
        text_.clear();
        totalFileBytes_ = 0;
        extractedTextBytes_ = 0;
    }

private:
    const size_t maxBytes_;
    std::string text_;
    uint64_t totalFileBytes_{0};
    uint64_t extractedTextBytes_{0};
};

} // namespace gx::content

