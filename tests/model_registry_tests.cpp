#include "syj/core/model_registry.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace {

namespace fs = std::filesystem;

void test_tiers() {
    constexpr std::uint64_t MiB = 1024ULL * 1024ULL;
    assert(syj::core::classify_memory_tier(1ULL * MiB) ==
           syj::core::MemoryTier::edge);
    assert(syj::core::classify_memory_tier(1536ULL * MiB) ==
           syj::core::MemoryTier::edge);
    assert(syj::core::classify_memory_tier(1536ULL * MiB + 1ULL) ==
           syj::core::MemoryTier::standard);
    assert(syj::core::classify_memory_tier(2560ULL * MiB) ==
           syj::core::MemoryTier::standard);
    assert(syj::core::classify_memory_tier(2560ULL * MiB + 1ULL) ==
           syj::core::MemoryTier::large);
    assert(syj::core::classify_memory_tier(4096ULL * MiB) ==
           syj::core::MemoryTier::large);
    assert(syj::core::classify_memory_tier(4096ULL * MiB + 1ULL) ==
           syj::core::MemoryTier::above_large);
}

void test_manifest_round_trip(const fs::path &root) {
    const fs::path manifest = root / "registry.json";

    syj::core::RegistryConfig config;
    config.manifest_path = manifest.string();
    config.models_directory = (root / "models").string();

    syj::core::ModelRegistry registry(config);

    syj::core::ModelEntry entry;
    entry.name = "fixture-model";
    entry.architecture = "llama";
    entry.parameter_count = 135000000ULL;
    entry.quantization = "Q4_K_M";
    entry.file_size_bytes = 101ULL * 1024ULL * 1024ULL;
    entry.expected_ram_tier = syj::core::MemoryTier::edge;
    entry.context_support = 2048;
    entry.local_path = (root / "models" / "fixture.gguf").string();
    entry.estimated_required_bytes = 160ULL * 1024ULL * 1024ULL;
    entry.metadata_complete = true;

    assert(registry.register_model(entry, false));
    assert(registry.save_manifest());

    syj::core::ModelRegistry loaded(config);
    assert(loaded.load_manifest());

    syj::core::ModelEntry recovered;
    assert(loaded.get_model("fixture-model", recovered));
    assert(recovered.architecture == "llama");
    assert(recovered.parameter_count == 135000000ULL);
    assert(recovered.quantization == "Q4_K_M");
    assert(recovered.expected_ram_tier == syj::core::MemoryTier::edge);
    assert(recovered.context_support == 2048);
}

void test_directory_scan_fixture(const fs::path &root) {
    const fs::path models = root / "scan";
    fs::create_directories(models);

    {
        std::ofstream non_model(models / "ignore.txt");
        non_model << "not a model\n";
    }

    {
        std::ofstream fake(models / "invalid.gguf", std::ios::binary);
        fake.write("GGUF", 4);
    }

    syj::core::RegistryConfig config;
    config.manifest_path = (root / "scan-registry.json").string();

    syj::core::ModelRegistry registry(config);
    const auto status = registry.scan_directory(models.string());

    // The scanner must recognize the GGUF extension/header but must not
    // register a file whose metadata cannot be inspected safely.
    assert(status.code == syj::core::StatusCode::model_not_found);
    assert(registry.list_models().empty());
}

} // namespace

int main() {
    const fs::path root =
        fs::temp_directory_path() / "syj_model_registry_tests";

    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(root, ec);
    assert(!ec);

    test_tiers();
    test_manifest_round_trip(root);
    test_directory_scan_fixture(root);

    fs::remove_all(root, ec);

    std::cout << "Phase 3 model registry tests passed\n";
    return 0;
}
