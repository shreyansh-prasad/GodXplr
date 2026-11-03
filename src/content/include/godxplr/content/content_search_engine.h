#pragma once

#include "godxplr/content/trigram_index.h"
#include <string>
#include <vector>
#include <atomic>
#include <optional>
#include <functional>

namespace gx::content {

struct ContentHit {
    uint32_t docId{0};
    std::wstring filePath;
    uint64_t fileSize{0};
    uint64_t mtime{0};
    std::string snippet;
    size_t matchOffset{0};
    double score{0.0};
};

struct ContentSearchResult {
    std::vector<ContentHit> hits;
    size_t candidatesExamined{0};
    size_t verifiedHitsCount{0};
    double elapsedMs{0.0};
};

class ContentSearchEngine {
public:
    using ContentResolver = std::function<std::string(uint32_t docId, const std::wstring& filePath)>;

    explicit ContentSearchEngine(const TrigramIndex& index, ContentResolver resolver = nullptr);
    ~ContentSearchEngine() = default;

    ContentSearchEngine(const ContentSearchEngine&) = delete;
    ContentSearchEngine& operator=(const ContentSearchEngine&) = delete;

    // Executes a content search query with candidate intersection and streaming verification
    ContentSearchResult search(
        std::string_view queryText,
        size_t maxResults = 100,
        const std::atomic<bool>* cancelToken = nullptr);

    // Fast-path substring verification in plain text
    static bool verifySubstring(std::string_view text, std::string_view query, size_t& outOffset);

private:
    const TrigramIndex& index_;
    ContentResolver resolver_;
};

} // namespace gx::content
