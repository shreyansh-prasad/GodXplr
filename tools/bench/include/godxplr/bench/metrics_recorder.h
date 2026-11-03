#pragma once

#include "godxplr/bench/dataset_generator.h"
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace gx::bench {

struct EnvironmentMetadata {
    std::string appCommit{"unknown"};
    std::string compiler{"MSVC"};
    std::string qtVersion{"None (Headless)"};
    std::string osBuild{"Windows 11 Build 26200"};
    std::string cpu{"Unknown CPU"};
    uint32_t cpuCores{1};
    double ramGb{8.0};
    std::string storage{"SSD (NTFS)"};
    std::string display{"1920x1080@60Hz"};
    double dpiScale{1.0};
};

struct BenchmarkStatistics {
    size_t samplesCount{0};
    double p50Ms{0.0};
    double p90Ms{0.0};
    double p95Ms{0.0};
    double p99Ms{0.0};
    double maxMs{0.0};
    double minMs{0.0};
    double meanMs{0.0};
    double stddevMs{0.0};
    double workingSetMb{0.0};
    double privateBytesMb{0.0};
};

class MetricsRecorder {
public:
    MetricsRecorder();

    // Captures hardware, compiler and operating system metadata
    static EnvironmentMetadata detectEnvironment();

    // Records an execution latency sample in milliseconds
    void recordSample(double ms);

    // Records process memory footprint
    void recordMemory(size_t workingSetBytes, size_t privateBytes);

    // Automatically queries current process memory via OS APIs
    void sampleCurrentProcessMemory();

    // Computes percentiles, min, max, mean, and standard deviation
    [[nodiscard]] BenchmarkStatistics computeStatistics() const;

    // Formats result into JSON conforming to benchmark-v1.schema.json
    [[nodiscard]] std::string toJson(std::string_view scenario,
                                     const DatasetManifest& manifest) const;

    // Formats Markdown summary row for documentation
    [[nodiscard]] std::string toMarkdown(std::string_view scenario,
                                         const DatasetManifest& manifest) const;

    // Resets recorded samples
    void reset();

private:
    std::vector<double> samples_;
    double lastWorkingSetMb_{0.0};
    double lastPrivateBytesMb_{0.0};
    EnvironmentMetadata env_;
};

} // namespace gx::bench
