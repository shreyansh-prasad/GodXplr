#pragma once

#include "godxplr/core/name_index.h"
#include <cstdint>
#include <string>
#include <vector>

namespace gx::bench {

struct SyntheticEntry {
    uint64_t frn{0};
    uint64_t parentFrn{0};
    std::string name;
    uint8_t flags{0};
    uint64_t size{0};
    uint64_t mtime{0};
};

struct DatasetManifest {
    uint64_t seed{0};
    size_t entryCount{0};
    size_t fileCount{0};
    size_t directoryCount{0};
    uint64_t totalBytes{0};
    std::string manifestHash;
};

struct GeneratorConfig {
    uint64_t seed{42};
    size_t totalEntries{100000};
    double directoryRatio{0.08};
    size_t maxDirectoryDepth{6};
};

class DatasetGenerator {
public:
    explicit DatasetGenerator(GeneratorConfig config = {});

    // Generates deterministic entries in memory
    std::vector<SyntheticEntry> generate(DatasetManifest& outManifest);

    // Populates a NameIndex directly with deterministic entries
    DatasetManifest populateIndex(NameIndex& index);

    // Computes deterministic manifest hash (SHA-256 style or CRC32)
    static std::string computeManifestHash(const std::vector<SyntheticEntry>& entries);

private:
    GeneratorConfig config_;
};

} // namespace gx::bench

