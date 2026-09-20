#pragma once

#include "syj/core/runtime.hpp"

namespace syj::core::detail {

Status estimate_model_memory(
    const std::string &path,
    const RuntimeConfig &config,
    MemoryEstimate &out);

std::uint64_t current_rss_bytes() noexcept;
std::uint64_t peak_rss_bytes() noexcept;

} // namespace syj::core::detail
