# Phase 5 Training Pipeline

This directory contains the Python-side training/build pipeline for
`SYJ-Model-v1`. It is deliberately separate from the SYJ C/C++ inference
runtime.

## Validate without a GPU

```bash
python3 training/scripts/validate_dataset.py training/data/example/train.jsonl
python3 training/scripts/validate_config.py training/configs/syj-model-v1.yaml
python3 third_party/llama.cpp/convert_hf_to_gguf.py --help
```

The first two checks are local. The converter help check is the mandatory
owner check against the pinned llama.cpp checkout.

## Training environment

Training requires a GPU/cloud/desktop environment.

```bash
python3 -m venv .venv-syj-training
. .venv-syj-training/bin/activate
python -m pip install --upgrade pip
python -m pip install -r training/requirements.txt
```

For QLoRA:

```bash
python3 training/scripts/train_lora.py   --config training/configs/syj-model-v1.yaml   --dataset training/data/example/train.jsonl   --output-dir training/output/syj-model-v1-adapter   --qlora
```

The example dataset is only a pipeline smoke-test dataset, not a useful
production corpus. For a real domain model, replace it with a curated,
licensed dataset.

## Merge

```bash
python3 training/scripts/merge_lora.py   --base-model /path/to/Qwen3-1.7B   --adapter-dir training/output/syj-model-v1-adapter   --output-dir training/output/SYJ-Model-v1-merged
```

## Pinned converter check and conversion

The converter source at the pinned llama.cpp revision
391fac16460f15233a7740550d858ac96df3419d has been source-verified to
define --outfile and --outtype.

The conversion script performs a runtime help check before conversion.

Then:

bash training/scripts/convert_to_gguf.sh training/output/SYJ-Model-v1-merged training/output/SYJ-Model-v1-f16.gguf

## Quantization

Build the quantizer from the existing pinned llama.cpp checkout, then:

```bash
bash training/scripts/quantize_q4_k_m.sh training/output/SYJ-Model-v1-f16.gguf training/output/SYJ-Model-v1.gguf
```

## Registry and runtime validation

```bash
./build/syj model scan training/output
./build/syj model show SYJ-Model-v1
./build/syj model load SYJ-Model-v1 --memory-budget BYTES --context 1024
./build/syj run SYJ-Model-v1 "Your validation prompt" --max-tokens 128
```

The Phase 2 estimator determines the RAM tier. Do not hard-code one.

The final GGUF must also be exercised through the unchanged Phase 4 local API.
