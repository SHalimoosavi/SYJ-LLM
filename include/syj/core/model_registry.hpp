#pragma once

#include "syj/core/runtime.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace syj::core {

enum class MemoryTier {
    edge,
    standard,
    large,
    above_large,
    unknown
};

const char *memory_tier_name(MemoryTier tier) noexcept;
MemoryTier classify_memory_tier(std::uint64_t required_bytes) noexcept;

struct ModelEntry {
    std::string name;
    std::string architecture;
    std::uint64_t parameter_count = 0;
    std::string quantization;
    std::uint64_t file_size_bytes = 0;
    MemoryTier expected_ram_tier = MemoryTier::unknown;
    std::uint32_t context_support = 0;
    std::string local_path;
    std::uint64_t estimated_required_bytes = 0;
    bool metadata_complete = false;
};

struct RegistryConfig {
    std::string manifest_path = "models/registry.json";
    std::string models_directory = "models";
};

struct ResolvedModel {
    ModelEntry entry;
    MemoryEstimate memory_estimate{};
};

class ModelRegistry final {
public:
    explicit ModelRegistry(RegistryConfig config = {});

    Status load_manifest();
    Status save_manifest() const;

    // Scans only local files. No network access is performed.
    Status scan_directory(const std::string &directory,
                          const RuntimeConfig &runtime_config = {});

    // Manual registration for metadata that cannot be safely inferred.
    Status register_model(const ModelEntry &entry, bool overwrite = true);

    Status get_model(const std::string &name, ModelEntry &out) const;
    std::vector<ModelEntry> list_models() const;

    // Resolves a registered model and performs the Phase 2 metadata-only
    // memory preflight through Runtime::estimate_memory().
    Status resolve(const std::string &name,
                   const RuntimeConfig &runtime_config,
                   ResolvedModel &out) const;

    // Returns only models whose fresh Phase 2 estimate fits the supplied
    // hard budget. No model weights are loaded.
    Status list_models_fitting_budget(
        std::uint64_t max_bytes,
        const RuntimeConfig &runtime_config,
        std::vector<ModelEntry> &out) const;

    const RegistryConfig &config() const noexcept;

private:
    RegistryConfig config_;
    std::vector<ModelEntry> entries_;
};

} // namespace syj::core
