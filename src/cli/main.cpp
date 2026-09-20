#include "syj/core/model_registry.hpp"
#include "syj/core/runtime.hpp"
#include "syj/core/version.hpp"

#include <iostream>
#include <string>
#include <string_view>

namespace {

void print_model(const syj::core::ModelEntry &model) {
    std::cout
        << "Name: " << model.name << '\n'
        << "Architecture: " << model.architecture << '\n'
        << "Parameters: " << model.parameter_count << '\n'
        << "Quantization: " << model.quantization << '\n'
        << "File bytes: " << model.file_size_bytes << '\n'
        << "RAM tier: "
        << syj::core::memory_tier_name(model.expected_ram_tier) << '\n'
        << "Context support: " << model.context_support << '\n'
        << "Estimated required bytes: "
        << model.estimated_required_bytes << '\n'
        << "Local path: " << model.local_path << '\n';
}

int registry_main(int argc, char **argv) {
    syj::core::ModelRegistry registry;

    if (argc < 3) {
        std::cerr
            << "Usage:\n"
            << "  syj model list\n"
            << "  syj model show <name>\n"
            << "  syj model scan <directory>\n";
        return 2;
    }

    const std::string command(argv[2]);

    if (command == "list") {
        const auto status = registry.load_manifest();
        if (!status &&
            status.code != syj::core::StatusCode::model_not_found) {
            std::cerr << "ERROR: " << status.message << '\n';
            return 3;
        }

        for (const auto &model : registry.list_models()) {
            std::cout
                << model.name << " | "
                << model.architecture << " | "
                << syj::core::memory_tier_name(model.expected_ram_tier)
                << " | " << model.local_path << '\n';
        }
        return 0;
    }

    if (command == "show") {
        if (argc < 4) {
            std::cerr << "Usage: syj model show <name>\n";
            return 2;
        }
        const auto load = registry.load_manifest();
        if (!load) {
            std::cerr << "ERROR: " << load.message << '\n';
            return 3;
        }

        syj::core::ModelEntry model;
        const auto status = registry.get_model(argv[3], model);
        if (!status) {
            std::cerr << "ERROR: " << status.message << '\n';
            return 4;
        }
        print_model(model);
        return 0;
    }

    if (command == "scan") {
        if (argc < 4) {
            std::cerr << "Usage: syj model scan <directory>\n";
            return 2;
        }

        const auto existing = registry.load_manifest();
        if (!existing &&
            existing.code != syj::core::StatusCode::model_not_found) {
            std::cerr << "ERROR: " << existing.message << '\n';
            return 3;
        }

        const auto status = registry.scan_directory(argv[3]);
        if (!status) {
            std::cerr << "ERROR: " << status.message << '\n';
            return 4;
        }

        const auto save = registry.save_manifest();
        if (!save) {
            std::cerr << "ERROR: " << save.message << '\n';
            return 5;
        }

        std::cout
            << "Registered models: "
            << registry.list_models().size()
            << '\n';
        return 0;
    }

    std::cerr << "Unknown model command: " << command << '\n';
    return 2;
}

} // namespace

int main(int argc, char **argv) {
    std::cout
        << "SYJ LLM runtime\n"
        << "Version: " << syj::core::version() << "\n"
        << "Vendor: " << syj::core::vendor() << "\n"
        << "Stage: Phase 3 - Model Registry\n";

    if (argc >= 2 && std::string_view(argv[1]) == "model") {
        return registry_main(argc, argv);
    }

    if (argc < 3) {
        std::cerr
            << "Usage: syj <model.gguf> <prompt>\n"
            << "       syj model <list|show|scan> ...\n";
        return 2;
    }

    syj::core::RuntimeConfig config;
    config.context_size = 1024;
    config.batch_size = 256;
    config.threads = 2;
    config.batch_threads = 2;
    config.max_output_tokens = 64;
    config.use_mmap = true;
    config.validate_tensors = true;

    syj::core::Runtime runtime;
    const auto load_status = runtime.load_model(argv[1], config);

    if (!load_status) {
        std::cerr << "ERROR: " << load_status.message << '\n';
        return 3;
    }

    std::cout
        << "Context: " << runtime.context_size() << " tokens\n"
        << "Model bytes: " << runtime.model_bytes() << "\n"
        << "Generation: ";

    const auto generation_status =
        runtime.generate(
            argv[2],
            [](std::string_view piece) {
                std::cout << piece << std::flush;
                return true;
            });

    std::cout
        << "\nPeak RSS: "
        << runtime.peak_rss_bytes()
        << " bytes\n";

    if (!generation_status) {
        std::cerr << "ERROR: " << generation_status.message << '\n';
        return 4;
    }

    return 0;
}
