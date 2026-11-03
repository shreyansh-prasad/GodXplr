#include "godxplr/core/name_index.h"
#include "godxplr/core/simd_search.h"
#include <chrono>
#include <iomanip>
#include <iostream>
#include <vector>

void run_benchmark(size_t fileCount) {
    std::cout << "\n======================================================\n";
    std::cout << "  BENCHMARK: " << fileCount << " Files Indexed in NameIndex SoA\n";
    std::cout << "======================================================\n";

    gx::NameIndex index;
    index.reserve(fileCount, fileCount * 28);

    auto tStart = std::chrono::high_resolution_clock::now();

    // Populate with realistic synthetic paths
    const char* exts[] = {".cpp", ".h", ".txt", ".json", ".dll", ".exe", ".pdf", ".docx", ".zip", ".png"};
    for (size_t i = 0; i < fileCount; ++i) {
        std::string name = "document_node_" + std::to_string(i) + exts[i % 10];
        uint64_t parentFrn = (i > 10) ? (i % 100) : 5;
        index.add(i + 1000, parentFrn, name, 0, 1024 + (i * 17) % 500000, 13350000000ULL + i);
    }

    auto tPopulated = std::chrono::high_resolution_clock::now();
    double popMs = std::chrono::duration<double, std::milli>(tPopulated - tStart).count();

    size_t memBytes = index.totalMemoryBytes();
    double memMb = static_cast<double>(memBytes) / (1024.0 * 1024.0);
    double bytesPerEntry = static_cast<double>(memBytes) / static_cast<double>(fileCount);

    std::cout << "Index Population Time : " << std::fixed << std::setprecision(2) << popMs << " ms\n";
    std::cout << "Total Resident Memory : " << memMb << " MB\n";
    std::cout << "Memory Cost per Entry : " << bytesPerEntry << " bytes/entry\n";
    std::cout << "Target Contract Check : " << (bytesPerEntry <= 120.0 ? "PASS (<= 120 B)" : "FAIL") << "\n";

    // Measure AVX2 SIMD Search Latency across queries
    const char* queries[] = {"node_999", "document_node", ".pdf", "5555", "cpp"};
    std::vector<gx::EntryId> hits;
    hits.reserve(1000);

    std::cout << "\nSearch Queries Latency (AVX2 SIMD Scan):\n";
    for (const char* q : queries) {
        hits.clear();
        auto qStart = std::chrono::high_resolution_clock::now();
        size_t count = gx::search::scanArenaAVX2(index, q, hits, 100);
        auto qEnd = std::chrono::high_resolution_clock::now();

        double qMicro = std::chrono::duration<double, std::micro>(qEnd - qStart).count();
        std::cout << "  Query: \"" << std::setw(15) << std::left << q << "\" -> "
                  << std::setw(6) << count << " hits in "
                  << std::setw(8) << qMicro << " us ("
                  << (qMicro / 1000.0) << " ms)\n";
    }
}

int main() {
    std::cout << "GodXplr NameIndex Memory & SIMD Search Benchmark\n";
    run_benchmark(100000);
    run_benchmark(1000000);
    return 0;
}
