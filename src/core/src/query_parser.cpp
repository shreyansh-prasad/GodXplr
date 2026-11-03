#include "godxplr/core/query_parser.h"
#include <algorithm>
#include <cctype>
#include <sstream>

namespace gx::query {

namespace {

class Lexer {
public:
    explicit Lexer(std::string_view src) : src_(src) {}

    enum class TokenKind {
        End,
        Term,
        Phrase,
        OrPipe,
        NotBang,
        GroupOpen,
        GroupClose,
        Filter
    };

    struct Token {
        TokenKind kind{TokenKind::End};
        std::string text;
        std::string filterKey;
        std::string filterVal;
    };

    Token next() {
        skipWhitespace();
        if (pos_ >= src_.size()) {
            return {TokenKind::End, "", "", ""};
        }

        char c = src_[pos_];
        if (c == '|') {
            pos_++;
            return {TokenKind::OrPipe, "|", "", ""};
        }
        if (c == '!') {
            pos_++;
            return {TokenKind::NotBang, "!", "", ""};
        }
        if (c == '<' || c == '(') {
            pos_++;
            return {TokenKind::GroupOpen, "(", "", ""};
        }
        if (c == '>' || c == ')') {
            pos_++;
            return {TokenKind::GroupClose, ")", "", ""};
        }
        if (c == '"') {
            return readPhrase();
        }

        return readTermOrFilter();
    }

    [[nodiscard]] bool hasMore() const { return pos_ < src_.size(); }

private:
    void skipWhitespace() {
        while (pos_ < src_.size() && std::isspace(static_cast<unsigned char>(src_[pos_]))) {
            pos_++;
        }
    }

    Token readPhrase() {
        pos_++; // skip opening quote
        size_t start = pos_;
        while (pos_ < src_.size() && src_[pos_] != '"') {
            pos_++;
        }
        std::string val(src_.substr(start, pos_ - start));
        if (pos_ < src_.size() && src_[pos_] == '"') {
            pos_++; // skip closing quote
        }
        return {TokenKind::Phrase, val, "", ""};
    }

    Token readTermOrFilter() {
        size_t start = pos_;
        bool inFilter = false;
        while (pos_ < src_.size()) {
            char c = src_[pos_];
            if (std::isspace(static_cast<unsigned char>(c)) || c == '|') {
                break;
            }
            if (!inFilter && (c == '(' || c == ')' || c == '<' || c == '>')) {
                break;
            }
            if (c == ':') {
                inFilter = true;
            }
            pos_++;
        }

        std::string_view raw = src_.substr(start, pos_ - start);
        size_t colon = raw.find(':');
        if (colon != std::string_view::npos && colon > 0 && colon + 1 <= raw.size()) {
            std::string key(raw.substr(0, colon));
            std::string val(raw.substr(colon + 1));
            // Lowercase key
            for (char& k : key) {
                k = static_cast<char>(std::tolower(static_cast<unsigned char>(k)));
            }
            return {TokenKind::Filter, std::string(raw), key, val};
        }

        return {TokenKind::Term, std::string(raw), "", ""};
    }

    std::string_view src_;
    size_t pos_{0};
};

uint64_t parseSizeModifier(std::string_view val) {
    if (val.empty()) {
        return 0;
    }
    double num = 0.0;
    size_t idx = 0;
    try {
        num = std::stod(std::string(val), &idx);
    } catch (...) {
        return 0;
    }

    std::string unit;
    if (idx < val.size()) {
        unit = std::string(val.substr(idx));
        for (char& u : unit) {
            u = static_cast<char>(std::tolower(static_cast<unsigned char>(u)));
        }
    }

    uint64_t mult = 1;
    if (unit == "kb" || unit == "k") mult = 1024ULL;
    else if (unit == "mb" || unit == "m") mult = 1024ULL * 1024ULL;
    else if (unit == "gb" || unit == "g") mult = 1024ULL * 1024ULL * 1024ULL;

    return static_cast<uint64_t>(num * static_cast<double>(mult));
}

NodePtr buildFilterNode(std::string_view key, std::string_view val) {
    if (key == "ext") {
        std::vector<std::string> exts;
        std::stringstream ss{std::string(val)};
        std::string item;
        while (std::getline(ss, item, ';')) {
            if (!item.empty()) {
                if (item.front() == '.') item.erase(0, 1);
                exts.push_back(item);
            }
        }
        return std::make_unique<ExtNode>(std::move(exts));
    }
    if (key == "size") {
        if (val.rfind('>', 0) == 0) {
            return std::make_unique<SizeNode>(SizeOp::GreaterThan, parseSizeModifier(val.substr(1)));
        }
        if (val.rfind('<', 0) == 0) {
            return std::make_unique<SizeNode>(SizeOp::LessThan, parseSizeModifier(val.substr(1)));
        }
        size_t dots = val.find("..");
        if (dots != std::string_view::npos) {
            uint64_t minB = parseSizeModifier(val.substr(0, dots));
            uint64_t maxB = parseSizeModifier(val.substr(dots + 2));
            return std::make_unique<SizeNode>(SizeOp::Range, minB, maxB);
        }
        return std::make_unique<SizeNode>(SizeOp::Equal, parseSizeModifier(val));
    }
    if (key == "file") {
        return std::make_unique<FlagNode>(false);
    }
    if (key == "folder") {
        return std::make_unique<FlagNode>(true);
    }
    if (key == "path") {
        return std::make_unique<PathNode>(std::string(val));
    }
    if (key == "regex") {
        return std::make_unique<RegexNode>(std::string(val));
    }

    // Default fallback to term
    return std::make_unique<TermNode>(std::string(key) + ":" + std::string(val));
}

class RecursiveParser {
public:
    explicit RecursiveParser(std::string_view src) : lexer_(src) {
        advance();
    }

    NodePtr parseExpression() {
        return parseOr();
    }

private:
    void advance() {
        cur_ = lexer_.next();
    }

    NodePtr parseOr() {
        NodePtr left = parseAnd();
        if (!left) return nullptr;

        std::vector<NodePtr> children;
        children.push_back(std::move(left));

        while (cur_.kind == Lexer::TokenKind::OrPipe) {
            advance();
            NodePtr right = parseAnd();
            if (right) {
                children.push_back(std::move(right));
            }
        }

        if (children.size() == 1) {
            return std::move(children.front());
        }
        return std::make_unique<MultiNode>(NodeType::Or, std::move(children));
    }

    NodePtr parseAnd() {
        NodePtr left = parseUnary();
        if (!left) return nullptr;

        std::vector<NodePtr> children;
        children.push_back(std::move(left));

        while (cur_.kind != Lexer::TokenKind::End &&
               cur_.kind != Lexer::TokenKind::OrPipe &&
               cur_.kind != Lexer::TokenKind::GroupClose) {
            NodePtr right = parseUnary();
            if (right) {
                children.push_back(std::move(right));
            }
        }

        if (children.size() == 1) {
            return std::move(children.front());
        }
        return std::make_unique<MultiNode>(NodeType::And, std::move(children));
    }

    NodePtr parseUnary() {
        if (cur_.kind == Lexer::TokenKind::NotBang) {
            advance();
            NodePtr child = parseUnary();
            if (child) {
                return std::make_unique<UnaryNode>(NodeType::Not, std::move(child));
            }
            return nullptr;
        }
        return parsePrimary();
    }

    NodePtr parsePrimary() {
        if (cur_.kind == Lexer::TokenKind::GroupOpen) {
            advance();
            NodePtr expr = parseOr();
            if (cur_.kind == Lexer::TokenKind::GroupClose) {
                advance();
            }
            return expr;
        }
        if (cur_.kind == Lexer::TokenKind::Term) {
            std::string t = cur_.text;
            advance();
            return std::make_unique<TermNode>(t);
        }
        if (cur_.kind == Lexer::TokenKind::Phrase) {
            std::string p = cur_.text;
            advance();
            return std::make_unique<PhraseNode>(p);
        }
        if (cur_.kind == Lexer::TokenKind::Filter) {
            std::string k = cur_.filterKey;
            std::string v = cur_.filterVal;
            advance();
            return buildFilterNode(k, v);
        }
        return nullptr;
    }

    Lexer lexer_;
    Lexer::Token cur_;
};

} // namespace

NodePtr Parser::parse(std::string_view query) {
    if (query.empty()) {
        return nullptr;
    }
    RecursiveParser rp(query);
    return rp.parseExpression();
}

NodePtr Parser::normalize(NodePtr root) {
    if (!root) {
        return nullptr;
    }

    // Flatten nested AND / OR nodes
    if (root->type == NodeType::And || root->type == NodeType::Or) {
        auto* multi = static_cast<MultiNode*>(root.get());
        std::vector<NodePtr> flatChildren;
        for (auto& ch : multi->children) {
            auto normChild = normalize(std::move(ch));
            if (normChild && normChild->type == root->type) {
                auto* subMulti = static_cast<MultiNode*>(normChild.get());
                for (auto& grandChild : subMulti->children) {
                    flatChildren.push_back(std::move(grandChild));
                }
            } else if (normChild) {
                flatChildren.push_back(std::move(normChild));
            }
        }
        multi->children = std::move(flatChildren);
        if (multi->children.size() == 1) {
            return std::move(multi->children.front());
        }
        return root;
    }

    if (root->type == NodeType::Not) {
        auto* un = static_cast<UnaryNode*>(root.get());
        un->child = normalize(std::move(un->child));
        // Double negation: !(!A) -> A
        if (un->child && un->child->type == NodeType::Not) {
            auto* subUn = static_cast<UnaryNode*>(un->child.get());
            return std::move(subUn->child);
        }
        return root;
    }

    return root;
}

std::string Parser::dump(const Node* node) {
    if (!node) return "null";
    switch (node->type) {
        case NodeType::Term: {
            const auto* t = static_cast<const TermNode*>(node);
            return "Term(" + t->text + ")";
        }
        case NodeType::Phrase: {
            const auto* p = static_cast<const PhraseNode*>(node);
            return "Phrase(\"" + p->phrase + "\")";
        }
        case NodeType::Ext: {
            const auto* e = static_cast<const ExtNode*>(node);
            std::string out = "Ext(";
            for (size_t i = 0; i < e->extensions.size(); ++i) {
                if (i > 0) out += ",";
                out += e->extensions[i];
            }
            return out + ")";
        }
        case NodeType::Size: {
            const auto* s = static_cast<const SizeNode*>(node);
            return "Size(" + std::to_string(s->minBytes) + ".." + std::to_string(s->maxBytes) + ")";
        }
        case NodeType::Path: {
            const auto* p = static_cast<const PathNode*>(node);
            return "Path(" + p->pathPattern + ")";
        }
        case NodeType::Regex: {
            const auto* r = static_cast<const RegexNode*>(node);
            return "Regex(" + r->pattern + ")";
        }
        case NodeType::FlagFile:
            return "Flag(File)";
        case NodeType::FlagFolder:
            return "Flag(Folder)";
        case NodeType::And: {
            const auto* a = static_cast<const MultiNode*>(node);
            std::string out = "AND(";
            for (size_t i = 0; i < a->children.size(); ++i) {
                if (i > 0) out += " ";
                out += dump(a->children[i].get());
            }
            return out + ")";
        }
        case NodeType::Or: {
            const auto* o = static_cast<const MultiNode*>(node);
            std::string out = "OR(";
            for (size_t i = 0; i < o->children.size(); ++i) {
                if (i > 0) out += " | ";
                out += dump(o->children[i].get());
            }
            return out + ")";
        }
        case NodeType::Not: {
            const auto* n = static_cast<const UnaryNode*>(node);
            return "NOT(" + dump(n->child.get()) + ")";
        }
        default:
            return "Unknown";
    }
}

} // namespace gx::query
