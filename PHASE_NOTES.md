# SYJ LLM — Phase 1 Notes

## Phase

Phase 1 — Core Inference Runtime

## llama.cpp Dependency Pin

SYJ uses the exact upstream llama.cpp commit:

`391fac16460f15233a7740550d858ac96df3419d`

This exact commit is the authoritative dependency identity for Phase 1.

SYJ does not identify the dependency by a release label. The commit SHA is used consistently across the build configuration, vendor metadata, documentation, and vendor script.

## Vendored Source

The Phase 1 repository contains the llama.cpp source under:

`third_party/llama.cpp`

The vendored source is used directly by the SYJ CMake build.

No network access is required during inference.

## Core Runtime

Phase 1 provides the SYJ Core inference wrapper around llama.cpp.

The public SYJ API does not expose raw llama.cpp types.

Implemented capabilities include:

- GGUF model loading
- Memory-mapped model loading
- Tokenization
- Prompt evaluation
- Token generation
- Streaming token callback
- Deterministic cleanup
- Context-size validation
- Configuration validation
- Callback cancellation
- Model-load error handling
- Context overflow handling
- Low-RAM-oriented default runtime configuration

Default runtime configuration:

- Context: 1024 tokens
- Threads: 2
- Batch size: 256
- CPU-first execution
- mmap model loading

## Sampling

Phase 1 intentionally uses deterministic greedy sampling.

Advanced sampling strategies are outside the Phase 1 scope.

## CLI

The Phase 1 CLI provides:

```text
syj <model.gguf> <prompt>
