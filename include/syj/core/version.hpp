#pragma once

#include <string_view>

namespace syj::core {

constexpr std::string_view project_name() noexcept { return "SYJ"; }
constexpr std::string_view vendor_name() noexcept { return "SAYANJALI NEXUS PRIVATE LIMITED"; }
constexpr std::string_view version() noexcept { return "0.1.0"; }
constexpr std::string_view phase() noexcept { return "Phase 0 - Bootstrap"; }

} // namespace syj::core
