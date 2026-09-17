#include <iostream>

#include "syj/core/version.hpp"

int main() {
    std::cout << syj::core::project_name() << " LLM runtime\n"
              << "Version: " << syj::core::version() << "\n"
              << "Vendor: " << syj::core::vendor_name() << "\n"
              << "Stage: " << syj::core::phase() << "\n"
              << "Status: bootstrap OK\n";
    return 0;
}
