#include "godxplr/content/text_sink.h"
#include "godxplr/content/encoding_detector.h"
#include "godxplr/content/text_extractor.h"
#include "godxplr/content/trigram_index.h"
#include "godxplr/content/content_search_engine.h"
#include "godxplr/content/snippet_generator.h"
#include "godxplr/content/content_queue.h"
#include "godxplr/content/ooxml_extractor.h"
#include "godxplr/content/extractor_sandbox.h"

#include <iostream>
#include <string>
#include <vector>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <cstdlib>

#define TEST_ASSERT(cond) \
    do { \
        if (!(cond)) { \
            std::cerr << "Assertion failed at line " << __LINE__ << ": " #cond "\n"; \
            std::abort(); \
        } \
    } while (0)

using namespace gx::content;

void testEncodingDetector() {
    std::cout << "[Test] EncodingDetector...\n";

    // 1. Plain ASCII
    const std::string ascii = "This is a simple ASCII string for testing.";
    size_t bom1 = 0;
    auto enc1 = EncodingDetector::detect(reinterpret_cast<const uint8_t*>(ascii.data()), ascii.size(), bom1);
    TEST_ASSERT(enc1 == EncodingType::Ascii || enc1 == EncodingType::Utf8);
    TEST_ASSERT(bom1 == 0);

    // 2. UTF-8 with BOM
    const uint8_t utf8Bom[] = { 0xEF, 0xBB, 0xBF, 'H', 'e', 'l', 'l', 'o' };
    size_t bom2 = 0;
    auto enc2 = EncodingDetector::detect(utf8Bom, sizeof(utf8Bom), bom2);
    TEST_ASSERT(enc2 == EncodingType::Utf8);
    TEST_ASSERT(bom2 == 3);

    std::string convertedUtf8 = EncodingDetector::toUtf8(utf8Bom, sizeof(utf8Bom), enc2, bom2);
    TEST_ASSERT(convertedUtf8 == "Hello");

    // 3. UTF-16LE with BOM
    const uint8_t utf16LeBom[] = { 0xFF, 0xFE, 'H', 0x00, 'i', 0x00 };
    size_t bom3 = 0;
    auto enc3 = EncodingDetector::detect(utf16LeBom, sizeof(utf16LeBom), bom3);
    TEST_ASSERT(enc3 == EncodingType::Utf16LE);
    TEST_ASSERT(bom3 == 2);

    std::string converted16Le = EncodingDetector::toUtf8(utf16LeBom, sizeof(utf16LeBom), enc3, bom3);
    TEST_ASSERT(converted16Le == "Hi");

    // 4. Binary data
    const uint8_t binaryData[64] = { 0x00, 0x01, 0x02, 0x00, 0x04, 0x00, 0x05 };
    size_t bom4 = 0;
    auto enc4 = EncodingDetector::detect(binaryData, sizeof(binaryData), bom4);
    TEST_ASSERT(enc4 == EncodingType::Binary);

    std::cout << "  -> PASS\n";
}

void testTextExtractor() {
    std::cout << "[Test] TextExtractor...\n";

    TextExtractor extractor(100); // 100 bytes cap

    // Extract small buffer
    std::string sample = "High performance C++20 content indexer for GodXplr.";
    StringTextSink sink1(1024);
    auto status1 = extractor.extractBuffer(reinterpret_cast<const uint8_t*>(sample.data()), sample.size(), sink1);
    TEST_ASSERT(status1 == ExtractionStatus::Success);
    TEST_ASSERT(sink1.text() == sample);

    // Truncation test
    std::string longText(300, 'A');
    StringTextSink sink2(100);
    auto status2 = extractor.extractBuffer(reinterpret_cast<const uint8_t*>(longText.data()), longText.size(), sink2);
    TEST_ASSERT(status2 == ExtractionStatus::Truncated);
    TEST_ASSERT(sink2.text().size() == 100);

    // Binary skipping test
    const uint8_t bin[32] = { 0 };
    StringTextSink sink3(1024);
    auto status3 = extractor.extractBuffer(bin, sizeof(bin), sink3);
    TEST_ASSERT(status3 == ExtractionStatus::BinarySkipped);

    std::cout << "  -> PASS\n";
}

void testTrigramIndex() {
    std::cout << "[Test] TrigramIndex (ADR-0024 Positionless Index)...\n";

    TrigramIndex index;

    // Document 1
    std::string doc1Text = "The quick brown fox jumps over the lazy dog.";
    index.addDocument(1, L"C:\\test\\doc1.txt", 1024, 1000, doc1Text);

    // Document 2
    std::string doc2Text = "A quick blue rabbit jumps over a sleeping cat.";
    index.addDocument(2, L"C:\\test\\doc2.txt", 2048, 1050, doc2Text);

    // Document 3
    std::string doc3Text = "Completely unrelated content without animals.";
    index.addDocument(3, L"C:\\test\\doc3.txt", 512, 1100, doc3Text);

    TEST_ASSERT(index.documentCount() == 3);
    TEST_ASSERT(index.uniqueTrigramsCount() > 0);

    // Candidate query for "quick"
    auto cQuick = index.queryCandidates("quick");
    TEST_ASSERT(cQuick.size() == 2);
    TEST_ASSERT(cQuick[0] == 1 && cQuick[1] == 2);

    // Candidate query for "brown fox"
    auto cFox = index.queryCandidates("brown fox");
    TEST_ASSERT(cFox.size() == 1 && cFox[0] == 1);

    // Candidate query for non-existent term
    auto cElephant = index.queryCandidates("elephant");
    TEST_ASSERT(cElephant.empty());

    // Set operations
    std::vector<uint32_t> setA = { 1, 3, 5, 7 };
    std::vector<uint32_t> setB = { 3, 4, 7, 9 };

    auto inter = TrigramIndex::intersectSets(setA, setB);
    TEST_ASSERT((inter == std::vector<uint32_t>{ 3, 7 }));

    auto uni = TrigramIndex::unionSets(setA, setB);
    TEST_ASSERT((uni == std::vector<uint32_t>{ 1, 3, 4, 5, 7, 9 }));

    auto diff = TrigramIndex::differenceSets(setA, setB);
    TEST_ASSERT((diff == std::vector<uint32_t>{ 1, 5 }));

    // Memory footprint verification (ADR-0024)
    size_t indexBytes = index.estimatedIndexBytes();
    size_t textBytes = index.totalIndexedTextBytes();
    std::cout << "  Indexed text: " << textBytes << " bytes, Est index: " << indexBytes << " bytes\n";
    TEST_ASSERT(indexBytes > 0);

    // Remove document
    index.removeDocument(2);
    TEST_ASSERT(index.documentCount() == 2);
    auto cQuickAfter = index.queryCandidates("quick");
    TEST_ASSERT(cQuickAfter.size() == 1 && cQuickAfter[0] == 1);

    std::cout << "  -> PASS\n";
}

void testContentSearchEngine() {
    std::cout << "[Test] ContentSearchEngine...\n";

    TrigramIndex index;
    std::unordered_map<uint32_t, std::string> corpus;

    corpus[1] = "Modern operating system engineering requires rigorous memory bounds.";
    corpus[2] = "High performance C++20 architecture with lock-free ring buffers.";
    corpus[3] = "Fast indexing and SIMD needle scanning in GodXplr core.";

    index.addDocument(1, L"doc1.txt", 100, 1000, corpus[1]);
    index.addDocument(2, L"doc2.txt", 200, 1100, corpus[2]);
    index.addDocument(3, L"doc3.txt", 300, 1200, corpus[3]);

    auto resolver = [&](uint32_t docId, const std::wstring&) -> std::string {
        auto it = corpus.find(docId);
        return (it != corpus.end()) ? it->second : "";
    };

    ContentSearchEngine engine(index, resolver);

    // 1. Exact substring verification test
    size_t offset = 0;
    bool found = ContentSearchEngine::verifySubstring("abcdef", "CDE", offset);
    TEST_ASSERT(found && offset == 2);

    // 2. Search execution
    auto res = engine.search("ring buffer");
    TEST_ASSERT(res.candidatesExamined >= 1);
    TEST_ASSERT(res.verifiedHitsCount == 1);
    TEST_ASSERT(res.hits.size() == 1);
    TEST_ASSERT(res.hits[0].docId == 2);
    TEST_ASSERT(!res.hits[0].snippet.empty());
    std::cout << "  Snippet: \"" << res.hits[0].snippet << "\"\n";

    // 3. Multi-word search
    auto resMulti = engine.search("operating system");
    TEST_ASSERT(resMulti.verifiedHitsCount == 1);
    TEST_ASSERT(resMulti.hits[0].docId == 1);

    // 4. Cancellation token
    std::atomic<bool> cancelToken{true};
    auto resCanceled = engine.search("ring buffer", 100, &cancelToken);
    TEST_ASSERT(resCanceled.verifiedHitsCount == 0);

    std::cout << "  -> PASS\n";
}

void testSnippetGenerator() {
    std::cout << "[Test] SnippetGenerator...\n";

    std::string sample = "The quick brown fox jumps over the lazy dog and runs across the wide green open valley.";
    // Match "jumps" at offset 20, len 5
    std::string snippet = SnippetGenerator::generateSnippet(sample, 20, 5, 20);
    TEST_ASSERT(!snippet.empty());
    TEST_ASSERT(snippet.find("jumps") != std::string::npos);
    std::cout << "  Generated snippet: \"" << snippet << "\"\n";

    std::cout << "  -> PASS\n";
}

void testOoxmlExtractor() {
    std::cout << "[Test] OoxmlExtractor...\n";

    OoxmlExtractor extractor(2 * 1024 * 1024);

    std::string xml = "<w:document><w:body><w:p><w:t>GodXplr Document</w:t></w:p>"
                      "<w:p><w:t>Tested &amp; Verified with &lt;Safe&gt; &quot;XML&quot;</w:t></w:p>"
                      "</w:body></w:document>";

    StringTextSink sink;
    auto status = extractor.extractXmlBuffer(reinterpret_cast<const uint8_t*>(xml.data()), xml.size(), sink);
    TEST_ASSERT(status == ExtractionStatus::Success);
    TEST_ASSERT(sink.text().find("GodXplr Document") != std::string::npos);
    TEST_ASSERT(sink.text().find("Tested & Verified with <Safe> \"XML\"") != std::string::npos);

    std::cout << "  Extracted XML Text: \"" << sink.text() << "\"\n";
    std::cout << "  -> PASS\n";
}

void testContentQueueAndReconciliation() {
    std::cout << "[Test] ContentQueue and Reconciliation...\n";

    ContentQueue queue;

    QueueItem q1;
    q1.frn = 101;
    q1.filePath = L"C:\\file1.txt";
    q1.priority = 100; // High
    q1.size = 1000;
    q1.mtime = 2000;
    queue.enqueue(q1);

    QueueItem q2;
    q2.frn = 102;
    q2.filePath = L"C:\\file2.txt";
    q2.priority = 50;  // Medium
    q2.size = 2000;
    q2.mtime = 3000;
    queue.enqueue(q2);

    TEST_ASSERT(queue.pendingCount() == 2);

    // Dequeue priority check (q1 must precede q2)
    auto next1 = queue.dequeueNext();
    TEST_ASSERT(next1.has_value() && next1->frn == 101);
    queue.markComplete(101);
    TEST_ASSERT(queue.indexedCount() == 1);

    // Failure retry & poison logic
    queue.markFailed(102);
    auto it2 = queue.getItem(102);
    TEST_ASSERT(it2.has_value() && it2->status == QueueStatus::Failed && it2->attempts == 1);
    TEST_ASSERT(queue.failedCount() == 1);

    // Second failure poisons the entry
    queue.markFailed(102);
    it2 = queue.getItem(102);
    TEST_ASSERT(it2.has_value() && it2->status == QueueStatus::Poisoned && it2->attempts == 2);
    TEST_ASSERT(queue.poisonedCount() == 1);

    // Reconciliation test
    std::vector<FileSnapshot> snapshot = {
        { 101, L"C:\\file1.txt", 1500, 2500, false }, // Changed size and mtime!
        { 103, L"C:\\file3.txt", 500,  1000, false }, // New file!
        // 102 was deleted from disk (not in snapshot)
    };

    auto report = queue.reconcile(snapshot, 1);
    TEST_ASSERT(report.newEnqueued == 1);     // 103
    TEST_ASSERT(report.changedEnqueued == 1); // 101
    TEST_ASSERT(report.removedCount == 1);    // 102

    auto item101 = queue.getItem(101);
    TEST_ASSERT(item101.has_value() && item101->status == QueueStatus::Pending && item101->reason == QueueReason::Changed);

    auto item103 = queue.getItem(103);
    TEST_ASSERT(item103.has_value() && item103->status == QueueStatus::Pending && item103->reason == QueueReason::New);

    TEST_ASSERT(!queue.getItem(102).has_value()); // Removed

    std::cout << "  -> PASS\n";
}

void testExtractorSandbox() {
    std::cout << "[Test] ExtractorSandbox (Win32 Job Object)...\n";

    SandboxLimits limits;
    limits.maxMemoryBytes = 128 * 1024 * 1024;
    limits.timeoutSeconds = 5;
    limits.restrictUi = true;
    limits.killOnClose = true;

    ExtractorSandbox sandbox(limits);
    TEST_ASSERT(sandbox.jobHandle() != nullptr);

    // Run clean child process under sandbox
    uint32_t exitCode = 999;
    auto res = sandbox.runProcess(L"cmd.exe /c exit 0", &exitCode);
    TEST_ASSERT(res == SandboxResult::Success);
    TEST_ASSERT(exitCode == 0);

    std::cout << "  Child execution under Job Object exit code: " << exitCode << "\n";
    std::cout << "  -> PASS\n";
}

int main() {
    std::cout << "========================================\n";
    std::cout << "  GodXplr Spec 02 Content Index Tests   \n";
    std::cout << "========================================\n";

    testEncodingDetector();
    testTextExtractor();
    testTrigramIndex();
    testContentSearchEngine();
    testSnippetGenerator();
    testOoxmlExtractor();
    testContentQueueAndReconciliation();
    testExtractorSandbox();

    std::cout << "========================================\n";
    std::cout << "  ALL CONTENT INDEX TESTS PASSED (100%) \n";
    std::cout << "========================================\n";
    return 0;
}
