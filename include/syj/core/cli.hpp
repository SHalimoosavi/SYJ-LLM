#pragma once
#include <string>
namespace syj::cli {
struct RunOptions { std::string model_name; std::string prompt; unsigned long long memory_budget_bytes=0; unsigned int context_size=1024; unsigned int max_output_tokens=64; };
struct LoadOptions { std::string model_name; unsigned long long memory_budget_bytes=0; unsigned int context_size=1024; };
int run(const RunOptions &options);
int run_path(const std::string &model_path, const std::string &prompt);
int load(const LoadOptions &options);
}
