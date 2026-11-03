#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace gx {

using EntryId = uint32_t;
inline constexpr EntryId kNoEntry = 0xFFFFFFFFu;

enum EntryFlag : uint8_t {
    kDir        = 1 << 0,
    kHidden     = 1 << 1,
    kSystem     = 1 << 2,
    kReparse    = 1 << 3,
    kMetaKnown  = 1 << 4,
    kDeleted    = 1 << 7
};

enum class SortKey : uint8_t {
    Name,
    Path,
    Size,
    Modified,
    Extension,
    Rank
};

enum class SortDir : uint8_t {
    Asc,
    Desc
};

struct QueryOptions {
    SortKey sort = SortKey::Rank;
    SortDir dir = SortDir::Desc;
    uint32_t offset = 0;
    uint32_t count = 100;
    bool fuzzy = true;
    bool matchPath = false;
    bool caseSensitive = false;
    uint32_t maxFuzzy = 200;
};

struct ResultRow {
    EntryId id{kNoEntry};
    float score{0.0f};
    uint8_t tier{0}; // 0=exact, 1=prefix, 2=word-boundary, 3=substring, 4=fuzzy
    uint64_t size{0};
    uint64_t mtime{0};
    uint8_t flags{0};
    std::string name;
    std::string dir;
};

struct ResultPage {
    uint64_t queryId{0};
    uint32_t total{0};
    bool totalIsFinal{true};
    bool fuzzyPending{false};
    std::vector<ResultRow> rows;
};

} // namespace gx
