#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>

namespace syj::core {

enum class StatusCode {
    ok,
    invalid_argument,
    model_not_found,
    model_load_failed,
    context_overflow,
    insufficient_memory,
    generation_failed
};

struct Status {
    StatusCode code;
    std::string message;

    explicit operator bool() const noexcept {
        return code == StatusCode::ok;
    }
};

struct MemoryBudget {
    // 0 means no admission limit is configured. This preserves Phase 1
    // behavior while allowing every Phase 2 caller to opt into a hard limit.
    std::uint64_t max_bytes = 0;
};

struct MemoryEstimate {
    std::uint64_t model_bytes = 0;
    std::uint64_t kv_cache_bytes = 0;
    std::uint64_t compute_buffer_bytes = 0;
    std::uint64_t overhead_bytes = 0;
    std::uint64_t required_bytes = 0;

    std::uint32_t context_size = 0;
    std::uint32_t batch_size = 0;
    std::int32_t threads = 0;

    std::int32_t layers = 0;
    std::int32_t kv_heads = 0;
    std::int32_t head_dimension = 0;

    // True when KV geometry was obtained from the model metadata API.
    // False means the estimator used its conservative fallback.
    bool exact_kv_geometry = false;
};

struct MemoryUsage {
    std::uint64_t current_rss_bytes = 0;
    std::uint64_t peak_rss_bytes = 0;
    std::uint64_t budget_bytes = 0;
    std::uint64_t estimated_required_bytes = 0;
};

struct RuntimeConfig {
    uint32_t context_size = 1024;
    uint32_t batch_size = 256;
    int32_t threads = 2;
    int32_t batch_threads = 2;
    uint32_t max_output_tokens = 128;
    bool use_mmap = true;
    bool validate_tensors = true;
    MemoryBudget memory_budget{};
};

using TokenCallback = std::function<bool(std::string_view)>;

// Applies the hard admission limit without touching the model file.
// A zero max_bytes budget means admission control is disabled.
Status enforce_memory_budget(
    const MemoryEstimate &estimate,
    const MemoryBudget &budget,
    uint32_t context_size);

class Runtime final {
public:
    Runtime();
    explicit Runtime(const RuntimeConfig &config);
    ~Runtime();

    Runtime(const Runtime &) = delete;
    Runtime &operator=(const Runtime &) = delete;

    // Uses the RuntimeConfig supplied to the constructor.
    Status load_model(const std::string &path);

    // Backward-compatible Phase 1 entry point. The supplied configuration
    // becomes the Runtime's stored configuration for subsequent loads.
    Status load_model(
        const std::string &path,
        const RuntimeConfig &config);

    // Metadata-only preflight. No model weights or inference context are
    // allocated by SYJ during this operation.
    Status estimate_memory(
        const std::string &path,
        MemoryEstimate &out) const;

    Status generate(
        std::string_view prompt,
        const TokenCallback &on_token);

    void unload() noexcept;

    bool loaded() const noexcept;
    uint32_t context_size() const noexcept;
    uint64_t model_bytes() const noexcept;
    std::size_t peak_rss_bytes() const noexcept;
    MemoryUsage memory_usage() const noexcept;
    const RuntimeConfig &config() const noexcept;

private:
    struct Impl;
    Impl *impl_ = nullptr;
};

} // namespace syj::core
