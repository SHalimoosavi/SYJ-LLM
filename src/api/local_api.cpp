#include "syj/api/local_api.hpp"

#include "syj/core/version.hpp"

#include <httplib.h>
#include <nlohmann/json.hpp>

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <utility>

namespace syj::api {
namespace {

using json = nlohmann::json;
using syj::core::ModelEntry;
using syj::core::ModelRegistry;
using syj::core::ResolvedModel;
using syj::core::Runtime;
using syj::core::RuntimeConfig;
using syj::core::Status;
using syj::core::StatusCode;

std::string status_code_name(StatusCode code) {
    switch (code) {
        case StatusCode::ok: return "ok";
        case StatusCode::invalid_argument: return "invalid_argument";
        case StatusCode::model_not_found: return "model_not_found";
        case StatusCode::model_load_failed: return "model_load_failed";
        case StatusCode::context_overflow: return "context_overflow";
        case StatusCode::insufficient_memory: return "insufficient_memory";
        case StatusCode::generation_failed: return "generation_failed";
        default: return "unknown";
    }
}

json model_json(const ModelEntry &model) {
    return {
        {"name", model.name},
        {"architecture", model.architecture},
        {"parameter_count", model.parameter_count},
        {"quantization", model.quantization},
        {"file_size_bytes", model.file_size_bytes},
        {"expected_ram_tier", syj::core::memory_tier_name(model.expected_ram_tier)},
        {"context_support", model.context_support},
        {"local_path", model.local_path},
        {"estimated_required_bytes", model.estimated_required_bytes},
        {"metadata_complete", model.metadata_complete}
    };
}

void write_status(httplib::Response &res, int http_status, const Status &status) {
    json body = {
        {"ok", false},
        {"error", status_code_name(status.code)},
        {"message", status.message}
    };
    res.status = http_status;
    res.set_content(body.dump(), "application/json");
}

bool read_request_json(const httplib::Request &req, json &body, std::string &error) {
    if (req.body.empty()) {
        body = json::object();
        return true;
    }
    try {
        body = json::parse(req.body);
        if (!body.is_object()) {
            error = "request body must be a JSON object";
            return false;
        }
        return true;
    } catch (const std::exception &ex) {
        error = std::string("invalid JSON request body: ") + ex.what();
        return false;
    }
}

RuntimeConfig config_from_json(const json &body, const RuntimeConfig &defaults) {
    RuntimeConfig cfg = defaults;
    if (body.contains("context_size")) cfg.context_size = body.at("context_size").get<std::uint32_t>();
    if (body.contains("batch_size")) cfg.batch_size = body.at("batch_size").get<std::uint32_t>();
    if (body.contains("threads")) cfg.threads = body.at("threads").get<std::int32_t>();
    if (body.contains("batch_threads")) cfg.batch_threads = body.at("batch_threads").get<std::int32_t>();
    if (body.contains("max_output_tokens")) cfg.max_output_tokens = body.at("max_output_tokens").get<std::uint32_t>();
    if (body.contains("memory_budget_bytes")) cfg.memory_budget.max_bytes = body.at("memory_budget_bytes").get<std::uint64_t>();
    return cfg;
}

} // namespace

struct LocalApiServer::Impl {
    explicit Impl(LocalApiConfig value)
        : config(std::move(value)), registry(config.registry), runtime(config.runtime) {}

    LocalApiConfig config;
    ModelRegistry registry;
    Runtime runtime;
    httplib::Server server;
    std::mutex state_mutex;
    std::mutex generation_mutex;
    std::string loaded_model_name;
    std::atomic<bool> running{false};

    void install_routes() {
        server.set_error_handler([](const httplib::Request &, httplib::Response &res) {
            if (res.status == 404) {
                res.set_content(R"({"ok":false,"error":"not_found","message":"endpoint not found"})", "application/json");
            }
        });

        server.Get("/v1/models", [this](const httplib::Request &, httplib::Response &res) {
            const Status status = registry.load_manifest();
            if (!status && status.code != StatusCode::model_not_found) {
                write_status(res, 500, status);
                return;
            }

            json models = json::array();
            for (const auto &model : registry.list_models()) models.push_back(model_json(model));
            res.set_content(json{{"ok", true}, {"models", models}}.dump(), "application/json");
        });

        server.Post("/v1/models/load", [this](const httplib::Request &req, httplib::Response &res) {
            json body;
            std::string parse_error;
            if (!read_request_json(req, body, parse_error)) {
                res.status = 400;
                res.set_content(json{{"ok", false}, {"error", "invalid_argument"}, {"message", parse_error}}.dump(), "application/json");
                return;
            }
            if (!body.contains("name") || !body.at("name").is_string()) {
                res.status = 400;
                res.set_content(R"({"ok":false,"error":"invalid_argument","message":"name is required"})", "application/json");
                return;
            }

            const Status manifest = registry.load_manifest();
            if (!manifest && manifest.code != StatusCode::model_not_found) {
                write_status(res, 500, manifest);
                return;
            }

            const std::string name = body.at("name").get<std::string>();
            ModelEntry entry;
            const Status get = registry.get_model(name, entry);
            if (!get) {
                write_status(res, get.code == StatusCode::model_not_found ? 404 : 400, get);
                return;
            }

            RuntimeConfig cfg = config_from_json(body, config.runtime);
            ResolvedModel resolved;
            const Status resolve = registry.resolve(name, cfg, resolved);
            if (!resolve) {
                write_status(res, resolve.code == StatusCode::insufficient_memory ? 413 : 400, resolve);
                return;
            }

            std::lock_guard<std::mutex> lock(state_mutex);
            const Status load = runtime.load_model(resolved.entry.local_path, cfg);
            if (!load) {
                write_status(res, load.code == StatusCode::insufficient_memory ? 413 : 400, load);
                return;
            }
            loaded_model_name = name;

            res.set_content(json{
                {"ok", true},
                {"model", model_json(resolved.entry)},
                {"estimated_required_bytes", resolved.memory_estimate.required_bytes}
            }.dump(), "application/json");
        });

        server.Post("/v1/generate", [this](const httplib::Request &req, httplib::Response &res) {
            json body;
            std::string parse_error;
            if (!read_request_json(req, body, parse_error)) {
                res.status = 400;
                res.set_content(json{{"ok", false}, {"error", "invalid_argument"}, {"message", parse_error}}.dump(), "application/json");
                return;
            }
            if (!body.contains("prompt") || !body.at("prompt").is_string()) {
                res.status = 400;
                res.set_content(R"({"ok":false,"error":"invalid_argument","message":"prompt is required"})", "application/json");
                return;
            }

            std::string requested_model;
            if (body.contains("model") && body.at("model").is_string()) requested_model = body.at("model").get<std::string>();
            const std::string prompt = body.at("prompt").get<std::string>();
            RuntimeConfig cfg = config_from_json(body, config.runtime);

            std::string current_model;
            {
                std::lock_guard<std::mutex> lock(state_mutex);
                current_model = loaded_model_name;
            }
            if (!requested_model.empty() && requested_model != current_model) {
                ModelEntry entry;
                const Status manifest = registry.load_manifest();
                if (!manifest && manifest.code != StatusCode::model_not_found) {
                    write_status(res, 500, manifest);
                    return;
                }
                const Status get = registry.get_model(requested_model, entry);
                if (!get) {
                    write_status(res, 404, get);
                    return;
                }
                ResolvedModel resolved;
                const Status resolve = registry.resolve(requested_model, cfg, resolved);
                if (!resolve) {
                    write_status(res, resolve.code == StatusCode::insufficient_memory ? 413 : 400, resolve);
                    return;
                }
                std::lock_guard<std::mutex> lock(state_mutex);
                const Status load = runtime.load_model(resolved.entry.local_path, cfg);
                if (!load) {
                    write_status(res, load.code == StatusCode::insufficient_memory ? 413 : 400, load);
                    return;
                }
                loaded_model_name = requested_model;
            }

            std::lock_guard<std::mutex> lock(state_mutex);
            if (!runtime.loaded()) {
                res.status = 409;
                res.set_content(R"({"ok":false,"error":"model_not_loaded","message":"load a model first or provide model in the request"})", "application/json");
                return;
            }

            res.set_chunked_content_provider(
                "text/event-stream",
                [this, prompt](size_t, httplib::DataSink &sink) {
                    std::lock_guard<std::mutex> generation_lock(generation_mutex);
                    const Status status = runtime.generate(prompt, [&](std::string_view piece) {
                        json event = {{"token", std::string(piece)}};
                        const std::string line = "data: " + event.dump() + "\n\n";
                        return sink.write(line.data(), line.size());
                    });
                    if (status) {
                        const std::string done = "event: done\ndata: {\"ok\":true}\n\n";
                        sink.write(done.data(), done.size());
                    } else {
                        json event = {{"ok", false}, {"error", status_code_name(status.code)}, {"message", status.message}};
                        const std::string line = "event: error\ndata: " + event.dump() + "\n\n";
                        sink.write(line.data(), line.size());
                    }
                    sink.done();
                    return false;
                });
            res.set_header("Cache-Control", "no-cache");
            res.set_header("Connection", "keep-alive");
            res.set_header("X-Accel-Buffering", "no");
        });
    }
};

LocalApiServer::LocalApiServer(LocalApiConfig config)
    : impl_(std::make_unique<Impl>(std::move(config))) {
    impl_->install_routes();
}

LocalApiServer::~LocalApiServer() {
    stop();
}

bool LocalApiServer::listen() {
    if (impl_->running.load()) return true;
    if (impl_->config.host.empty()) return false;
    impl_->running.store(true);
    const bool started = impl_->server.listen(impl_->config.host, impl_->config.port);
    impl_->running.store(false);
    return started;
}

void LocalApiServer::stop() noexcept {
    if (impl_ && impl_->running.exchange(false)) impl_->server.stop();
}

bool LocalApiServer::running() const noexcept {
    return impl_ && impl_->server.is_running();
}

const LocalApiConfig &LocalApiServer::config() const noexcept {
    return impl_->config;
}

} // namespace syj::api
