#include "syj/core/runtime.hpp"

#include <cassert>
#include <cstdlib>
#include <iostream>
#include <string>

namespace {

constexpr std::uint64_t MiB = 1024ULL * 1024ULL;
constexpr std::uint64_t GiB = 1024ULL * MiB;

void test_small_estimate_fits() {
    syj::core::MemoryEstimate estimate{};
    estimate.model_bytes = 128ULL * MiB;
    estimate.kv_cache_bytes = 32ULL * MiB;
    estimate.compute_buffer_bytes = 32ULL * MiB;
    estimate.overhead_bytes = 24ULL * MiB;
    estimate.required_bytes =
        estimate.model_bytes +
        estimate.kv_cache_bytes +
        estimate.compute_buffer_bytes +
        estimate.overhead_bytes;
    estimate.context_size = 1024;

    const syj::core::MemoryBudget budget{512ULL * MiB};
    const auto status = syj::core::enforce_memory_budget(
        estimate,
        budget,
        1024);

    assert(status.code == syj::core::StatusCode::ok);
}

void test_known_phase1_3b_budget_is_rejected() {
    // Calibrated from the measured Phase 1 Llama-3.2-3B-Instruct-Q4_K_M run:
    // peak RSS ~= 2.04 GiB at context 1024. The Phase 2 estimator adds
    // explicit KV/compute components and a 10% overhead margin.
    syj::core::MemoryEstimate estimate{};
    estimate.model_bytes = 2011539712ULL;
    estimate.kv_cache_bytes = 112ULL * MiB;
    estimate.compute_buffer_bytes = 130ULL * MiB;
    estimate.overhead_bytes = 230ULL * MiB;
    estimate.required_bytes =
        estimate.model_bytes +
        estimate.kv_cache_bytes +
        estimate.compute_buffer_bytes +
        estimate.overhead_bytes;
    estimate.context_size = 1024;

    const syj::core::MemoryBudget budget{
        static_cast<std::uint64_t>(1.50L * static_cast<long double>(GiB))};

    const auto status = syj::core::enforce_memory_budget(
        estimate,
        budget,
        1024);

    assert(status.code == syj::core::StatusCode::insufficient_memory);
    assert(status.message.find("SYJ_ERROR_INSUFFICIENT_MEMORY") != std::string::npos);
    assert(status.message.find("Required:") != std::string::npos);
    assert(status.message.find("Available budget:") != std::string::npos);
    assert(status.message.find("Suggested: Reduce context from 1024 to 512") != std::string::npos);
}

void test_missing_model_is_safe() {
    syj::core::RuntimeConfig config;
    config.memory_budget.max_bytes = 512ULL * MiB;

    syj::core::Runtime runtime(config);
    syj::core::MemoryEstimate estimate{};

    const auto status = runtime.estimate_memory(
        "/definitely/missing/syj-model.gguf",
        estimate);

    assert(status.code == syj::core::StatusCode::model_not_found);
}

void test_optional_real_models() {
    const char *small_path = std::getenv("SYJ_PHASE2_SMALL_MODEL");
    const char *large_path = std::getenv("SYJ_PHASE2_3B_MODEL");

    if (small_path == nullptr || large_path == nullptr ||
        *small_path == '\0' || *large_path == '\0') {
        std::cout
            << "Phase 2 real-model checks skipped: set "
            << "SYJ_PHASE2_SMALL_MODEL and SYJ_PHASE2_3B_MODEL.\n";
        return;
    }

    {
        syj::core::RuntimeConfig config;
        config.memory_budget.max_bytes = 1536ULL * MiB;
        config.max_output_tokens = 16;

        syj::core::Runtime runtime(config);
        const auto status = runtime.load_model(small_path);

        assert(status.code == syj::core::StatusCode::ok);
        assert(runtime.loaded());

        const auto usage = runtime.memory_usage();
        assert(usage.budget_bytes == config.memory_budget.max_bytes);
        assert(usage.estimated_required_bytes > 0);
    }

    {
        syj::core::RuntimeConfig config;
        config.memory_budget.max_bytes = 1536ULL * MiB;
        config.max_output_tokens = 16;

        syj::core::Runtime runtime(config);
        const auto status = runtime.load_model(large_path);

        assert(status.code == syj::core::StatusCode::insufficient_memory);
        assert(status.message.find("SYJ_ERROR_INSUFFICIENT_MEMORY") != std::string::npos);
        assert(status.message.find("Required:") != std::string::npos);
        assert(status.message.find("Available budget:") != std::string::npos);
        assert(status.message.find("Suggested:") != std::string::npos);
        assert(!runtime.loaded());
    }

    std::cout << "Phase 2 real-model memory checks passed\n";
}

} // namespace

int main() {
    test_small_estimate_fits();
    test_known_phase1_3b_budget_is_rejected();
    test_missing_model_is_safe();
    test_optional_real_models();

    std::cout << "Phase 2 memory budget tests passed\n";
    return 0;
}
