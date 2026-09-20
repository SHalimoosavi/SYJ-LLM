#include "syj/api/local_api.hpp"
#include <httplib.h>
#include <cassert>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>
namespace fs=std::filesystem;
int main(){
    const fs::path root=fs::temp_directory_path()/"syj_phase4_api_test";
    std::error_code ec;fs::remove_all(root,ec);fs::create_directories(root/"models",ec);assert(!ec);
    {std::ofstream out(root/"registry.json");out<<"{\n  \"schema_version\": 1,\n  \"models\": []\n}\n";}
    syj::api::LocalApiConfig cfg;cfg.host="127.0.0.1";cfg.port=18080;cfg.registry.manifest_path=(root/"registry.json").string();cfg.registry.models_directory=(root/"models").string();
    syj::api::LocalApiServer server(cfg);
    std::thread thread([&]{server.listen();});
    for(int i=0;i<100 && !server.running();++i) std::this_thread::sleep_for(std::chrono::milliseconds(10));
    assert(server.running());
    httplib::Client client("127.0.0.1",18080);client.set_connection_timeout(1,0);client.set_read_timeout(2,0);
    auto list=client.Get("/v1/models");assert(list);assert(list->status==200);assert(list->body.find("\"models\"")!=std::string::npos);
    auto bad=client.Post("/v1/models/load","application/json",R"({"name":"missing"})");assert(bad);assert(bad->status==404);assert(bad->body.find("model_not_found")!=std::string::npos);
    const char* model=std::getenv("SYJ_TEST_MODEL");
    if(model && *model){
        std::ofstream out(root/"registry.json");
        out << "{\n  \"schema_version\": 1,\n  \"models\": [{\n"
            << "    \"name\": \"test-model\",\n    \"architecture\": \"unknown\",\n    \"parameter_count\": 0,\n    \"quantization\": \"unknown\",\n    \"file_size_bytes\": 0,\n    \"expected_ram_tier\": \"unknown\",\n    \"context_support\": 1024,\n    \"local_path\": \"" << model << "\",\n    \"estimated_required_bytes\": 0,\n    \"metadata_complete\": false\n  }]\n}\n";
        out.close();
        auto load=client.Post("/v1/models/load","application/json",R"({"name":"test-model","context_size":512,"max_output_tokens":32})");
        assert(load);assert(load->status==200);
        auto gen=client.Post("/v1/generate","application/json",R"({"model":"test-model","prompt":"Say one word."})");
        assert(gen);assert(gen->status==200);assert(gen->body.find("event: done")!=std::string::npos || gen->body.find("event: error")!=std::string::npos);
    } else {
        std::cout << "SYJ_TEST_MODEL not set: real-GGUF load/generate integration skipped\n";
    }
    server.stop();thread.join();fs::remove_all(root,ec);
    std::cout << "Phase 4 local API contract tests passed\n";
    return 0;
}
