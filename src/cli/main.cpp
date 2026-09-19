#include "syj/core/runtime.hpp"
#include "syj/core/version.hpp"

#include <iostream>
#include <string_view>

int main(int argc, char **argv) {
    std::cout
        << "SYJ LLM runtime\n"
        << "Version: " << syj::core::version() << "\n"
        << "Vendor: " << syj::core::vendor() << "\n"
        << "Stage: Phase 1 - Core Inference Runtime\n";

    if (argc < 3) {
        std::cerr
            << "Usage: syj <model.gguf> <prompt>\n";
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

    const auto load_status =
        runtime.load_model(argv[1], config);

    if (!load_status) {
        std::cerr
            << "ERROR: "
            << load_status.message
            << "\n";

        return 3;
    }

    std::cout
        << "Context: "
        << runtime.context_size()
        << " tokens\n"
        << "Model bytes: "
        << runtime.model_bytes()
        << "\n"
        << "Generation: ";

    const auto generation_status =
        runtime.generate(
            argv[2],
            [](std::string_view piece) {
                std::cout
                    << piece
                    << std::flush;

                return true;
            });

    std::cout
        << "\nPeak RSS: "
        << runtime.peak_rss_bytes()
        << " bytes\n";

    if (!generation_status) {
        std::cerr
            << "ERROR: "
            << generation_status.message
            << "\n";

        return 4;
    }

    return 0;
}
