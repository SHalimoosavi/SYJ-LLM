#pragma once

#include "syj/core/model_registry.hpp"

#include <cstdint>
#include <memory>
#include <string>

namespace syj::api {

struct LocalApiConfig {
    std::string host = "127.0.0.1";
    int port = 8080;
    syj::core::RegistryConfig registry{};
    syj::core::RuntimeConfig runtime{};
};

class LocalApiServer final {
public:
    explicit LocalApiServer(LocalApiConfig config = {});
    ~LocalApiServer();

    LocalApiServer(const LocalApiServer &) = delete;
    LocalApiServer &operator=(const LocalApiServer &) = delete;

    bool listen();
    void stop() noexcept;
    bool running() const noexcept;
    const LocalApiConfig &config() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace syj::api
