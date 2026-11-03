#include "godxplr/bench/dataset_generator.h"
#include "godxplr/bench/metrics_recorder.h"
#include <iostream>
#include <cstdlib>
#include <cmath>

#define TEST_ASSERT(cond)                                                      \
    do {                                                                       \
        if (!(cond)) {                                                         \
            std::cerr << "Assertion failed: " #cond " at " << __FILE__         \
                      << ":" << __LINE__ << std::endl;                         \
            std::abort();                                                      \
        }                                                                      \
    } while (0)

using namespace gx;
using namespace gx::bench;

void testDeterministicRepeatability() {
    std::cout << "[Test] DeterministicRepeatability...\n";

    GeneratorConfig cfgA{12345, 5000, 0.08, 5};
    DatasetGenerator genA(cfgA);
    DatasetManifest manifestA;
    auto entriesA = genA.generate(manifestA);

    GeneratorConfig cfgB{12345, 5000, 0.08, 5};
    DatasetGenerator genB(cfgB);
    DatasetManifest manifestB;
    auto entriesB = genB.generate(manifestB);

    TEST_ASSERT(manifestA.manifestHash == manifestB.manifestHash);
    TEST_ASSERT(entriesA.size() == entriesB.size());
    TEST_ASSERT(manifestA.totalBytes == manifestB.totalBytes);

    for (size_t i = 0; i < entriesA.size(); ++i) {
        TEST_ASSERT(entriesA[i].frn == entriesB[i].frn);
        TEST_ASSERT(entriesA[i].parentFrn == entriesB[i].parentFrn);
        TEST_ASSERT(entriesA[i].name == entriesB[i].name);
        TEST_ASSERT(entriesA[i].size == entriesB[i].size);
        TEST_ASSERT(entriesA[i].mtime == entriesB[i].mtime);
        TEST_ASSERT(entriesA[i].flags == entriesB[i].flags);
    }

    std::cout << "  Hash: " << manifestA.manifestHash << "\n";
    std::cout << "  -> Passed!\n";
}

void testDistinctSeedsDifferentHashes() {
    std::cout << "[Test] DistinctSeedsDifferentHashes...\n";

    GeneratorConfig cfg1{1001, 1000, 0.08, 5};
    DatasetGenerator gen1(cfg1);
    DatasetManifest manifest1;
    gen1.generate(manifest1);

    GeneratorConfig cfg2{1002, 1000, 0.08, 5};
    DatasetGenerator gen2(cfg2);
    DatasetManifest manifest2;
    gen2.generate(manifest2);

    TEST_ASSERT(manifest1.manifestHash != manifest2.manifestHash);
    std::cout << "  -> Passed!\n";
}

void testDirectoryHierarchyAndIndexPopulate() {
    std::cout << "[Test] DirectoryHierarchyAndIndexPopulate...\n";

    GeneratorConfig cfg{42, 2000, 0.10, 5};
    DatasetGenerator gen(cfg);
    NameIndex index;

    auto manifest = gen.populateIndex(index);
    TEST_ASSERT(index.size() == 2000);
    TEST_ASSERT(manifest.entryCount == 2000);
    TEST_ASSERT(manifest.directoryCount > 0);
    TEST_ASSERT(manifest.fileCount > 0);

    // Verify root resolution and path building
    for (EntryId i = 0; i < static_cast<EntryId>(index.size()); ++i) {
        std::string path = index.buildPath(i);
        TEST_ASSERT(!path.empty());
    }

    std::cout << "  -> Passed!\n";
}

void testMetricsPercentiles() {
    std::cout << "[Test] MetricsPercentiles...\n";

    MetricsRecorder recorder;
    // Insert 100 known values: 1.0, 2.0, ..., 100.0
    for (int i = 1; i <= 100; ++i) {
        recorder.recordSample(static_cast<double>(i));
    }
    recorder.recordMemory(100 * 1024 * 1024, 80 * 1024 * 1024);

    auto stats = recorder.computeStatistics();
    TEST_ASSERT(stats.samplesCount == 100);
    TEST_ASSERT(stats.minMs == 1.0);
    TEST_ASSERT(stats.maxMs == 100.0);
    TEST_ASSERT(std::abs(stats.meanMs - 50.5) < 0.01);
    TEST_ASSERT(std::abs(stats.p50Ms - 50.5) <= 1.0);
    TEST_ASSERT(std::abs(stats.p90Ms - 90.1) <= 1.0);
    TEST_ASSERT(std::abs(stats.p95Ms - 95.05) <= 1.0);
    TEST_ASSERT(std::abs(stats.p99Ms - 99.01) <= 1.0);
    TEST_ASSERT(std::abs(stats.workingSetMb - 100.0) < 0.01);

    DatasetManifest dummyManifest{42, 100, 90, 10, 1048576, "deadbeefcafebabe"};
    std::string json = recorder.toJson("synthetic-latency-test", dummyManifest);
    TEST_ASSERT(json.find("\"schema_version\": \"1.0.0\"") != std::string::npos);
    TEST_ASSERT(json.find("\"scenario\": \"synthetic-latency-test\"") != std::string::npos);
    TEST_ASSERT(json.find("\"p50_ms\":") != std::string::npos);

    std::cout << "  -> Passed!\n";
}

int main() {
    std::cout << "========================================\n";
    std::cout << "Running GodXplr Dataset Generator Tests \n";
    std::cout << "========================================\n";

    testDeterministicRepeatability();
    testDistinctSeedsDifferentHashes();
    testDirectoryHierarchyAndIndexPopulate();
    testMetricsPercentiles();

    std::cout << "All DatasetGenerator tests passed successfully!\n";
    return 0;
}

