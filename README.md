# SYJ LLM — Phase 2

Memory safety and budget management for the SYJ offline-first C++ inference runtime.

## Baseline

Phase 2 is designed against live `main` commit:

`696529857642f607b86618a7f24c13e888c49bdc`

Pinned llama.cpp dependency:

`391fac16460f15233a7740550d858ac96df3419d`

The llama.cpp dependency is identified by exact commit SHA only.

## Phase 2 capabilities

- Pre-load memory estimation before real weight loading
- Hard `MemoryBudget.max_bytes` admission control
- KV-cache estimation from model geometry and context length
- Compute-buffer and runtime-overhead estimate
- Structured `SYJ_ERROR_INSUFFICIENT_MEMORY` error
- Queryable current RSS, peak RSS, budget, and estimated requirement
- Constructor-based runtime configuration
- Phase 1-compatible `load_model(path, config)` entry point
- Phase 1 inference/generation path retained
- Deterministic Phase 1 CTest retained unchanged
- Optional real-model Phase 2 CTest using environment-provided GGUF paths

## Public API

```cpp
syj::core::MemoryBudget budget;
budget.max_bytes = 1536ULL * 1024ULL * 1024ULL;

syj::core::RuntimeConfig config;
config.memory_budget = budget;

syj::core::Runtime runtime(config);
syj::core::Status status = runtime.load_model("model.gguf");
```

The public API contains no raw llama.cpp types.

## Budget semantics

`MemoryBudget.max_bytes == 0` means no admission limit is configured. This preserves the Phase 1 default behavior.

When a non-zero budget is configured, SYJ estimates the model footprint before loading the real weights. A model is loaded only when the estimate is within the configured budget.

The preflight uses llama.cpp's metadata-only `no_alloc` model path. The upstream API documents `no_alloc` as loading metadata and simulating memory allocations rather than allocating model buffers. See the pinned llama.cpp API in `third_party/llama.cpp/include/llama.h`.

## Error format

Budget rejection is returned as:

```text
SYJ_ERROR_INSUFFICIENT_MEMORY
Required: 2.32 GiB
Available budget: 1.50 GiB
Suggested: Reduce context from 1024 to 512, or use a smaller quantized model.
```

The numeric requirement is model/config dependent; the four-line shape is stable.

## Scope boundary

Phase 2 does not add:

- model registry
- Studio HTTP/API server
- web dashboard
- fine-tuning
- agents
- platform packaging
- WASM

Those remain later phases.
