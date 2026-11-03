#include "godxplr/bench/dataset_generator.h"
#include "godxplr/bench/metrics_recorder.h"
#include "godxplr/core/search_engine.h"
#include <iostream>
#include <chrono>
#include <fstream>
#include <numeric>
#include <algorithm>

using namespace gx;
using namespace gx::bench;

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    std::cout << "========================================================\n";
    std::cout << "GodXplr Benchmark Harness (Spec 03 U0 Baseline Runner)  \n";
    std::cout << "========================================================\n";

    MetricsRecorder recorder;
    auto env = MetricsRecorder::detectEnvironment();
    std::cout << "Environment:\n"
              << "  OS: " << env.osBuild << "\n"
              << "  CPU: " << env.cpu << " (" << env.cpuCores << " logical cores)\n"
              << "  RAM: " << env.ramGb << " GB\n"
              << "  Storage: " << env.storage << "\n\n";

    // ---------------------------------------------------------
    // Scenario 1: 10,000 items Dataset Generation and Indexing
    // ---------------------------------------------------------
    std::cout << "Running Scenario 1: dataset-generation-10k...\n";
    GeneratorConfig cfg10k{42, 10000, 0.08, 6};
    DatasetGenerator gen10k(cfg10k);
    DatasetManifest manifest10k;
    recorder.reset();

    constexpr size_t kSamples10k = 10;
    for (size_t s = 0; s < kSamples10k; ++s) {
        auto t0 = std::chrono::high_resolution_clock::now();
        NameIndex idx;
        manifest10k = gen10k.populateIndex(idx);
        auto t1 = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
        recorder.recordSample(ms);
    }
    recorder.sampleCurrentProcessMemory();
    auto stats10k = recorder.computeStatistics();
    std::cout << "  10k entries: p50=" << stats10k.p50Ms << " ms, p95="
              << stats10k.p95Ms << " ms, max=" << stats10k.maxMs << " ms\n";

    // ---------------------------------------------------------
    // Scenario 2: 100,000 items Dataset Generation and Indexing
    // ---------------------------------------------------------
    std::cout << "Running Scenario 2: dataset-generation-100k...\n";
    GeneratorConfig cfg100k{42, 100000, 0.08, 6};
    DatasetGenerator gen100k(cfg100k);
    DatasetManifest manifest100k;
    recorder.reset();

    NameIndex index100k;
    constexpr size_t kSamples100k = 10;
    for (size_t s = 0; s < kSamples100k; ++s) {
        NameIndex tempIndex;
        auto t0 = std::chrono::high_resolution_clock::now();
        manifest100k = gen100k.populateIndex(tempIndex);
        auto t1 = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
        recorder.recordSample(ms);
        if (s == 0) {
            index100k = std::move(tempIndex);
        }
    }
    recorder.sampleCurrentProcessMemory();
    auto stats100k = recorder.computeStatistics();
    std::cout << "  100k entries: p50=" << stats100k.p50Ms << " ms, p95="
              << stats100k.p95Ms << " ms, max=" << stats100k.maxMs << " ms\n";
    std::cout << "  Manifest Hash: " << manifest100k.manifestHash << "\n";

    // Write baseline JSON report
    std::string jsonOutput = recorder.toJson("dataset-generation-100k", manifest100k);
    std::ofstream jsonFile("docs/benchmarks/baseline-synthetic-u0.json");
    if (jsonFile) {
        jsonFile << jsonOutput;
        jsonFile.close();
        std::cout << "  Written JSON report to docs/benchmarks/baseline-synthetic-u0.json\n";
    }

    // ---------------------------------------------------------
    // Scenario 3: Sorting 100,000 entries by Name
    // ---------------------------------------------------------
    std::cout << "Running Scenario 3: sort-by-name-100k...\n";
    recorder.reset();
    std::vector<EntryId> rowIds(index100k.size());
    std::iota(rowIds.begin(), rowIds.end(), 0);

    constexpr size_t kSortSamples = 20;
    for (size_t s = 0; s < kSortSamples; ++s) {
        std::vector<EntryId> copy = rowIds;
        auto t0 = std::chrono::high_resolution_clock::now();
        std::sort(copy.begin(), copy.end(), [&](EntryId a, EntryId b) {
            return index100k.getName(a) < index100k.getName(b);
        });
        auto t1 = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
        recorder.recordSample(ms);
    }
    recorder.sampleCurrentProcessMemory();
    auto sortStats = recorder.computeStatistics();
    std::cout << "  Sort 100k by name: p50=" << sortStats.p50Ms << " ms, p95="
              << sortStats.p95Ms << " ms, max=" << sortStats.maxMs << " ms\n";

    // ---------------------------------------------------------
    // Scenario 4: AVX2 SIMD Search over 100,000 entries
    // ---------------------------------------------------------
    std::cout << "Running Scenario 4: simd-search-query-100k...\n";
    SearchEngine engine(index100k);
    recorder.reset();

    constexpr size_t kSearchSamples = 100;
    QueryOptions qOpts;
    qOpts.offset = 0;
    qOpts.count = 100;

    for (size_t s = 0; s < kSearchSamples; ++s) {
        auto t0 = std::chrono::high_resolution_clock::now();
        auto page = engine.search("report", qOpts);
        auto t1 = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
        recorder.recordSample(ms);
        (void)page;
    }
    auto searchStats = recorder.computeStatistics();
    std::cout << "  Search 100k entries ('report'): p50=" << (searchStats.p50Ms * 1000.0)
              << " µs, p95=" << (searchStats.p95Ms * 1000.0) << " µs\n";

    std::cout << "\n========================================================\n";
    std::cout << "All U0 benchmark scenarios completed successfully!\n";
    std::cout << "========================================================\n";

    return 0;
}
