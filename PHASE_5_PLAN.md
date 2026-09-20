# SYJ LLM Phase 5 — Fine-Tuning Pipeline

Baseline: SYJ LLM v0.4.0, commit `721962e`, tag `v0.4.0`.
Pinned llama.cpp commit: `391fac16460f15233a7740550d858ac96df3419d`.

This is an overlay for the existing Phase 1–4 checkout. Preserve the existing
`third_party/llama.cpp` tree.

## Scope

Phase 5 produces a fine-tuned model artifact. It does not modify the SYJ C/C++
inference core, memory-budget logic, or Phase 4 HTTP API.

Training uses Python tooling. Runtime inference remains Python-free and uses
the existing SYJ Core + llama.cpp + GGUF stack.

## Base model

`Qwen/Qwen3-1.7B`

License: Apache License 2.0.

The final model package must retain the upstream Qwen license and attribution
notices. See `model_card/SYJ-Model-v1.md` and
`model_card/LICENSE-APACHE-2.0.txt`.

## Pipeline

1. Validate JSONL instruction data.
2. Fine-tune with the included Transformers + PEFT LoRA/QLoRA training entry point.
3. Merge the adapter into the base model.
4. Convert the merged Hugging Face model to GGUF.
5. Quantize the GGUF to Q4_K_M with the pinned llama.cpp quantizer.
6. Scan/register the resulting GGUF with the existing Phase 3 registry.
7. Load and run the artifact through the unchanged Phase 1–4 CLI/API.

## Hardware

Training is not expected to run on a 4 GB Android/Termux device.

A practical first target is a CUDA GPU with approximately 16 GB+ VRAM. Smaller
VRAM configurations may require more aggressive QLoRA/offloading settings and
are not promised by this pipeline.

Inference remains the low-RAM target.

## Pinned-converter qualification

The convert_hf_to_gguf.py source at the pinned llama.cpp revision
391fac16460f15233a7740550d858ac96df3419d was inspected directly.

The pinned converter defines the interfaces used by Phase 5:

--outfile
--outtype

The Phase 5 conversion script therefore invokes the pinned converter with:

--outtype f16

The conversion script also performs a local --help check before executing
the conversion command.

Actual converter execution still requires the Python dependencies required
by the pinned converter, including its model-processing dependencies.

## Validation split

### Verifiable without training hardware

- JSONL schema validation.
- Example dataset validation.
- Training configuration validation.
- Script argument validation.
- Presence/help checks for conversion and quantization tools.
- GGUF/registry integration checks when a GGUF exists.
- CTest pipeline checks that do not require GPU training.
- Documentation and reproducibility checks.

### Requires project-owner GPU/cloud/desktop hardware

- Actual LoRA/QLoRA training.
- VRAM consumption and training completion.
- Adapter quality.
- LoRA merge.
- Actual HF→GGUF conversion.
- Q4_K_M quantization.
- Final model memory estimation.
- Real CLI/API inference with `SYJ-Model-v1`.
- Qualitative model behavior validation.

Do not proceed to Phase 5.5 until the owner confirms Phase 5 validation.
