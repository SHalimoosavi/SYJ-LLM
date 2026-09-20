#include <cassert>
#include <filesystem>

int main() {
    namespace fs = std::filesystem;
    const fs::path root = fs::path(SYJ_SOURCE_DIR);
    const char* paths[] = {
        "training/data/example/train.jsonl",
        "training/configs/syj-model-v1.yaml",
        "training/scripts/validate_dataset.py",
        "training/scripts/validate_config.py",
        "training/scripts/train_lora.py",
        "training/scripts/merge_lora.py",
        "training/scripts/convert_to_gguf.sh",
        "training/scripts/quantize_q4_k_m.sh",
        "model_card/SYJ-Model-v1.md",
        "model_card/LICENSE-APACHE-2.0.txt"
    };
    for (const char* p : paths) assert(fs::exists(root / p));
    return 0;
}
