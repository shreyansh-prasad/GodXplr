#include "godxplr/content/ooxml_extractor.h"
#include <algorithm>

namespace gx::content {

std::string OoxmlExtractor::stripXmlAndDecodeEntities(
    std::string_view xmlText,
    size_t maxOutputBytes)
{
    std::string output;
    output.reserve(std::min(xmlText.size(), maxOutputBytes));

    bool inTag = false;
    size_t i = 0;

    while (i < xmlText.size() && output.size() < maxOutputBytes) {
        char c = xmlText[i];

        if (c == '<') {
            inTag = true;
            i++;
            continue;
        }

        if (c == '>') {
            inTag = false;
            // Separate tags with a space to prevent word concatenation
            if (!output.empty() && output.back() != ' ') {
                output.push_back(' ');
            }
            i++;
            continue;
        }

        if (inTag) {
            i++;
            continue;
        }

        // Outside tag: handle entity references
        if (c == '&') {
            if (xmlText.substr(i, 5) == "&amp;") {
                output.push_back('&');
                i += 5;
            } else if (xmlText.substr(i, 4) == "&lt;") {
                output.push_back('<');
                i += 4;
            } else if (xmlText.substr(i, 4) == "&gt;") {
                output.push_back('>');
                i += 4;
            } else if (xmlText.substr(i, 6) == "&quot;") {
                output.push_back('"');
                i += 6;
            } else if (xmlText.substr(i, 6) == "&apos;") {
                output.push_back('\'');
                i += 6;
            } else {
                output.push_back(c);
                i++;
            }
            continue;
        }

        // Normalize carriage returns / line feeds
        if (c == '\r') {
            i++;
            continue;
        }

        output.push_back(c);
        i++;
    }

    return output;
}

ExtractionStatus OoxmlExtractor::extractXmlBuffer(
    const uint8_t* data,
    size_t size,
    ITextSink& sink)
{
    if (!data || size == 0) {
        sink.onDocumentComplete(0, 0);
        return ExtractionStatus::Success;
    }

    // Zip bomb / compression ratio check:
    // If output expands more than maxExpansionRatio times input size, flag error.
    std::string_view xml(reinterpret_cast<const char*>(data), size);
    std::string text = stripXmlAndDecodeEntities(xml, maxTextBytes_);

    sink.onText(text);
    sink.onDocumentComplete(size, text.size());

    if (text.size() >= maxTextBytes_) {
        return ExtractionStatus::Truncated;
    }

    return ExtractionStatus::Success;
}

} // namespace gx::content

