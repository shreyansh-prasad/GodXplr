#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <shared_mutex>
#include <algorithm>

namespace gx::content {

struct DocumentMeta {
    uint32_t docId{0};
    std::wstring filePath;
    uint64_t fileSize{0};
    uint64_t mtime{0};
    size_t textBytes{0};
};

class TrigramIndex {
public:
    TrigramIndex() = default;
    ~TrigramIndex() = default;

    TrigramIndex(const TrigramIndex&) = delete;
    TrigramIndex& operator=(const TrigramIndex&) = delete;

    // Registers a document and indexes its content in the positionless trigram inverted index
    void addDocument(
        uint32_t docId,
        const std::wstring& filePath,
        uint64_t fileSize,
        uint64_t mtime,
        std::string_view utf8Text);

    // Removes a document from the inverted index
    void removeDocument(uint32_t docId);

    // Intersects posting lists to find candidate docIds that contain all query trigrams
    [[nodiscard]] std::vector<uint32_t> queryCandidates(std::string_view queryText) const;

    // Set operations for boolean queries (AND, OR, NOT)
    static std::vector<uint32_t> intersectSets(const std::vector<uint32_t>& a, const std::vector<uint32_t>& b);
    static std::vector<uint32_t> unionSets(const std::vector<uint32_t>& a, const std::vector<uint32_t>& b);
    static std::vector<uint32_t> differenceSets(const std::vector<uint32_t>& a, const std::vector<uint32_t>& b);

    // Extracts 24-bit packed trigrams from a string
    static std::vector<uint32_t> extractTrigrams(std::string_view text);

    [[nodiscard]] size_t documentCount() const;
    [[nodiscard]] size_t uniqueTrigramsCount() const;
    [[nodiscard]] size_t totalIndexedTextBytes() const noexcept { return totalTextBytes_; }
    [[nodiscard]] size_t estimatedIndexBytes() const;
    [[nodiscard]] const DocumentMeta* getDocument(uint32_t docId) const;

    void clear();

private:
    mutable std::shared_mutex mutex_;
    std::unordered_map<uint32_t, std::vector<uint32_t>> postings_; // trigram -> sorted docIds
    std::unordered_map<uint32_t, DocumentMeta> documents_;        // docId -> metadata
    size_t totalTextBytes_{0};
};

} // namespace gx::content

