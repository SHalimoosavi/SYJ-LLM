#include "syj/core/runtime.hpp"

#include <llama.h>

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

#if defined(__linux__) || defined(__ANDROID__)
#include <sys/resource.h>
#elif defined(__APPLE__)
#include <mach/mach.h>
#endif

namespace syj::core {

namespace {

std::size_t peak_rss() noexcept {
#if defined(__linux__) || defined(__ANDROID__)

    /*
     * Linux/Android:
     *
     * VmHWM is the process peak resident set size.
     * /proc/self/status reports it in KiB.
     */
    {
        std::ifstream status_file("/proc/self/status");

        if (status_file.is_open()) {
            std::string line;

            while (std::getline(status_file, line)) {
                if (line.compare(0, 6, "VmHWM:") != 0) {
                    continue;
                }

                std::istringstream stream(line.substr(6));

                std::uint64_t value_kib = 0;
                std::string unit;

                if (stream >> value_kib >> unit) {
                    if (unit == "kB" ||
                        unit == "KB" ||
                        unit == "KiB") {

                        constexpr std::uint64_t kib = 1024ULL;

                        if (value_kib <=
                            std::numeric_limits<std::uint64_t>::max() /
                                kib) {

                            return static_cast<std::size_t>(
                                value_kib * kib
                            );
                        }
                    }
                }

                break;
            }
        }
    }

    /*
     * Fallback.
     */
    rusage usage{};

    if (getrusage(RUSAGE_SELF, &usage) != 0) {
        return 0;
    }

#if defined(__ANDROID__)
    return static_cast<std::size_t>(
        usage.ru_maxrss
    );
#else
    return static_cast<std::size_t>(
        usage.ru_maxrss
    ) * 1024U;
#endif

#elif defined(__APPLE__)

    mach_task_basic_info info{};

    mach_msg_type_number_t count =
        MACH_TASK_BASIC_INFO_COUNT;

    if (task_info(
            mach_task_self(),
            MACH_TASK_BASIC_INFO,
            reinterpret_cast<task_info_t>(&info),
            &count) != KERN_SUCCESS) {

        return 0;
    }

    return static_cast<std::size_t>(
        info.resident_size_max
    );

#else

    return 0;

#endif
}

Status validate_config(
    const RuntimeConfig &config) {

    if (config.context_size == 0 ||
        config.context_size > 32768U) {

        return {
            StatusCode::invalid_argument,
            "context_size must be between 1 and 32768"
        };
    }

    if (config.batch_size == 0) {
        return {
            StatusCode::invalid_argument,
            "batch_size must be greater than zero"
        };
    }

    if (config.threads <= 0 ||
        config.threads > 8) {

        return {
            StatusCode::invalid_argument,
            "threads must be between 1 and 8"
        };
    }

    if (config.batch_threads <= 0 ||
        config.batch_threads > 8) {

        return {
            StatusCode::invalid_argument,
            "batch_threads must be between 1 and 8"
        };
    }

    if (config.max_output_tokens == 0) {
        return {
            StatusCode::invalid_argument,
            "max_output_tokens must be greater than zero"
        };
    }

    if (config.max_output_tokens >=
        config.context_size) {

        return {
            StatusCode::invalid_argument,
            "max_output_tokens must be smaller than context_size"
        };
    }

    return {
        StatusCode::ok,
        "ok"
    };
}

const char *decode_error_message(
    const int result) noexcept {

    switch (result) {
        case 0:
            return "ok";

        case 1:
            return "no KV cache slot available";

        case 2:
            return "decode aborted";

        case -1:
            return "invalid decode input";

        default:
            return "fatal decode error";
    }
}

} // namespace

struct Runtime::Impl {
    llama_model *model = nullptr;
    llama_context *ctx = nullptr;
    llama_sampler *sampler = nullptr;

    const llama_vocab *vocab = nullptr;

    RuntimeConfig cfg{};

    bool backend_ready = false;
};

Runtime::Runtime()
    : impl_(new Impl{}) {

    llama_backend_init();

    impl_->backend_ready = true;
}

Runtime::~Runtime() {
    unload();

    if (impl_ != nullptr &&
        impl_->backend_ready) {

        llama_backend_free();

        impl_->backend_ready = false;
    }

    delete impl_;
    impl_ = nullptr;
}

Status Runtime::load_model(
    const std::string &path,
    const RuntimeConfig &config) {

    if (impl_ == nullptr) {
        return {
            StatusCode::generation_failed,
            "runtime implementation is unavailable"
        };
    }

    const Status config_status =
        validate_config(config);

    if (!config_status) {
        return config_status;
    }

    unload();

    std::ifstream file(
        path,
        std::ios::binary
    );

    if (!file.good()) {
        return {
            StatusCode::model_not_found,
            "model not found: " + path
        };
    }

    impl_->cfg = config;

    llama_model_params model_params =
        llama_model_default_params();

    /*
     * Phase 1:
     * CPU-first + GGUF mmap.
     */
    model_params.load_mode =
        config.use_mmap
            ? LLAMA_LOAD_MODE_MMAP
            : LLAMA_LOAD_MODE_NONE;

    model_params.check_tensors =
        config.validate_tensors;

    model_params.n_gpu_layers = 0;

    impl_->model =
        llama_model_load_from_file(
            path.c_str(),
            model_params
        );

    if (impl_->model == nullptr) {
        return {
            StatusCode::model_load_failed,
            "llama.cpp could not load GGUF model "
            "(missing, corrupt, unsupported, or insufficient memory)"
        };
    }

    llama_context_params context_params =
        llama_context_default_params();

    context_params.n_ctx =
        config.context_size;

    const uint32_t effective_batch =
        std::min(
            config.batch_size,
            config.context_size
        );

    context_params.n_batch =
        effective_batch;

    context_params.n_ubatch =
        effective_batch;

    context_params.n_threads =
        config.threads;

    context_params.n_threads_batch =
        config.batch_threads;

    context_params.n_seq_max = 1;

    impl_->ctx =
        llama_init_from_model(
            impl_->model,
            context_params
        );

    if (impl_->ctx == nullptr) {
        unload();

        return {
            StatusCode::insufficient_memory,
            "failed to create context; "
            "lower context_size or batch_size"
        };
    }

    impl_->vocab =
        llama_model_get_vocab(
            impl_->model
        );

    if (impl_->vocab == nullptr) {
        unload();

        return {
            StatusCode::model_load_failed,
            "model vocabulary is unavailable"
        };
    }

    /*
     * Phase 1 uses deterministic greedy decoding.
     */
    impl_->sampler =
        llama_sampler_chain_init(
            llama_sampler_chain_default_params()
        );

    if (impl_->sampler == nullptr) {
        unload();

        return {
            StatusCode::generation_failed,
            "failed to initialize sampler"
        };
    }

    llama_sampler *greedy =
        llama_sampler_init_greedy();

    if (greedy == nullptr) {
        unload();

        return {
            StatusCode::generation_failed,
            "failed to initialize greedy sampler"
        };
    }

    llama_sampler_chain_add(
        impl_->sampler,
        greedy
    );

    return {
        StatusCode::ok,
        "ok"
    };
}

Status Runtime::generate(
    std::string_view prompt,
    const TokenCallback &on_token) {

    if (impl_ == nullptr ||
        impl_->model == nullptr ||
        impl_->ctx == nullptr ||
        impl_->sampler == nullptr ||
        impl_->vocab == nullptr) {

        return {
            StatusCode::model_load_failed,
            "no model loaded"
        };
    }

    if (!on_token) {
        return {
            StatusCode::invalid_argument,
            "token callback is empty"
        };
    }

    if (impl_->cfg.max_output_tokens == 0) {
        return {
            StatusCode::invalid_argument,
            "max_output_tokens must be greater than zero"
        };
    }

    const std::string prompt_string(prompt);

    /*
     * First tokenizer call determines required capacity.
     */
    const int required_token_count =
        llama_tokenize(
            impl_->vocab,
            prompt_string.c_str(),
            static_cast<int32_t>(
                prompt_string.size()
            ),
            nullptr,
            0,
            true,
            true
        );

    if (required_token_count >= 0) {
        return {
            StatusCode::generation_failed,
            "unexpected tokenizer sizing result"
        };
    }

    const int64_t required_capacity =
        -static_cast<int64_t>(
            required_token_count
        );

    if (required_capacity <= 0 ||
        required_capacity >
            static_cast<int64_t>(
                std::numeric_limits<int32_t>::max()
            )) {

        return {
            StatusCode::generation_failed,
            "invalid tokenizer capacity"
        };
    }

    std::vector<llama_token> tokens(
        static_cast<std::size_t>(
            required_capacity
        )
    );

    const int token_count =
        llama_tokenize(
            impl_->vocab,
            prompt_string.c_str(),
            static_cast<int32_t>(
                prompt_string.size()
            ),
            tokens.data(),
            static_cast<int32_t>(
                tokens.size()
            ),
            true,
            true
        );

    if (token_count < 0) {
        return {
            StatusCode::generation_failed,
            "tokenization failed"
        };
    }

    tokens.resize(
        static_cast<std::size_t>(
            token_count
        )
    );

    const uint32_t context_tokens =
        llama_n_ctx(impl_->ctx);

    const uint64_t prompt_tokens =
        static_cast<uint64_t>(
            tokens.size()
        );

    const uint64_t output_tokens =
        static_cast<uint64_t>(
            impl_->cfg.max_output_tokens
        );

    if (prompt_tokens + output_tokens >
        static_cast<uint64_t>(
            context_tokens
        )) {

        return {
            StatusCode::context_overflow,
            "prompt plus configured output exceeds context"
        };
    }

    if (tokens.empty()) {
        return {
            StatusCode::invalid_argument,
            "prompt produced zero tokens"
        };
    }

    /*
     * Prompt batch.
     *
     * Only the final prompt token produces logits.
     */
    llama_batch prompt_batch =
        llama_batch_init(
            static_cast<int32_t>(
                tokens.size()
            ),
            0,
            1
        );

    if (prompt_batch.token == nullptr ||
        prompt_batch.pos == nullptr ||
        prompt_batch.n_seq_id == nullptr ||
        prompt_batch.seq_id == nullptr ||
        prompt_batch.logits == nullptr) {

        llama_batch_free(prompt_batch);

        return {
            StatusCode::insufficient_memory,
            "failed to allocate prompt batch"
        };
    }

    for (std::size_t i = 0;
         i < tokens.size();
         ++i) {

        prompt_batch.token[i] =
            tokens[i];

        prompt_batch.pos[i] =
            static_cast<llama_pos>(i);

        prompt_batch.n_seq_id[i] = 1;

        prompt_batch.seq_id[i][0] = 0;

        prompt_batch.logits[i] =
            (i + 1U == tokens.size())
                ? 1
                : 0;
    }

    prompt_batch.n_tokens =
        static_cast<int32_t>(
            tokens.size()
        );

    const int prompt_decode_result =
        llama_decode(
            impl_->ctx,
            prompt_batch
        );

    llama_batch_free(prompt_batch);

    if (prompt_decode_result != 0) {
        return {
            StatusCode::generation_failed,
            std::string(
                "prompt decode failed: "
            ) +
            decode_error_message(
                prompt_decode_result
            )
        };
    }

    /*
     * The sampler requires logits from the final prompt token.
     */
    if (llama_get_logits_ith(
            impl_->ctx,
            -1) == nullptr) {

        return {
            StatusCode::generation_failed,
            "prompt decode produced no logits"
        };
    }

    /*
     * Reusable single-token generation batch.
     */
    llama_batch next =
        llama_batch_init(
            1,
            0,
            1
        );

    if (next.token == nullptr ||
        next.pos == nullptr ||
        next.n_seq_id == nullptr ||
        next.seq_id == nullptr ||
        next.logits == nullptr) {

        llama_batch_free(next);

        return {
            StatusCode::insufficient_memory,
            "failed to allocate generation batch"
        };
    }

    next.n_seq_id[0] = 1;
    next.seq_id[0][0] = 0;
    next.logits[0] = 1;
    next.n_tokens = 1;

    for (uint32_t i = 0;
         i < impl_->cfg.max_output_tokens;
         ++i) {

        /*
         * -1 means the last logits row.
         */
        const llama_token token =
            llama_sampler_sample(
                impl_->sampler,
                impl_->ctx,
                -1
            );

        if (token == LLAMA_TOKEN_NULL) {
            llama_batch_free(next);

            return {
                StatusCode::generation_failed,
                "sampler returned LLAMA_TOKEN_NULL"
            };
        }

        /*
         * Modern llama.cpp has multiple EOG tokens:
         * EOS, EOT, etc.
         */
        if (llama_vocab_is_eog(
                impl_->vocab,
                token)) {

            break;
        }

        /*
         * IMPORTANT:
         *
         * llama_token_to_piece() returns a NEGATIVE value
         * when the supplied buffer is too small.
         *
         * The absolute value is the required byte count.
         */
        const int piece_size_result =
            llama_token_to_piece(
                impl_->vocab,
                token,
                nullptr,
                0,
                0,
                true
            );

        if (piece_size_result >= 0) {
            llama_batch_free(next);

            return {
                StatusCode::generation_failed,
                "unexpected token piece sizing result"
            };
        }

        const int64_t required_piece_size =
            -static_cast<int64_t>(
                piece_size_result
            );

        if (required_piece_size <= 0 ||
            required_piece_size >
                static_cast<int64_t>(
                    std::numeric_limits<int32_t>::max()
                )) {

            llama_batch_free(next);

            return {
                StatusCode::generation_failed,
                "invalid token piece size"
            };
        }

        std::string piece(
            static_cast<std::size_t>(
                required_piece_size
            ),
            '\0'
        );

        const int written =
            llama_token_to_piece(
                impl_->vocab,
                token,
                piece.data(),
                static_cast<int32_t>(
                    piece.size()
                ),
                0,
                true
            );

        if (written < 0 ||
            written >
                static_cast<int>(
                    piece.size()
                )) {

            llama_batch_free(next);

            return {
                StatusCode::generation_failed,
                "failed to convert token to text"
            };
        }

        piece.resize(
            static_cast<std::size_t>(
                written
            )
        );

        if (!piece.empty()) {
            if (!on_token(piece)) {
                llama_batch_free(next);

                return {
                    StatusCode::ok,
                    "generation cancelled by callback"
                };
            }
        }

        /*
         * Update sampler state.
         */
        llama_sampler_accept(
            impl_->sampler,
            token
        );

        /*
         * Feed generated token back into the model.
         */
        next.token[0] = token;

        next.pos[0] =
            static_cast<llama_pos>(
                tokens.size() +
                static_cast<std::size_t>(i)
            );

        const int decode_result =
            llama_decode(
                impl_->ctx,
                next
            );

        if (decode_result != 0) {
            llama_batch_free(next);

            return {
                StatusCode::generation_failed,
                std::string(
                    "generation decode failed: "
                ) +
                decode_error_message(
                    decode_result
                )
            };
        }

        /*
         * Make sure the next sampling operation has logits.
         */
        if (llama_get_logits_ith(
                impl_->ctx,
                -1) == nullptr) {

            llama_batch_free(next);

            return {
                StatusCode::generation_failed,
                "generation decode produced no logits"
            };
        }
    }

    llama_batch_free(next);

    return {
        StatusCode::ok,
        "ok"
    };
}

void Runtime::unload() noexcept {
    if (impl_ == nullptr) {
        return;
    }

    if (impl_->sampler != nullptr) {
        llama_sampler_free(
            impl_->sampler
        );

        impl_->sampler = nullptr;
    }

    if (impl_->ctx != nullptr) {
        llama_free(
            impl_->ctx
        );

        impl_->ctx = nullptr;
    }

    if (impl_->model != nullptr) {
        llama_model_free(
            impl_->model
        );

        impl_->model = nullptr;
    }

    impl_->vocab = nullptr;
}

bool Runtime::loaded() const noexcept {
    return impl_ != nullptr &&
           impl_->model != nullptr &&
           impl_->ctx != nullptr;
}

uint32_t Runtime::context_size() const noexcept {
    if (!loaded()) {
        return 0;
    }

    return llama_n_ctx(
        impl_->ctx
    );
}

uint64_t Runtime::model_bytes() const noexcept {
    if (impl_ == nullptr ||
        impl_->model == nullptr) {

        return 0;
    }

    return llama_model_size(
        impl_->model
    );
}

std::size_t Runtime::peak_rss_bytes() const noexcept {
    return peak_rss();
}

} // namespace syj::core
