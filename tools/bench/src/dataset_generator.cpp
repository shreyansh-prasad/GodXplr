#include "godxplr/bench/dataset_generator.h"
#include <random>
#include <algorithm>
#include <iomanip>
#include <sstream>

namespace gx::bench {

namespace {

const std::vector<std::string> kDirStems = {
    "Documents", "Pictures", "Projects", "Source", "Build", "Cache",
    "AppData", "System32", "Downloads", "Logs", "Archives", "Config",
    "Modules", "Core", "Tests", "Assets", "Vendor", "Shared", "Include"
};

const std::vector<std::string> kFileStems = {
    "report", "document", "summary", "presentation", "budget", "invoice",
    "setup", "kernel", "database", "user_profile", "analytics", "test_case",
    "readme", "license", "changelog", "main", "config", "settings", "index",
    "backup", "archive", "build_output", "package", "manifest", "runtime",
    "café_notes", "résumé_2026", "über_projekt", "überblick", "façade_render",
    "データ分析", "プロジェクト仕様書", "システムログ", "財務報告書"
};

const std::vector<std::string> kDocExts = {
    ".pdf", ".docx", ".xlsx", ".pptx", ".txt", ".md", ".csv"
};

const std::vector<std::string> kCodeExts = {
    ".cpp", ".h", ".hpp", ".py", ".js", ".ts", ".json", ".xml", ".html", ".css"
};

const std::vector<std::string> kMediaExts = {
    ".png", ".jpg", ".jpeg", ".svg", ".mp4", ".mp3", ".wav"
};

const std::vector<std::string> kBinExts = {
    ".exe", ".dll", ".sys", ".zip", ".tar.gz", ".7z"
};

const std::vector<std::string> kSystemExts = {
    ".ini", ".log", ".dat", ".tmp"
};

// 64-bit FNV-1a Hash
uint64_t fnv1a64(const void* data, size_t len, uint64_t hash = 0xCBF29CE484222325ULL) {
    const auto* p = static_cast<const uint8_t*>(data);
    for (size_t i = 0; i < len; ++i) {
        hash ^= p[i];
        hash *= 0x100000001B3ULL;
    }
    return hash;
}

} // namespace

DatasetGenerator::DatasetGenerator(GeneratorConfig config)
    : config_(config) {}

std::string DatasetGenerator::computeManifestHash(const std::vector<SyntheticEntry>& entries) {
    uint64_t h1 = 0xCBF29CE484222325ULL;
    uint64_t h2 = 0x84222325CBF29CE4ULL;

    for (const auto& e : entries) {
        h1 = fnv1a64(&e.frn, sizeof(e.frn), h1);
        h1 = fnv1a64(&e.parentFrn, sizeof(e.parentFrn), h1);
        h1 = fnv1a64(e.name.data(), e.name.size(), h1);
        h1 = fnv1a64(&e.flags, sizeof(e.flags), h1);
        h1 = fnv1a64(&e.size, sizeof(e.size), h1);
        h1 = fnv1a64(&e.mtime, sizeof(e.mtime), h1);

        h2 = fnv1a64(&e.size, sizeof(e.size), h2);
        h2 = fnv1a64(e.name.data(), e.name.size(), h2);
    }

    std::ostringstream oss;
    oss << std::hex << std::setfill('0')
        << std::setw(16) << h1
        << std::setw(16) << h2;
    return oss.str();
}

std::vector<SyntheticEntry> DatasetGenerator::generate(DatasetManifest& outManifest) {
    std::mt19937_64 rng(config_.seed);

    size_t targetDirs = static_cast<size_t>(config_.totalEntries * config_.directoryRatio);
    if (targetDirs < 5) targetDirs = 5;
    size_t targetFiles = config_.totalEntries > targetDirs ? (config_.totalEntries - targetDirs) : 0;

    std::vector<SyntheticEntry> entries;
    entries.reserve(config_.totalEntries);

    // Directory tracking: vector of directory FRNs
    std::vector<uint64_t> dirFrns;
    dirFrns.reserve(targetDirs + 1);

    // Root directory (FRN 5 in NTFS is the root folder)
    uint64_t rootFrn = 5;
    dirFrns.push_back(rootFrn);

    uint64_t nextFrn = 1000;
    uint64_t baseTime = 133800000000000000ULL; // ~2026-01-01 FILETIME

    // Step 1: Generate directories
    for (size_t i = 0; i < targetDirs; ++i) {
        uint64_t frn = nextFrn++;
        // Choose parent from existing directories (bounded depth)
        size_t parentIdx = static_cast<size_t>(rng() % dirFrns.size());
        uint64_t parentFrn = dirFrns[parentIdx];

        std::string stem = kDirStems[rng() % kDirStems.size()];
        uint32_t suffix = static_cast<uint32_t>(rng() % 1000);
        std::string name = stem + "_" + std::to_string(suffix);

        uint8_t flags = kDir;
        if ((rng() % 100) < 5) {
            flags |= kHidden;
        }

        uint64_t mtime = baseTime - (rng() % (10000000ULL * 86400 * 365));

        entries.push_back(SyntheticEntry{frn, parentFrn, name, flags, 0, mtime});
        dirFrns.push_back(frn);
    }

    // Step 2: Generate files
    uint64_t totalBytes = 0;
    for (size_t i = 0; i < targetFiles; ++i) {
        uint64_t frn = nextFrn++;
        size_t parentIdx = static_cast<size_t>(rng() % dirFrns.size());
        uint64_t parentFrn = dirFrns[parentIdx];

        std::string stem = kFileStems[rng() % kFileStems.size()];
        uint32_t suffix = static_cast<uint32_t>(rng() % 10000);

        // Pick extension category
        uint32_t cat = static_cast<uint32_t>(rng() % 100);
        std::string ext;
        if (cat < 30) {
            ext = kDocExts[rng() % kDocExts.size()];
        } else if (cat < 60) {
            ext = kCodeExts[rng() % kCodeExts.size()];
        } else if (cat < 80) {
            ext = kMediaExts[rng() % kMediaExts.size()];
        } else if (cat < 95) {
            ext = kBinExts[rng() % kBinExts.size()];
        } else {
            ext = kSystemExts[rng() % kSystemExts.size()];
        }

        std::string name = stem + "_" + std::to_string(suffix) + ext;

        // File size distribution (Pareto / log-normal)
        uint64_t size = 0;
        uint32_t sizeCat = static_cast<uint32_t>(rng() % 100);
        if (sizeCat < 50) {
            size = static_cast<uint64_t>(rng() % 10240); // 0 to 10 KB
        } else if (sizeCat < 85) {
            size = static_cast<uint64_t>(10240 + (rng() % (5 * 1024 * 1024))); // 10 KB to 5 MB
        } else if (sizeCat < 98) {
            size = static_cast<uint64_t>(5 * 1024 * 1024 + (rng() % (100 * 1024 * 1024))); // 5 to 100 MB
        } else {
            size = static_cast<uint64_t>(100 * 1024 * 1024 + (rng() % (1024ULL * 1024 * 1024))); // 100 MB to 1 GB
        }

        uint8_t flags = 0;
        if ((rng() % 100) < 4) {
            flags |= kHidden;
        }

        uint64_t mtime = baseTime - (rng() % (10000000ULL * 86400 * 365));

        entries.push_back(SyntheticEntry{frn, parentFrn, name, flags, size, mtime});
        totalBytes += size;
    }

    outManifest.seed = config_.seed;
    outManifest.entryCount = entries.size();
    outManifest.fileCount = targetFiles;
    outManifest.directoryCount = targetDirs;
    outManifest.totalBytes = totalBytes;
    outManifest.manifestHash = computeManifestHash(entries);

    return entries;
}

DatasetManifest DatasetGenerator::populateIndex(NameIndex& index) {
    DatasetManifest manifest;
    auto entries = generate(manifest);

    index.reserve(entries.size(), entries.size() * 32);
    for (const auto& e : entries) {
        index.add(e.frn, e.parentFrn, e.name, e.flags, e.size, e.mtime);
    }
    index.resolveParents();

    return manifest;
}

} // namespace gx::bench
