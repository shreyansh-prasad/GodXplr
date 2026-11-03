#pragma once

#include <string>
#include <string_view>

namespace gx::content {

class SnippetGenerator {
public:
    // Generates a context snippet around the match offset with word-boundary snapping
    static std::string generateSnippet(
        std::string_view text,
        size_t matchOffset,
        size_t matchLen,
        size_t snippetRadius = 80);
};

} // namespace gx::content

