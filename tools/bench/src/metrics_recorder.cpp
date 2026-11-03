#include "godxplr/bench/metrics_recorder.h"
#include <algorithm>
#include <numeric>
#include <cmath>
#include <sstream>
#include <iomanip>
#include <chrono>

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#endif

namespace gx::bench {

MetricsRecorder::MetricsRecorder() {
    env_ = detectEnvironment();
}

EnvironmentMetadata MetricsRecorder::detectEnvironment() {
    EnvironmentMetadata env;
    env.appCommit = "0d06e67"; // Current baseline commit
#ifdef _MSC_VER
    env.compiler = "MSVC " + std::to_string(_MSC_VER);
#else
    env.compiler = "Clang/GCC";
#endif

#ifdef _WIN32
    // Query CPU Name from Registry
    HKEY hKey;
    if (::RegOpenKeyExW(HKEY_LOCAL_MACHINE,
                        L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
                        0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        WCHAR cpuBuf[256]{};
        DWORD bufSize = sizeof(cpuBuf);
        if (::RegQueryValueExW(hKey, L"ProcessorNameString", nullptr, nullptr,
                               reinterpret_cast<LPBYTE>(cpuBuf), &bufSize) == ERROR_SUCCESS) {
            char cpuNarrow[256]{};
            ::WideCharToMultiByte(CP_UTF8, 0, cpuBuf, -1, cpuNarrow, sizeof(cpuNarrow), nullptr, nullptr);
            env.cpu = cpuNarrow;
            // Trim leading/trailing whitespace
            size_t first = env.cpu.find_first_not_of(" \t\r\n");
            size_t last = env.cpu.find_last_not_of(" \t\r\n");
            if (first != std::string::npos && last != std::string::npos) {
                env.cpu = env.cpu.substr(first, (last - first + 1));
            }
        }
        ::RegCloseKey(hKey);
    }

    SYSTEM_INFO sysInfo{};
    ::GetSystemInfo(&sysInfo);
    env.cpuCores = sysInfo.dwNumberOfProcessors;

    MEMORYSTATUSEX memStatus{};
    memStatus.dwLength = sizeof(memStatus);
    if (::GlobalMemoryStatusEx(&memStatus)) {
        env.ramGb = static_cast<double>(memStatus.ullTotalPhys) / (1024.0 * 1024.0 * 1024.0);
    }

    env.osBuild = "Windows 11 Build 26200";
    env.storage = "NVMe SSD (NTFS)";

    int cx = ::GetSystemMetrics(SM_CXSCREEN);
    int cy = ::GetSystemMetrics(SM_CYSCREEN);
    env.display = std::to_string(cx) + "x" + std::to_string(cy) + "@60Hz";

    UINT dpi = ::GetDpiForSystem();
    env.dpiScale = static_cast<double>(dpi) / 96.0;
#endif

    return env;
}

void MetricsRecorder::recordSample(double ms) {
    samples_.push_back(ms);
}

void MetricsRecorder::recordMemory(size_t workingSetBytes, size_t privateBytes) {
    lastWorkingSetMb_ = static_cast<double>(workingSetBytes) / (1024.0 * 1024.0);
    lastPrivateBytesMb_ = static_cast<double>(privateBytes) / (1024.0 * 1024.0);
}

void MetricsRecorder::sampleCurrentProcessMemory() {
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS_EX pmc{};
    if (::GetProcessMemoryInfo(::GetCurrentProcess(),
                               reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc),
                               sizeof(pmc))) {
        recordMemory(pmc.WorkingSetSize, pmc.PrivateUsage);
    }
#endif
}

BenchmarkStatistics MetricsRecorder::computeStatistics() const {
    BenchmarkStatistics stats;
    if (samples_.empty()) {
        return stats;
    }

    stats.samplesCount = samples_.size();
    std::vector<double> sorted = samples_;
    std::sort(sorted.begin(), sorted.end());

    stats.minMs = sorted.front();
    stats.maxMs = sorted.back();

    auto percentile = [&](double pct) -> double {
        if (sorted.empty()) return 0.0;
        size_t idx = static_cast<size_t>(pct * (sorted.size() - 1));
        return sorted[idx];
    };

    stats.p50Ms = percentile(0.50);
    stats.p90Ms = percentile(0.90);
    stats.p95Ms = percentile(0.95);
    stats.p99Ms = percentile(0.99);

    double sum = std::accumulate(sorted.begin(), sorted.end(), 0.0);
    stats.meanMs = sum / sorted.size();

    double accum = 0.0;
    for (double val : sorted) {
        accum += (val - stats.meanMs) * (val - stats.meanMs);
    }
    stats.stddevMs = std::sqrt(accum / sorted.size());

    stats.workingSetMb = lastWorkingSetMb_;
    stats.privateBytesMb = lastPrivateBytesMb_;

    return stats;
}

void MetricsRecorder::reset() {
    samples_.clear();
}

std::string MetricsRecorder::toJson(std::string_view scenario,
                                    const DatasetManifest& manifest) const {
    auto stats = computeStatistics();

    // Get current UTC ISO timestamp
    auto now = std::chrono::system_clock::now();
    auto tt = std::chrono::system_clock::to_time_t(now);
    std::tm tmUtc{};
#ifdef _WIN32
    gmtime_s(&tmUtc, &tt);
#else
    gmtime_r(&tt, &tmUtc);
#endif
    char timeBuf[64];
    std::strftime(timeBuf, sizeof(timeBuf), "%Y-%m-%dT%H:%M:%SZ", &tmUtc);

    std::ostringstream oss;
    oss << std::fixed << std::setprecision(3);
    oss << "{\n";
    oss << "  \"schema_version\": \"1.0.0\",\n";
    oss << "  \"timestamp_iso\": \"" << timeBuf << "\",\n";
    oss << "  \"environment\": {\n";
    oss << "    \"app_commit\": \"" << env_.appCommit << "\",\n";
    oss << "    \"compiler\": \"" << env_.compiler << "\",\n";
    oss << "    \"qt_version\": \"" << env_.qtVersion << "\",\n";
    oss << "    \"os_build\": \"" << env_.osBuild << "\",\n";
    oss << "    \"cpu\": \"" << env_.cpu << "\",\n";
    oss << "    \"cpu_cores\": " << env_.cpuCores << ",\n";
    oss << "    \"ram_gb\": " << env_.ramGb << ",\n";
    oss << "    \"storage\": \"" << env_.storage << "\",\n";
    oss << "    \"display\": \"" << env_.display << "\",\n";
    oss << "    \"dpi_scale\": " << env_.dpiScale << "\n";
    oss << "  },\n";
    oss << "  \"dataset_manifest\": {\n";
    oss << "    \"seed\": " << manifest.seed << ",\n";
    oss << "    \"entry_count\": " << manifest.entryCount << ",\n";
    oss << "    \"file_count\": " << manifest.fileCount << ",\n";
    oss << "    \"directory_count\": " << manifest.directoryCount << ",\n";
    oss << "    \"total_bytes\": " << manifest.totalBytes << ",\n";
    oss << "    \"manifest_hash\": \"" << manifest.manifestHash << "\"\n";
    oss << "  },\n";
    oss << "  \"scenario\": \"" << scenario << "\",\n";
    oss << "  \"samples\": " << stats.samplesCount << ",\n";
    oss << "  \"metrics\": {\n";
    oss << "    \"p50_ms\": " << stats.p50Ms << ",\n";
    oss << "    \"p90_ms\": " << stats.p90Ms << ",\n";
    oss << "    \"p95_ms\": " << stats.p95Ms << ",\n";
    oss << "    \"p99_ms\": " << stats.p99Ms << ",\n";
    oss << "    \"max_ms\": " << stats.maxMs << ",\n";
    oss << "    \"min_ms\": " << stats.minMs << ",\n";
    oss << "    \"mean_ms\": " << stats.meanMs << ",\n";
    oss << "    \"stddev_ms\": " << stats.stddevMs << ",\n";
    oss << "    \"working_set_mb\": " << stats.workingSetMb << ",\n";
    oss << "    \"private_bytes_mb\": " << stats.privateBytesMb << "\n";
    oss << "  }\n";
    oss << "}\n";

    return oss.str();
}

std::string MetricsRecorder::toMarkdown(std::string_view scenario,
                                        const DatasetManifest& manifest) const {
    auto stats = computeStatistics();
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2);
    oss << "| `" << scenario << "` | " << manifest.entryCount
        << " | " << stats.samplesCount
        << " | " << stats.p50Ms << " ms"
        << " | " << stats.p95Ms << " ms"
        << " | " << stats.p99Ms << " ms"
        << " | " << stats.maxMs << " ms"
        << " | " << stats.workingSetMb << " MB |";
    return oss.str();
}

} // namespace gx::bench

