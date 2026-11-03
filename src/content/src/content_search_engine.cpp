#include "godxplr/content/content_search_engine.h"
#include "godxplr/content/text_extractor.h"
#include "godxplr/content/snippet_generator.h"
#include <chrono>
#include <cctype>
#include <algorithm>

namespace gx::content {

ContentSearchEngine::ContentSearchEngine(const TrigramIndex& index, ContentResolver resolver)
    : index_(index), resolver_(std::move(resolver))
{
}

bool ContentSearchEngine::verifySubstring(
    std::string_view text,
    std::string_view query,
    size_t& outOffset)
{
    if (query.empty()) {
        outOffset = 0;
        return true;
    }
    if (text.size() < query.size()) {
        return false;
    }

    const size_t maxStart = text.size() - query.size();
    for (size_t i = 0; i <= maxStart; ++i) {
        bool match = true;
        for (size_t j = 0; j < query.size(); ++j) {
            char tc = static_cast<char>(std::tolower(static_cast<unsigned char>(text[i + j])));
            char qc = static_cast<char>(std::tolower(static_cast<unsigned char>(query[j])));
            if (tc != qc) {
                match = false;
                break;
            }
        }
        if (match) {
            outOffset = i;
            return true;
        }
    }

    return false;
}

ContentSearchResult ContentSearchEngine::search(
    std::string_view queryText,
    size_t maxResults,
    const std::atomic<bool>* cancelToken)
{
    auto startTime = std::chrono::high_resolution_clock::now();
    ContentSearchResult result;

    if (queryText.empty()) {
        return result;
    }

    auto candidates = index_.queryCandidates(queryText);

    for (uint32_t docId : candidates) {
        if (cancelToken && cancelToken->load(std::memory_order_relaxed)) {
            break;
        }

        result.candidatesExamined++;

        const auto* doc = index_.getDocument(docId);
        if (!doc) {
            continue;
        }

        std::string text;
        if (resolver_) {
            text = resolver_(docId, doc->filePath);
        } else {
            TextExtractor extractor(2 * 1024 * 1024);
            StringTextSink sink;
            auto status = extractor.extractFile(doc->filePath, sink);
            if (status != ExtractionStatus::Success && status != ExtractionStatus::Truncated) {
                continue;
            }
            text = sink.text();
        }

        size_t matchOffset = 0;
        if (!verifySubstring(text, queryText, matchOffset)) {
            continue;
        }

        result.verifiedHitsCount++;

        ContentHit hit;
        hit.docId = docId;
        hit.filePath = doc->filePath;
        hit.fileSize = doc->fileSize;
        hit.mtime = doc->mtime;
        hit.matchOffset = matchOffset;
        hit.snippet = SnippetGenerator::generateSnippet(text, matchOffset, queryText.size(), 80);
        hit.score = 100.0 + (50.0 / (1.0 + static_cast<double>(matchOffset) * 0.01));

        result.hits.push_back(std::move(hit));
        if (result.hits.size() >= maxResults) {
            break;
        }
    }

    std::sort(result.hits.begin(), result.hits.end(), [](const ContentHit& a, const ContentHit& b) {
        return a.score > b.score;
    });

    auto endTime = std::chrono::high_resolution_clock::now();
    result.elapsedMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();

    return result;
}

} // namespace gx::content

