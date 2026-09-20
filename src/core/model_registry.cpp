#include "syj/core/model_registry.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <llama.h>
#include <utility>
#include <sstream>
#include <string>

namespace syj::core {
namespace {

namespace fs = std::filesystem;

constexpr std::uint64_t kMiB = 1024ULL * 1024ULL;
constexpr std::uint64_t kEdgeBudget = 1536ULL * kMiB;
constexpr std::uint64_t kStandardBudget = 2560ULL * kMiB;
constexpr std::uint64_t kLargeBudget = 4096ULL * kMiB;

std::string json_escape(const std::string &value) {
    std::ostringstream out;
    for (const char ch : value) {
        switch (ch) {
            case '\\': out << "\\\\"; break;
            case '"': out << "\\\""; break;
            case '\n': out << "\\n"; break;
            case '\r': out << "\\r"; break;
            case '\t': out << "\\t"; break;
            default:
                if (static_cast<unsigned char>(ch) < 0x20U) {
                    out << ' ';
                } else {
                    out << ch;
                }
                break;
        }
    }
    return out.str();
}

void skip_ws(const std::string &s, std::size_t &p) {
    while (p < s.size() &&
           std::isspace(static_cast<unsigned char>(s[p])) != 0) {
        ++p;
    }
}

bool consume(const std::string &s, std::size_t &p, char expected) {
    skip_ws(s, p);
    if (p >= s.size() || s[p] != expected) {
        return false;
    }
    ++p;
    return true;
}

bool parse_string(const std::string &s, std::size_t &p, std::string &out) {
    skip_ws(s, p);
    if (p >= s.size() || s[p] != '"') {
        return false;
    }
    ++p;
    std::ostringstream value;
    while (p < s.size()) {
        const char ch = s[p++];
        if (ch == '"') {
            out = value.str();
            return true;
        }
        if (ch != '\\') {
            value << ch;
            continue;
        }
        if (p >= s.size()) {
            return false;
        }
        const char escaped = s[p++];
        switch (escaped) {
            case '"': value << '"'; break;
            case '\\': value << '\\'; break;
            case '/': value << '/'; break;
            case 'n': value << '\n'; break;
            case 'r': value << '\r'; break;
            case 't': value << '\t'; break;
            default: return false;
        }
    }
    return false;
}

bool parse_uint64(const std::string &s, std::size_t &p, std::uint64_t &out) {
    skip_ws(s, p);
    const std::size_t start = p;
    while (p < s.size() &&
           std::isdigit(static_cast<unsigned char>(s[p])) != 0) {
        ++p;
    }
    if (start == p) {
        return false;
    }
    try {
        out = std::stoull(s.substr(start, p - start));
    } catch (...) {
        return false;
    }
    return true;
}

bool parse_bool(const std::string &s, std::size_t &p, bool &out) {
    skip_ws(s, p);
    if (s.compare(p, 4, "true") == 0) {
        p += 4;
        out = true;
        return true;
    }
    if (s.compare(p, 5, "false") == 0) {
        p += 5;
        out = false;
        return true;
    }
    return false;
}

bool skip_value(const std::string &s, std::size_t &p);

bool skip_object(const std::string &s, std::size_t &p) {
    if (!consume(s, p, '{')) {
        return false;
    }
    skip_ws(s, p);
    if (p < s.size() && s[p] == '}') {
        ++p;
        return true;
    }
    while (p < s.size()) {
        std::string key;
        if (!parse_string(s, p, key) || !consume(s, p, ':') ||
            !skip_value(s, p)) {
            return false;
        }
        skip_ws(s, p);
        if (p < s.size() && s[p] == '}') {
            ++p;
            return true;
        }
        if (!consume(s, p, ',')) {
            return false;
        }
    }
    return false;
}

bool skip_array(const std::string &s, std::size_t &p) {
    if (!consume(s, p, '[')) {
        return false;
    }
    skip_ws(s, p);
    if (p < s.size() && s[p] == ']') {
        ++p;
        return true;
    }
    while (p < s.size()) {
        if (!skip_value(s, p)) {
            return false;
        }
        skip_ws(s, p);
        if (p < s.size() && s[p] == ']') {
            ++p;
            return true;
        }
        if (!consume(s, p, ',')) {
            return false;
        }
    }
    return false;
}

bool skip_value(const std::string &s, std::size_t &p) {
    skip_ws(s, p);
    if (p >= s.size()) {
        return false;
    }
    if (s[p] == '"') {
        std::string ignored;
        return parse_string(s, p, ignored);
    }
    if (s[p] == '{') {
        return skip_object(s, p);
    }
    if (s[p] == '[') {
        return skip_array(s, p);
    }
    if (std::isdigit(static_cast<unsigned char>(s[p])) != 0) {
        std::uint64_t ignored = 0;
        return parse_uint64(s, p, ignored);
    }
    for (const char *literal : {"true", "false", "null"}) {
        const std::size_t len = std::char_traits<char>::length(literal);
        if (s.compare(p, len, literal) == 0) {
            p += len;
            return true;
        }
    }
    return false;
}

bool find_field_start(const std::string &object,
                      const std::string &field,
                      std::size_t &value_pos) {
    std::size_t p = 0;
    if (!consume(object, p, '{')) {
        return false;
    }
    while (p < object.size()) {
        skip_ws(object, p);
        if (p < object.size() && object[p] == '}') {
            return false;
        }
        std::string key;
        if (!parse_string(object, p, key) || !consume(object, p, ':')) {
            return false;
        }
        if (key == field) {
            value_pos = p;
            return true;
        }
        if (!skip_value(object, p) || !consume(object, p, ',')) {
            return false;
        }
    }
    return false;
}

bool extract_string(const std::string &object,
                    const std::string &field,
                    std::string &out) {
    std::size_t p = 0;
    if (!find_field_start(object, field, p)) {
        return false;
    }
    return parse_string(object, p, out);
}

bool extract_uint64(const std::string &object,
                    const std::string &field,
                    std::uint64_t &out) {
    std::size_t p = 0;
    if (!find_field_start(object, field, p)) {
        return false;
    }
    return parse_uint64(object, p, out);
}

bool extract_bool(const std::string &object,
                  const std::string &field,
                  bool &out) {
    std::size_t p = 0;
    if (!find_field_start(object, field, p)) {
        return false;
    }
    return parse_bool(object, p, out);
}

std::string entry_json(const ModelEntry &entry) {
    std::ostringstream out;
    out << "    {\n"
        << "      \"name\": \"" << json_escape(entry.name) << "\",\n"
        << "      \"architecture\": \"" << json_escape(entry.architecture) << "\",\n"
        << "      \"parameter_count\": " << entry.parameter_count << ",\n"
        << "      \"quantization\": \"" << json_escape(entry.quantization) << "\",\n"
        << "      \"file_size_bytes\": " << entry.file_size_bytes << ",\n"
        << "      \"expected_ram_tier\": \"" << memory_tier_name(entry.expected_ram_tier) << "\",\n"
        << "      \"context_support\": " << entry.context_support << ",\n"
        << "      \"local_path\": \"" << json_escape(entry.local_path) << "\",\n"
        << "      \"estimated_required_bytes\": " << entry.estimated_required_bytes << ",\n"
        << "      \"metadata_complete\": " << (entry.metadata_complete ? "true" : "false") << "\n"
        << "    }";
    return out.str();
}

bool split_model_objects(const std::string &json,
                         std::vector<std::string> &objects) {
    std::size_t models_pos = json.find("\"models\"");
    if (models_pos == std::string::npos) {
        return false;
    }
    std::size_t p = json.find('[', models_pos);
    if (p == std::string::npos) {
        return false;
    }
    ++p;
    while (p < json.size()) {
        skip_ws(json, p);
        if (p < json.size() && json[p] == ']') {
            return true;
        }
        if (p >= json.size() || json[p] != '{') {
            return false;
        }
        const std::size_t start = p;
        int depth = 0;
        bool in_string = false;
        bool escaped = false;
        for (; p < json.size(); ++p) {
            const char ch = json[p];
            if (in_string) {
                if (escaped) {
                    escaped = false;
                } else if (ch == '\\') {
                    escaped = true;
                } else if (ch == '"') {
                    in_string = false;
                }
                continue;
            }
            if (ch == '"') {
                in_string = true;
            } else if (ch == '{') {
                ++depth;
            } else if (ch == '}') {
                --depth;
                if (depth == 0) {
                    ++p;
                    objects.push_back(json.substr(start, p - start));
                    break;
                }
            }
        }
        skip_ws(json, p);
        if (p < json.size() && json[p] == ',') {
            ++p;
            continue;
        }
        if (p < json.size() && json[p] == ']') {
            return true;
        }
        return false;
    }
    return false;
}

bool parse_entry(const std::string &object, ModelEntry &entry) {
    std::uint64_t value = 0;
    std::string tier;

    if (!extract_string(object, "name", entry.name) ||
        !extract_string(object, "architecture", entry.architecture) ||
        !extract_string(object, "quantization", entry.quantization) ||
        !extract_string(object, "expected_ram_tier", tier) ||
        !extract_string(object, "local_path", entry.local_path) ||
        !extract_uint64(object, "parameter_count", entry.parameter_count) ||
        !extract_uint64(object, "file_size_bytes", entry.file_size_bytes) ||
        !extract_uint64(object, "estimated_required_bytes",
                        entry.estimated_required_bytes) ||
        !extract_uint64(object, "context_support", value) ||
        !extract_bool(object, "metadata_complete", entry.metadata_complete)) {
        return false;
    }

    entry.context_support = static_cast<std::uint32_t>(value);
    if (tier == "edge") entry.expected_ram_tier = MemoryTier::edge;
    else if (tier == "standard") entry.expected_ram_tier = MemoryTier::standard;
    else if (tier == "large") entry.expected_ram_tier = MemoryTier::large;
    else if (tier == "above_large") entry.expected_ram_tier = MemoryTier::above_large;
    else entry.expected_ram_tier = MemoryTier::unknown;

    return true;
}

std::string architecture_from_metadata(llama_model *model) {
    constexpr std::size_t kBufferSize = 256;
    char buffer[kBufferSize]{};
    const int32_t result = llama_model_meta_val_str(
        model, "general.architecture", buffer, kBufferSize);
    if (result <= 0) {
        return "unknown";
    }
    return std::string(buffer);
}

} // namespace

const char *memory_tier_name(MemoryTier tier) noexcept {
    switch (tier) {
        case MemoryTier::edge: return "edge";
        case MemoryTier::standard: return "standard";
        case MemoryTier::large: return "large";
        case MemoryTier::above_large: return "above_large";
        default: return "unknown";
    }
}

MemoryTier classify_memory_tier(std::uint64_t required_bytes) noexcept {
    if (required_bytes == 0) return MemoryTier::unknown;
    if (required_bytes <= kEdgeBudget) return MemoryTier::edge;
    if (required_bytes <= kStandardBudget) return MemoryTier::standard;
    if (required_bytes <= kLargeBudget) return MemoryTier::large;
    return MemoryTier::above_large;
}

ModelRegistry::ModelRegistry(RegistryConfig config)
    : config_(std::move(config)) {}

Status ModelRegistry::load_manifest() {
    entries_.clear();
    std::ifstream input(config_.manifest_path);
    if (!input.good()) {
        return {StatusCode::model_not_found,
                "registry manifest not found: " + config_.manifest_path};
    }
    std::ostringstream buffer;
    buffer << input.rdbuf();
    const std::string json = buffer.str();

    std::vector<std::string> objects;
    if (!split_model_objects(json, objects)) {
        return {StatusCode::invalid_argument,
                "invalid registry manifest JSON"};
    }
    for (const auto &object : objects) {
        ModelEntry entry;
        if (!parse_entry(object, entry)) {
            return {StatusCode::invalid_argument,
                    "invalid model entry in registry manifest"};
        }
        entries_.push_back(std::move(entry));
    }
    return {StatusCode::ok, "ok"};
}

Status ModelRegistry::save_manifest() const {
    const fs::path manifest(config_.manifest_path);
    if (manifest.has_parent_path()) {
        std::error_code ec;
        fs::create_directories(manifest.parent_path(), ec);
        if (ec) {
            return {StatusCode::model_load_failed,
                    "failed to create registry directory: " + ec.message()};
        }
    }
    std::ofstream output(config_.manifest_path, std::ios::trunc);
    if (!output.good()) {
        return {StatusCode::model_load_failed,
                "unable to write registry manifest: " + config_.manifest_path};
    }

    output << "{\n"
           << "  \"schema_version\": 1,\n"
           << "  \"models\": [\n";
    for (std::size_t i = 0; i < entries_.size(); ++i) {
        output << entry_json(entries_[i]);
        if (i + 1U < entries_.size()) output << ',';
        output << '\n';
    }
    output << "  ]\n}\n";
    return output.good() ? Status{StatusCode::ok, "ok"}
                         : Status{StatusCode::model_load_failed,
                                  "failed while writing registry manifest"};
}

Status ModelRegistry::register_model(const ModelEntry &entry, bool overwrite) {
    if (entry.name.empty() || entry.local_path.empty()) {
        return {StatusCode::invalid_argument,
                "model name and local_path are required"};
    }
    auto it = std::find_if(
        entries_.begin(), entries_.end(),
        [&](const ModelEntry &current) { return current.name == entry.name; });
    if (it != entries_.end()) {
        if (!overwrite) {
            return {StatusCode::invalid_argument,
                    "model name already registered: " + entry.name};
        }
        *it = entry;
    } else {
        entries_.push_back(entry);
    }
    return {StatusCode::ok, "ok"};
}

Status ModelRegistry::get_model(const std::string &name,
                                ModelEntry &out) const {
    const auto it = std::find_if(
        entries_.begin(), entries_.end(),
        [&](const ModelEntry &entry) { return entry.name == name; });
    if (it == entries_.end()) {
        return {StatusCode::model_not_found,
                "model is not registered: " + name};
    }
    out = *it;
    return {StatusCode::ok, "ok"};
}

std::vector<ModelEntry> ModelRegistry::list_models() const {
    return entries_;
}

Status ModelRegistry::scan_directory(const std::string &directory,
                                      const RuntimeConfig &runtime_config) {
    std::error_code ec;
    if (!fs::exists(directory, ec) || !fs::is_directory(directory, ec)) {
        return {StatusCode::model_not_found,
                "model directory not found: " + directory};
    }

    Runtime inspector(runtime_config);
    std::size_t discovered = 0;

    for (const auto &item : fs::directory_iterator(directory, ec)) {
        if (ec) {
            return {StatusCode::model_load_failed,
                    "failed while scanning model directory: " + ec.message()};
        }
        if (!item.is_regular_file()) continue;
        if (item.path().extension() != ".gguf") continue;

        std::ifstream header(item.path(), std::ios::binary);
        char magic[4]{};
        if (!header.read(magic, sizeof(magic)) ||
            std::string(magic, sizeof(magic)) != "GGUF") {
            continue;
        }

        MemoryEstimate estimate{};
        const Status estimate_status =
            inspector.estimate_memory(item.path().string(), estimate);
        if (!estimate_status) {
            continue;
        }

        ModelEntry entry;
        entry.name = item.path().stem().string();
        entry.architecture = "unknown";
        entry.parameter_count = 0;
        entry.quantization = "unknown";
        entry.file_size_bytes =
            static_cast<std::uint64_t>(fs::file_size(item.path(), ec));
        entry.expected_ram_tier =
            classify_memory_tier(estimate.required_bytes);
        entry.context_support = estimate.context_size;
        entry.local_path = item.path().string();
        entry.estimated_required_bytes = estimate.required_bytes;
        entry.metadata_complete = false;

        // Use a metadata-only load to obtain architecture and parameter count.
        // If a particular llama.cpp build cannot expose a field, the entry
        // remains valid with "unknown" rather than guessing from filenames.
        llama_model_params params = llama_model_default_params();
        params.no_alloc = true;
        params.check_tensors = false;
        params.n_gpu_layers = 0;
        params.load_mode = LLAMA_LOAD_MODE_NONE;
        llama_model *model =
            llama_model_load_from_file(item.path().string().c_str(), params);
        if (model == nullptr) {
            continue;
        }
        {
            entry.architecture = architecture_from_metadata(model);
            entry.parameter_count =
                static_cast<std::uint64_t>(llama_model_n_params(model));
            entry.context_support =
                static_cast<std::uint32_t>(llama_model_n_ctx_train(model));
            entry.metadata_complete =
                entry.architecture != "unknown" &&
                entry.parameter_count > 0 &&
                entry.context_support > 0 &&
                entry.quantization != "unknown";
            llama_model_free(model);
        }

        const Status register_status = register_model(entry, true);
        if (!register_status) return register_status;
        ++discovered;
    }

    if (discovered == 0) {
        return {StatusCode::model_not_found,
                "no loadable GGUF models discovered in: " + directory};
    }
    return {StatusCode::ok, "ok"};
}

Status ModelRegistry::resolve(const std::string &name,
                              const RuntimeConfig &runtime_config,
                              ResolvedModel &out) const {
    ModelEntry entry;
    const Status lookup = get_model(name, entry);
    if (!lookup) return lookup;

    Runtime inspector(runtime_config);
    MemoryEstimate estimate{};
    const Status status =
        inspector.estimate_memory(entry.local_path, estimate);
    if (!status) return status;

    out.entry = entry;
    out.entry.file_size_bytes = entry.file_size_bytes;
    out.entry.estimated_required_bytes = estimate.required_bytes;
    out.entry.expected_ram_tier =
        classify_memory_tier(estimate.required_bytes);
    out.memory_estimate = estimate;
    return {StatusCode::ok, "ok"};
}

Status ModelRegistry::list_models_fitting_budget(
    std::uint64_t max_bytes,
    const RuntimeConfig &runtime_config,
    std::vector<ModelEntry> &out) const {
    out.clear();
    Runtime inspector(runtime_config);
    for (const auto &entry : entries_) {
        MemoryEstimate estimate{};
        const Status status =
            inspector.estimate_memory(entry.local_path, estimate);
        if (!status) continue;
        if (estimate.required_bytes <= max_bytes) {
            ModelEntry resolved = entry;
            resolved.estimated_required_bytes = estimate.required_bytes;
            resolved.expected_ram_tier =
                classify_memory_tier(estimate.required_bytes);
            out.push_back(std::move(resolved));
        }
    }
    return {StatusCode::ok, "ok"};
}

const RegistryConfig &ModelRegistry::config() const noexcept {
    return config_;
}

} // namespace syj::core
