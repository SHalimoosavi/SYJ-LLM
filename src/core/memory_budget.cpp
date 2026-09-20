#include "syj/core/runtime.hpp"
#include "memory_budget.hpp"

#include <llama.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string>

#if defined(_WIN32)
#include <windows.h>
#include <psapi.h>
#elif defined(__APPLE__)
#include <mach/mach.h>
#elif defined(__linux__) || defined(__ANDROID__)
#include <sys/resource.h>
#endif

namespace syj::core {
namespace {

constexpr std::uint64_t kMiB = 1024ULL * 1024ULL;
constexpr std::uint64_t kGiB = 1024ULL * kMiB;
constexpr std::uint64_t kMinimumComputeBytes = 16ULL * kMiB;
constexpr std::uint64_t kFallbackKvBytesPerToken = 256ULL * 1024ULL;

bool checked_add(
    std::uint64_t a,
    std::uint64_t b,
    std::uint64_t &out) noexcept {

    if (b > std::numeric_limits<std::uint64_t>::max() - a) {
        return false;
    }

    out = a + b;
    return true;
}

bool checked_mul(
    std::uint64_t a,
    std::uint64_t b,
    std::uint64_t &out) noexcept {

    if (a != 0 &&
        b > std::numeric_limits<std::uint64_t>::max() / a) {
        return false;
    }

    out = a * b;
    return true;
}

std::string gib_string(std::uint64_t bytes) {
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(2)
           << (static_cast<long double>(bytes) /
               static_cast<long double>(kGiB))
           << " GiB";
    return stream.str();
}

std::string budget_suggestion(std::uint32_t context_size) {
    if (context_size > 512U) {
        const std::uint32_t reduced =
            std::max<std::uint32_t>(512U, context_size / 2U);

        std::ostringstream stream;
        stream << "Reduce context from "
               << context_size
               << " to "
               << reduced
               << ", or use a smaller quantized model.";
        return stream.str();
    }

    return "Use a smaller quantized model or increase the memory budget.";
}

std::uint64_t estimate_kv_bytes(
    std::uint32_t context_size,
    std::int32_t layers,
    std::int32_t kv_heads,
    std::int32_t head_dimension,
    bool &exact) noexcept {

    exact = layers > 0 &&
            kv_heads > 0 &&
            head_dimension > 0;

    if (!exact) {
        std::uint64_t fallback = 0;
        if (!checked_mul(
                static_cast<std::uint64_t>(context_size),
                kFallbackKvBytesPerToken,
                fallback)) {
            return std::numeric_limits<std::uint64_t>::max();
        }
        return fallback;
    }

    // Phase 1 uses F16 K/V cache. The cache contains both K and V, hence
    // two tensors and two bytes per element.
    std::uint64_t values = 0;

    if (!checked_mul(
            static_cast<std::uint64_t>(context_size),
            static_cast<std::uint64_t>(layers),
            values)) {
        return std::numeric_limits<std::uint64_t>::max();
    }

    if (!checked_mul(
            values,
            static_cast<std::uint64_t>(kv_heads),
            values)) {
        return std::numeric_limits<std::uint64_t>::max();
    }

    if (!checked_mul(
            values,
            static_cast<std::uint64_t>(head_dimension),
            values)) {
        return std::numeric_limits<std::uint64_t>::max();
    }

    if (!checked_mul(values, 4ULL, values)) {
        return std::numeric_limits<std::uint64_t>::max();
    }

    return values;
}

} // namespace

Status enforce_memory_budget(
    const MemoryEstimate &estimate,
    const MemoryBudget &budget,
    const std::uint32_t context_size) {

    if (budget.max_bytes == 0) {
        return {StatusCode::ok, "ok"};
    }

    if (estimate.required_bytes <= budget.max_bytes) {
        return {StatusCode::ok, "ok"};
    }

    std::ostringstream message;
    message << "SYJ_ERROR_INSUFFICIENT_MEMORY\n"
            << "Required: "
            << gib_string(estimate.required_bytes)
            << "\n"
            << "Available budget: "
            << gib_string(budget.max_bytes)
            << "\n"
            << "Suggested: "
            << budget_suggestion(context_size);

    return {
        StatusCode::insufficient_memory,
        message.str()
    };
}

namespace detail {

std::uint64_t current_rss_bytes() noexcept {
#if defined(_WIN32)
    PROCESS_MEMORY_COUNTERS counters{};
    counters.cb = sizeof(counters);

    if (GetProcessMemoryInfo(
            GetCurrentProcess(),
            &counters,
            sizeof(counters)) != 0) {
        return static_cast<std::uint64_t>(
            counters.WorkingSetSize);
    }

    return 0;
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

    return static_cast<std::uint64_t>(info.resident_size);
#elif defined(__linux__) || defined(__ANDROID__)
    std::ifstream status_file("/proc/self/status");

    if (!status_file.is_open()) {
        return 0;
    }

    std::string line;

    while (std::getline(status_file, line)) {
        if (line.compare(0, 6, "VmRSS:") != 0) {
            continue;
        }

        std::istringstream stream(line.substr(6));
        std::uint64_t value_kib = 0;
        std::string unit;

        if (stream >> value_kib >> unit &&
            (unit == "kB" || unit == "KB" || unit == "KiB")) {
            std::uint64_t result = 0;
            if (checked_mul(value_kib, 1024ULL, result)) {
                return result;
            }
        }

        break;
    }

    return 0;
#else
    return 0;
#endif
}

std::uint64_t peak_rss_bytes() noexcept {
#if defined(_WIN32)
    PROCESS_MEMORY_COUNTERS counters{};
    counters.cb = sizeof(counters);

    if (GetProcessMemoryInfo(
            GetCurrentProcess(),
            &counters,
            sizeof(counters)) != 0) {
        return static_cast<std::uint64_t>(
            counters.PeakWorkingSetSize);
    }

    return 0;
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

    return static_cast<std::uint64_t>(info.resident_size_max);
#elif defined(__linux__) || defined(__ANDROID__)
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

                if (stream >> value_kib >> unit &&
                    (unit == "kB" || unit == "KB" || unit == "KiB")) {
                    std::uint64_t result = 0;
                    if (checked_mul(value_kib, 1024ULL, result)) {
                        return result;
                    }
                }

                break;
            }
        }
    }

    rusage usage{};
    if (getrusage(RUSAGE_SELF, &usage) != 0) {
        return 0;
    }

#if defined(__ANDROID__)
    return static_cast<std::uint64_t>(usage.ru_maxrss);
#else
    return static_cast<std::uint64_t>(usage.ru_maxrss) * 1024ULL;
#endif
#else
    return 0;
#endif
}

Status estimate_model_memory(
    const std::string &path,
    const RuntimeConfig &config,
    MemoryEstimate &out) {

    std::ifstream file(path, std::ios::binary);
    if (!file.good()) {
        return {
            StatusCode::model_not_found,
            "model not found: " + path
        };
    }

    llama_model_params params =
        llama_model_default_params();

    // This is the critical pre-load guard: llama.cpp documents no_alloc as
    // metadata-only loading with simulated memory allocation, so weights are
    // not materialized before the admission decision.
    params.no_alloc = true;
    params.check_tensors = false;
    params.n_gpu_layers = 0;
    params.load_mode = LLAMA_LOAD_MODE_NONE;

    llama_model *model =
        llama_model_load_from_file(
            path.c_str(),
            params);

    if (model == nullptr) {
        return {
            StatusCode::model_load_failed,
            "unable to inspect GGUF metadata for memory estimation"
        };
    }

    out = {};
    out.model_bytes = llama_model_size(model);
    out.context_size = config.context_size;
    out.batch_size = config.batch_size;
    out.threads = config.threads;
    out.layers = llama_model_n_layer(model);
    out.kv_heads = llama_model_n_head_kv(model);

    const std::int32_t heads =
        llama_model_n_head(model);
    const std::int32_t embd =
        llama_model_n_embd(model);

    if (out.kv_heads <= 0) {
        out.kv_heads = heads;
    }

    if (heads > 0 && embd > 0 && embd % heads == 0) {
        out.head_dimension = embd / heads;
    }

    llama_model_free(model);

    bool exact_kv = false;
    out.kv_cache_bytes = estimate_kv_bytes(
        out.context_size,
        out.layers,
        out.kv_heads,
        out.head_dimension,
        exact_kv);
    out.exact_kv_geometry = exact_kv;

    if (out.model_bytes == 0 ||
        out.kv_cache_bytes == std::numeric_limits<std::uint64_t>::max()) {
        return {
            StatusCode::model_load_failed,
            "memory estimator could not derive a safe model/KV size"
        };
    }

    // Calibrated against the real Phase 1 3B measurement:
    // ~131 MiB CPU compute buffer for a ~1.87 GiB model at batch 256.
    // The estimate is intentionally conservative, not an exact allocator
    // prediction. Thread count changes scratch-space pressure modestly.
    long double compute =
        static_cast<long double>(out.model_bytes) * 0.068L;

    compute *=
        static_cast<long double>(std::max<std::uint32_t>(1U, config.batch_size)) /
        256.0L;

    compute *=
        1.0L +
        (0.05L * static_cast<long double>(
            std::max(-1, std::min(6, config.threads - 2))));

    std::uint64_t compute_bytes =
        compute >= static_cast<long double>(
            std::numeric_limits<std::uint64_t>::max())
            ? std::numeric_limits<std::uint64_t>::max()
            : static_cast<std::uint64_t>(std::ceil(compute));

    out.compute_buffer_bytes =
        std::max(kMinimumComputeBytes, compute_bytes);

    std::uint64_t subtotal = 0;
    if (!checked_add(
            out.model_bytes,
            out.kv_cache_bytes,
            subtotal) ||
        !checked_add(
            subtotal,
            out.compute_buffer_bytes,
            subtotal)) {
        return {
            StatusCode::model_load_failed,
            "memory estimator overflow"
        };
    }

    // 10% covers allocator/runtime overhead not represented by the three
    // principal components. This keeps the estimate above the Phase 1
    // measured peak while remaining simple and explainable.
    out.overhead_bytes =
        subtotal / 10ULL;

    if (!checked_add(
            subtotal,
            out.overhead_bytes,
            out.required_bytes)) {
        return {
            StatusCode::model_load_failed,
            "memory estimator overflow"
        };
    }

    return {StatusCode::ok, "ok"};
}

} // namespace detail
} // namespace syj::core
