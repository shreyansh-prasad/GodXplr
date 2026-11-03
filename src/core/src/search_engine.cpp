#include "godxplr/core/search_engine.h"
#include "godxplr/core/query_parser.h"
#include "godxplr/core/simd_search.h"
#include <algorithm>

namespace gx {

SearchEngine::SearchEngine(const NameIndex& index) : index_(index) {}

bool SearchEngine::matchesFilters(EntryId id, const query::Node* node) {
    if (!node) return true;

    switch (node->type) {
        case query::NodeType::Term: {
            const auto* t = static_cast<const query::TermNode*>(node);
            std::string_view name = index_.getFoldedName(id);
            std::string needle = NameIndex::foldCase(t->text);
            return name.find(needle) != std::string_view::npos;
        }
        case query::NodeType::Phrase: {
            const auto* p = static_cast<const query::PhraseNode*>(node);
            std::string_view name = index_.getFoldedName(id);
            std::string needle = NameIndex::foldCase(p->phrase);
            return name.find(needle) != std::string_view::npos;
        }
        case query::NodeType::Ext: {
            const auto* e = static_cast<const query::ExtNode*>(node);
            std::string_view name = index_.getFoldedName(id);
            size_t dot = name.rfind('.');
            if (dot == std::string_view::npos || dot + 1 >= name.size()) {
                return false;
            }
            std::string_view ext = name.substr(dot + 1);
            for (const auto& targetExt : e->extensions) {
                if (ext == targetExt) {
                    return true;
                }
            }
            return false;
        }
        case query::NodeType::Size: {
            const auto* s = static_cast<const query::SizeNode*>(node);
            uint64_t sz = index_.getFileSize(id);
            switch (s->op) {
                case query::SizeOp::GreaterThan: return sz > s->minBytes;
                case query::SizeOp::LessThan:    return sz < s->minBytes;
                case query::SizeOp::Equal:       return sz == s->minBytes;
                case query::SizeOp::Range:       return sz >= s->minBytes && sz <= s->maxBytes;
            }
            return true;
        }
        case query::NodeType::FlagFile:
            return !index_.isDirectory(id);
        case query::NodeType::FlagFolder:
            return index_.isDirectory(id);
        case query::NodeType::And: {
            const auto* a = static_cast<const query::MultiNode*>(node);
            for (const auto& ch : a->children) {
                if (!matchesFilters(id, ch.get())) return false;
            }
            return true;
        }
        case query::NodeType::Or: {
            const auto* o = static_cast<const query::MultiNode*>(node);
            for (const auto& ch : o->children) {
                if (matchesFilters(id, ch.get())) return true;
            }
            return false;
        }
        case query::NodeType::Not: {
            const auto* n = static_cast<const query::UnaryNode*>(node);
            return !matchesFilters(id, n->child.get());
        }
        default:
            return true;
    }
}

ResultPage SearchEngine::search(std::string_view queryString, const QueryOptions& opts) {
    ResultPage page;
    if (queryString.empty() || index_.size() == 0) {
        return page;
    }

    auto ast = query::Parser::parse(queryString);
    ast = query::Parser::normalize(std::move(ast));
    if (!ast) {
        return page;
    }

    std::vector<EntryId> candidates;
    candidates.reserve(1024);

    // Identify primary text term for AVX2 needle scanning
    const query::TermNode* primaryTerm = nullptr;
    const query::PhraseNode* primaryPhrase = nullptr;

    if (ast->type == query::NodeType::Term) {
        primaryTerm = static_cast<const query::TermNode*>(ast.get());
    } else if (ast->type == query::NodeType::Phrase) {
        primaryPhrase = static_cast<const query::PhraseNode*>(ast.get());
    } else if (ast->type == query::NodeType::And) {
        const auto* a = static_cast<const query::MultiNode*>(ast.get());
        for (const auto& ch : a->children) {
            if (ch->type == query::NodeType::Term) {
                primaryTerm = static_cast<const query::TermNode*>(ch.get());
                break;
            }
            if (ch->type == query::NodeType::Phrase) {
                primaryPhrase = static_cast<const query::PhraseNode*>(ch.get());
                break;
            }
        }
    }

    // Step 1: Execute fast SIMD scan to populate initial candidate set
    if (primaryTerm) {
        search::scanArenaAVX2(index_, primaryTerm->text, candidates, 50000);
    } else if (primaryPhrase) {
        search::scanArenaAVX2(index_, primaryPhrase->phrase, candidates, 50000);
    } else {
        // If query has no substring term (e.g. pure "ext:pdf" or "size:>10mb"), scan active entries
        for (EntryId i = 0; i < index_.size(); ++i) {
            if (!index_.isDeleted(i)) {
                candidates.push_back(i);
            }
        }
    }

    // Step 2: Evaluate compound AST filters against candidate set
    std::vector<EntryId> filtered;
    filtered.reserve(candidates.size());

    for (EntryId id : candidates) {
        if (matchesFilters(id, ast.get())) {
            filtered.push_back(id);
        }
    }

    page.total = static_cast<uint32_t>(filtered.size());
    page.totalIsFinal = true;

    // Step 3: Paging window slice [offset, offset + count)
    uint32_t startIdx = std::min(opts.offset, page.total);
    uint32_t endIdx = std::min(startIdx + opts.count, page.total);

    page.rows.reserve(endIdx - startIdx);
    for (uint32_t i = startIdx; i < endIdx; ++i) {
        EntryId id = filtered[i];
        ResultRow row;
        row.id = id;
        row.name = std::string(index_.getName(id));
        EntryId p = index_.getParent(id);
        if (p != kNoEntry) {
            row.dir = index_.buildPath(p);
        }
        row.size = index_.getFileSize(id);
        row.mtime = index_.getMtime(id);
        row.flags = index_.getFlags(id);
        row.tier = 3; // Substring tier
        row.score = 100.0f;
        page.rows.push_back(std::move(row));
    }

    return page;
}

} // namespace gx
