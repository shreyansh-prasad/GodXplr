#pragma once

#include "godxplr/core/query_ast.h"
#include <string_view>

namespace gx::query {

class Parser {
public:
    // Parses an Everything-compatible search query string into an AST
    static NodePtr parse(std::string_view query);

    // Normalizes AST: flattens AND/OR, pushes NOT down via De Morgan's laws
    static NodePtr normalize(NodePtr root);

    // Formats AST back to a canonical string for debugging and cache hashing
    static std::string dump(const Node* node);
};

} // namespace gx::query

