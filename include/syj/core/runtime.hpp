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

struct RuntimeConfig {
    uint32_t context_size = 1024;
    uint32_t batch_size = 256;
    int32_t threads = 2;
    int32_t batch_threads = 2;
    uint32_t max_output_tokens = 128;
    bool use_mmap = true;
    bool validate_tensors = true;
};

using TokenCallback = std::function<bool(std::string_view)>;

class Runtime final {
public:
    Runtime();
    ~Runtime();

    Runtime(const Runtime &) = delete;
    Runtime &operator=(const Runtime &) = delete;

    Status load_model(
        const std::string &path,
        const RuntimeConfig &config = {}
    );

    Status generate(
        std::string_view prompt,
        const TokenCallback &on_token
    );

    void unload() noexcept;

    bool loaded() const noexcept;
    uint32_t context_size() const noexcept;
    uint64_t model_bytes() const noexcept;
    std::size_t peak_rss_bytes() const noexcept;

private:
    struct Impl;
    Impl *impl_ = nullptr;
};

} // namespace syj::core
