#pragma once

#include "godxplr/core/name_index.h"
#include "godxplr/core/query_ast.h"
#include "godxplr/core/search_types.h"
#include <memory>
#include <string_view>
#include <vector>

namespace gx {

class SearchEngine {
public:
    explicit SearchEngine(const NameIndex& index);
    ~SearchEngine() = default;

    // Executes a query string and returns a paginated ResultPage
    ResultPage search(std::string_view queryString, const QueryOptions& opts = {});

private:
    // Internal evaluation of AST nodes against candidate EntryIds
    void evaluateNode(const query::Node* node, std::vector<EntryId>& outCandidates);

    // Fast-path execution for single term/phrase
    void executeSimpleTerm(std::string_view term, std::vector<EntryId>& outCandidates);

    // Evaluates filter predicates on a candidate entry
    bool matchesFilters(EntryId id, const query::Node* filterNode);

    const NameIndex& index_;
};

} // namespace gx
