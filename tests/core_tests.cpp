#include <cassert>
#include <string_view>

#include "syj/core/version.hpp"

int main() {
    assert(syj::core::project_name() == std::string_view{"SYJ"});
    assert(syj::core::version() == std::string_view{"0.1.0"});
    assert(syj::core::phase() == std::string_view{"Phase 0 - Bootstrap"});
    return 0;
}
