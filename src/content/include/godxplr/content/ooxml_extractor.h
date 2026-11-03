#pragma once

#include "godxplr/content/text_sink.h"
#include "godxplr/content/text_extractor.h"
#include <string>
#include <string_view>
#include <cstdint>

namespace gx::content {

class OoxmlExtractor {
public:
    explicit OoxmlExtractor(
        size_t maxTextBytes = 2 * 1024 * 1024,
        size_t maxExpansionRatio = 100)
        : maxTextBytes_(maxTextBytes), maxExpansionRatio_(maxExpansionRatio) {}

    // Extracts text from an XML or OOXML stream/buffer
    ExtractionStatus extractXmlBuffer(
        const uint8_t* data,
        size_t size,
        ITextSink& sink);

    // Strips XML tags, decodes XML entities, and outputs plain text
    static std::string stripXmlAndDecodeEntities(
        std::string_view xmlText,
        size_t maxOutputBytes = 2 * 1024 * 1024);

    [[nodiscard]] size_t maxTextBytes() const noexcept { return maxTextBytes_; }
    [[nodiscard]] size_t maxExpansionRatio() const noexcept { return maxExpansionRatio_; }

private:
    const size_t maxTextBytes_;
    const size_t maxExpansionRatio_;
};

} // namespace gx::content

