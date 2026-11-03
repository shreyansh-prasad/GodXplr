#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace gx::query {

enum class NodeType : uint8_t {
    Term,
    Phrase,
    Ext,
    Size,
    DateModified,
    Path,
    Parent,
    Regex,
    FlagFile,
    FlagFolder,
    And,
    Or,
    Not
};

enum class SizeOp : uint8_t {
    Equal,
    GreaterThan,
    LessThan,
    Range
};

struct Node {
    NodeType type;
    explicit Node(NodeType t) : type(t) {}
    virtual ~Node() = default;
};

using NodePtr = std::unique_ptr<Node>;

struct TermNode : public Node {
    std::string text;
    bool caseSensitive{false};
    explicit TermNode(std::string t, bool cs = false)
        : Node(NodeType::Term), text(std::move(t)), caseSensitive(cs) {}
};

struct PhraseNode : public Node {
    std::string phrase;
    bool caseSensitive{false};
    explicit PhraseNode(std::string p, bool cs = false)
        : Node(NodeType::Phrase), phrase(std::move(p)), caseSensitive(cs) {}
};

struct ExtNode : public Node {
    std::vector<std::string> extensions; // e.g. ["cpp", "h"]
    explicit ExtNode(std::vector<std::string> exts)
        : Node(NodeType::Ext), extensions(std::move(exts)) {}
};

struct SizeNode : public Node {
    SizeOp op{SizeOp::Equal};
    uint64_t minBytes{0};
    uint64_t maxBytes{0};
    SizeNode(SizeOp o, uint64_t minB, uint64_t maxB = 0)
        : Node(NodeType::Size), op(o), minBytes(minB), maxBytes(maxB) {}
};

struct DateNode : public Node {
    uint64_t minFiletime{0};
    uint64_t maxFiletime{0};
    DateNode(uint64_t minFt, uint64_t maxFt)
        : Node(NodeType::DateModified), minFiletime(minFt), maxFiletime(maxFt) {}
};

struct PathNode : public Node {
    std::string pathPattern;
    explicit PathNode(std::string p)
        : Node(NodeType::Path), pathPattern(std::move(p)) {}
};

struct RegexNode : public Node {
    std::string pattern;
    explicit RegexNode(std::string p)
        : Node(NodeType::Regex), pattern(std::move(p)) {}
};

struct FlagNode : public Node {
    explicit FlagNode(bool isFolder)
        : Node(isFolder ? NodeType::FlagFolder : NodeType::FlagFile) {}
};

struct UnaryNode : public Node {
    NodePtr child;
    UnaryNode(NodeType t, NodePtr c) : Node(t), child(std::move(c)) {}
};

struct MultiNode : public Node {
    std::vector<NodePtr> children;
    MultiNode(NodeType t, std::vector<NodePtr> c)
        : Node(t), children(std::move(c)) {}
};

} // namespace gx::query

