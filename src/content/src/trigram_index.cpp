#include "godxplr/content/trigram_index.h"
#include <cctype>

namespace gx::content {

std::vector<uint32_t> TrigramIndex::extractTrigrams(std::string_view text) {
    if (text.size() < 3) {
        return {};
    }

    std::vector<uint32_t> trigrams;
    trigrams.reserve(text.size() - 2);

    for (size_t i = 0; i + 2 < text.size(); ++i) {
        uint32_t b0 = static_cast<uint8_t>(std::tolower(static_cast<unsigned char>(text[i])));
        uint32_t b1 = static_cast<uint8_t>(std::tolower(static_cast<unsigned char>(text[i + 1])));
        uint32_t b2 = static_cast<uint8_t>(std::tolower(static_cast<unsigned char>(text[i + 2])));

        uint32_t tg = (b0 << 16) | (b1 << 8) | b2;
        trigrams.push_back(tg);
    }

    std::sort(trigrams.begin(), trigrams.end());
    trigrams.erase(std::unique(trigrams.begin(), trigrams.end()), trigrams.end());

    return trigrams;
}

void TrigramIndex::addDocument(
    uint32_t docId,
    const std::wstring& filePath,
    uint64_t fileSize,
    uint64_t mtime,
    std::string_view utf8Text)
{
    auto trigrams = extractTrigrams(utf8Text);

    std::unique_lock<std::shared_mutex> lock(mutex_);

    // If document already exists, remove it first
    auto it = documents_.find(docId);
    if (it != documents_.end()) {
        totalTextBytes_ -= it->second.textBytes;
        for (auto& [tg, list] : postings_) {
            auto pos = std::lower_bound(list.begin(), list.end(), docId);
            if (pos != list.end() && *pos == docId) {
                list.erase(pos);
            }
        }
    }

    DocumentMeta meta;
    meta.docId = docId;
    meta.filePath = filePath;
    meta.fileSize = fileSize;
    meta.mtime = mtime;
    meta.textBytes = utf8Text.size();
    documents_[docId] = std::move(meta);
    totalTextBytes_ += utf8Text.size();

    for (uint32_t tg : trigrams) {
        auto& list = postings_[tg];
        auto pos = std::lower_bound(list.begin(), list.end(), docId);
        if (pos == list.end() || *pos != docId) {
            list.insert(pos, docId);
        }
    }
}

void TrigramIndex::removeDocument(uint32_t docId) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    auto it = documents_.find(docId);
    if (it == documents_.end()) {
        return;
    }

    totalTextBytes_ -= it->second.textBytes;
    documents_.erase(it);

    for (auto& [tg, list] : postings_) {
        auto pos = std::lower_bound(list.begin(), list.end(), docId);
        if (pos != list.end() && *pos == docId) {
            list.erase(pos);
        }
    }
}

std::vector<uint32_t> TrigramIndex::queryCandidates(std::string_view queryText) const {
    auto queryTgs = extractTrigrams(queryText);

    std::shared_lock<std::shared_mutex> lock(mutex_);
    if (queryTgs.empty()) {
        // Query has < 3 characters; return all document IDs for candidate verification
        std::vector<uint32_t> allDocs;
        allDocs.reserve(documents_.size());
        for (const auto& [docId, _] : documents_) {
            allDocs.push_back(docId);
        }
        std::sort(allDocs.begin(), allDocs.end());
        return allDocs;
    }

    // Sort trigrams by posting list size ascending to intersect most selective first
    struct TrigramRef {
        uint32_t tg;
        size_t count;
    };
    std::vector<TrigramRef> sortedTgs;
    sortedTgs.reserve(queryTgs.size());

    for (uint32_t tg : queryTgs) {
        auto it = postings_.find(tg);
        if (it == postings_.end() || it->second.empty()) {
            return {}; // A required trigram does not exist anywhere in the index
        }
        sortedTgs.push_back({tg, it->second.size()});
    }

    std::sort(sortedTgs.begin(), sortedTgs.end(), [](const TrigramRef& a, const TrigramRef& b) {
        return a.count < b.count;
    });

    // Start with the smallest posting list
    std::vector<uint32_t> candidates = postings_.at(sortedTgs[0].tg);

    for (size_t i = 1; i < sortedTgs.size(); ++i) {
        const auto& nextList = postings_.at(sortedTgs[i].tg);
        candidates = intersectSets(candidates, nextList);
        if (candidates.empty()) {
            break;
        }
    }

    return candidates;
}

std::vector<uint32_t> TrigramIndex::intersectSets(const std::vector<uint32_t>& a, const std::vector<uint32_t>& b) {
    std::vector<uint32_t> result;
    result.reserve(std::min(a.size(), b.size()));

    size_t i = 0;
    size_t j = 0;
    while (i < a.size() && j < b.size()) {
        if (a[i] == b[j]) {
            result.push_back(a[i]);
            i++;
            j++;
        } else if (a[i] < b[j]) {
            i++;
        } else {
            j++;
        }
    }
    return result;
}

std::vector<uint32_t> TrigramIndex::unionSets(const std::vector<uint32_t>& a, const std::vector<uint32_t>& b) {
    std::vector<uint32_t> result;
    result.reserve(a.size() + b.size());

    size_t i = 0;
    size_t j = 0;
    while (i < a.size() && j < b.size()) {
        if (a[i] == b[j]) {
            result.push_back(a[i]);
            i++;
            j++;
        } else if (a[i] < b[j]) {
            result.push_back(a[i]);
            i++;
        } else {
            result.push_back(b[j]);
            j++;
        }
    }
    while (i < a.size()) result.push_back(a[i++]);
    while (j < b.size()) result.push_back(b[j++]);
    return result;
}

std::vector<uint32_t> TrigramIndex::differenceSets(const std::vector<uint32_t>& a, const std::vector<uint32_t>& b) {
    std::vector<uint32_t> result;
    result.reserve(a.size());

    size_t i = 0;
    size_t j = 0;
    while (i < a.size() && j < b.size()) {
        if (a[i] == b[j]) {
            i++;
            j++;
        } else if (a[i] < b[j]) {
            result.push_back(a[i]);
            i++;
        } else {
            j++;
        }
    }
    while (i < a.size()) result.push_back(a[i++]);
    return result;
}

size_t TrigramIndex::documentCount() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return documents_.size();
}

size_t TrigramIndex::uniqueTrigramsCount() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return postings_.size();
}

size_t TrigramIndex::estimatedIndexBytes() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    size_t totalBytes = sizeof(TrigramIndex);
    for (const auto& [_, list] : postings_) {
        totalBytes += sizeof(uint32_t) + sizeof(std::vector<uint32_t>) + list.capacity() * sizeof(uint32_t);
    }
    totalBytes += documents_.size() * sizeof(DocumentMeta);
    return totalBytes;
}

const DocumentMeta* TrigramIndex::getDocument(uint32_t docId) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    auto it = documents_.find(docId);
    if (it != documents_.end()) {
        return &(it->second);
    }
    return nullptr;
}

void TrigramIndex::clear() {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    postings_.clear();
    documents_.clear();
    totalTextBytes_ = 0;
}

} // namespace gx::content

