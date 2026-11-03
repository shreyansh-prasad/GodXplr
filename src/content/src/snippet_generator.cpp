#include "godxplr/content/snippet_generator.h"
#include <algorithm>
#include <cctype>

namespace gx::content {

std::string SnippetGenerator::generateSnippet(
    std::string_view text,
    size_t matchOffset,
    size_t matchLen,
    size_t snippetRadius)
{
    if (text.empty() || matchOffset >= text.size()) {
        return {};
    }

    size_t start = (matchOffset > snippetRadius) ? (matchOffset - snippetRadius) : 0;
    size_t end = std::min(text.size(), matchOffset + matchLen + snippetRadius);

    // Snap start forward to word boundary
    if (start > 0) {
        while (start < matchOffset && !std::isspace(static_cast<unsigned char>(text[start]))) {
            start++;
        }
        if (start < matchOffset && std::isspace(static_cast<unsigned char>(text[start]))) {
            start++;
        }
    }

    // Snap end backward to word boundary
    if (end < text.size()) {
        while (end > matchOffset + matchLen && !std::isspace(static_cast<unsigned char>(text[end - 1]))) {
            end--;
        }
    }

    std::string snippet;
    if (start > 0) {
        snippet += "... ";
    }

    // Replace newlines and tabs with spaces for uniform display
    for (size_t i = start; i < end; ++i) {
        char c = text[i];
        if (c == '\r' || c == '\n' || c == '\t') {
            snippet.push_back(' ');
        } else {
            snippet.push_back(c);
        }
    }

    if (end < text.size()) {
        snippet += " ...";
    }

    return snippet;
}

} // namespace gx::content
