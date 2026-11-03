#include "godxplr/bench/dataset_generator.h"
#include <iostream>
#include <string_view>

using namespace gx::bench;

int main(int argc, char* argv[]) {
    uint64_t seed = 42;
    size_t count = 10000;

    for (int i = 1; i < argc; ++i) {
        std::string_view arg(argv[i]);
        if (arg == "--count" && i + 1 < argc) {
            count = std::stoull(argv[++i]);
        } else if (arg == "--seed" && i + 1 < argc) {
            seed = std::stoull(argv[++i]);
        }
    }

    std::cout << "GodXplr Synthetic Dataset Generator\n";
    std::cout << "  Generating " << count << " entries with seed " << seed << "...\n";

    GeneratorConfig config{seed, count, 0.08, 6};
    DatasetGenerator gen(config);
    DatasetManifest manifest;

    auto entries = gen.generate(manifest);
    std::cout << "Generated successfully!\n";
    std::cout << "  Total Entries: " << manifest.entryCount << "\n";
    std::cout << "  Files:         " << manifest.fileCount << "\n";
    std::cout << "  Directories:   " << manifest.directoryCount << "\n";
    std::cout << "  Total Bytes:   " << manifest.totalBytes << " ("
              << (manifest.totalBytes / 1024 / 1024) << " MB)\n";
    std::cout << "  Manifest Hash: " << manifest.manifestHash << "\n";

    return 0;
}

