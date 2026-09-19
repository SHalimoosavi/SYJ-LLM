#include "syj/core/runtime.hpp"

#include <cassert>
#include <iostream>
#include <string>

int main() {
    using syj::core::Runtime;
    using syj::core::RuntimeConfig;
    using syj::core::StatusCode;

    /*
     * Default low-RAM configuration.
     */
    {
        const RuntimeConfig config;

        assert(config.context_size == 1024);
        assert(config.batch_size == 256);
        assert(config.threads == 2);
        assert(config.batch_threads == 2);
        assert(config.max_output_tokens == 128);
        assert(config.use_mmap);
        assert(config.validate_tensors);
    }

    /*
     * Runtime starts unloaded.
     */
    {
        Runtime runtime;

        assert(!runtime.loaded());
        assert(runtime.context_size() == 0);
        assert(runtime.model_bytes() == 0);
    }

    /*
     * Missing model must be distinguished from a malformed model.
     */
    {
        Runtime runtime;

        RuntimeConfig config;

        const auto status =
            runtime.load_model(
                "/definitely/missing/syj-model.gguf",
                config);

        assert(status.code == StatusCode::model_not_found);
        assert(!runtime.loaded());
    }

    /*
     * Invalid context configuration must be rejected before
     * attempting to open/load the model.
     */
    {
        Runtime runtime;

        RuntimeConfig config;
        config.context_size = 0;

        const auto status =
            runtime.load_model(
                "/definitely/missing/syj-model.gguf",
                config);

        assert(status.code == StatusCode::invalid_argument);
        assert(!runtime.loaded());
    }

    /*
     * Invalid batch configuration.
     */
    {
        Runtime runtime;

        RuntimeConfig config;
        config.batch_size = 0;

        const auto status =
            runtime.load_model(
                "/definitely/missing/syj-model.gguf",
                config);

        assert(status.code == StatusCode::invalid_argument);
        assert(!runtime.loaded());
    }

    /*
     * Invalid thread configuration.
     */
    {
        Runtime runtime;

        RuntimeConfig config;
        config.threads = 0;

        const auto status =
            runtime.load_model(
                "/definitely/missing/syj-model.gguf",
                config);

        assert(status.code == StatusCode::invalid_argument);
        assert(!runtime.loaded());
    }

    /*
     * Output budget must leave room for the prompt.
     */
    {
        Runtime runtime;

        RuntimeConfig config;
        config.context_size = 1024;
        config.max_output_tokens = 1024;

        const auto status =
            runtime.load_model(
                "/definitely/missing/syj-model.gguf",
                config);

        assert(status.code == StatusCode::invalid_argument);
        assert(!runtime.loaded());
    }

    /*
     * Generation without a model must fail cleanly.
     */
    {
        Runtime runtime;

        const auto status =
            runtime.generate(
                "hello",
                [](std::string_view) {
                    return true;
                });

        assert(status.code == StatusCode::model_load_failed);
    }

    /*
     * Empty callback must be rejected before generation.
     */
    {
        Runtime runtime;

        const auto status =
            runtime.generate(
                "hello",
                {});

        assert(status.code == StatusCode::model_load_failed);
    }

    /*
     * Explicit unload must be safe even when already unloaded.
     */
    {
        Runtime runtime;

        runtime.unload();
        runtime.unload();

        assert(!runtime.loaded());
    }

    std::cout << "Phase 1 core tests passed\n";
    return 0;
}
