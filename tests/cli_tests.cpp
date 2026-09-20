#include "syj/core/cli.hpp"
#include <cassert>
#include <iostream>
int main(){
    syj::cli::RunOptions run{};
    assert(syj::cli::run(run) == 2);
    syj::cli::LoadOptions load{};
    assert(syj::cli::load(load) == 2);
    std::cout << "Phase 4 CLI argument behavior tests passed\n";
    return 0;
}
