#pragma once

#include "godxplr/content/text_sink.h"
#include <string>
#include <cstdint>

namespace gx::content {

enum class ExtractionStatus : uint8_t {
    Success,
    FileNotFound,
    AccessDenied,
    BinarySkipped,
    Truncated,
    IoError
};

class TextExtractor {
public:
    explicit TextExtractor(size_t maxBytesPerFile = 2 * 1024 * 1024)
        : maxBytes_(maxBytesPerFile) {}

    // Extracts text from a file into the provided sink
    ExtractionStatus extractFile(const std::wstring& filePath, ITextSink& sink);

    // Extracts text directly from memory buffer
    ExtractionStatus extractBuffer(const uint8_t* data, size_t size, ITextSink& sink);

    [[nodiscard]] size_t maxBytes() const noexcept { return maxBytes_; }

private:
    const size_t maxBytes_;
};

} // namespace gx::content

