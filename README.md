# SYJ LLM

**Offline-first, low-RAM LLM runtime and local GGUF model registry in C++17.**

SYJ is a small native runtime built around a pinned `llama.cpp` dependency. At Phase 3, it can load local GGUF models through the Phase 1 inference core, enforce Phase 2 memory budgets, and maintain an offline registry of locally available models.

[![License: Apache 2.0](https://img.shields.io/badge/License-Apache%202.0-blue.svg)](LICENSE)
[![Baseline](https://img.shields.io/badge/baseline-v0.2.0-blue.svg)](https://github.com/SHalimoosavi/SYJ-LLM/releases/tag/v0.2.0)

## Status

**Current phase: Phase 3 — Model Registry & GGUF Management**
Development version: `0.3.0`; the currently released repository tag remains `v0.2.0` until Phase 3 validation is complete.

Implemented:

- [x] Phase 0 bootstrap
- [x] Phase 1 native inference runtime
- [x] GGUF mmap loading through `llama.cpp`
- [x] Low-RAM runtime defaults
- [x] Phase 2 metadata-only memory estimation
- [x] Hard memory-budget admission
- [x] KV-cache budgeting
- [x] Current/peak RSS reporting
- [x] `SYJ_ERROR_INSUFFICIENT_MEMORY`
- [x] Local model manifest format
- [x] Model registry API
- [x] Offline GGUF directory discovery
- [x] Manual model registration API
- [x] RAM-tier classification
- [x] Registry-to-Phase-2 memory-estimator integration
- [x] Minimal `syj model list/show/scan` CLI surface
- [x] Registry unit-test coverage

Not implemented:

- [ ] Model downloading or remote registry synchronization
- [ ] Studio HTTP/API layer
- [ ] Web dashboard
- [ ] Fine-tuning
- [ ] Agent/function-calling runtime
- [ ] Windows packaging
- [ ] Linux packaging/release artifacts
- [ ] iOS bridge/UI
- [ ] macOS/Metal integration
- [ ] Android JNI
- [ ] WASM/GitHub Pages runtime

Phase 3 validation on the target ARM64 Termux device is **pending manual execution** for this artifact. No new Phase 3 build, benchmark, or real-GGUF result is claimed here.

## Architecture

```text
Application / CLI
       |
       v
+-----------------------------+
| SYJ Core                    |
| Runtime + Model Registry    |
+-------------+---------------+
              |
       +------+------+
       |             |
       v             v
Phase 2 memory    Local registry
estimator         registry.json
       |             |
       +------+------+
              |
              v
        local GGUF files
              |
              v
        pinned llama.cpp
              |
              v
       CPU-first inference
```

The registry does not replace `Runtime`. A model is resolved by name, then the existing Phase 2 estimator is used for a metadata-only preflight before a caller loads the model.

## Dependency pin

The exact `llama.cpp` source identity is:

```text
391fac16460f15233a7740550d858ac96df3419d
```

The hash above is the dependency identity. Internal upstream version strings are not used as the SYJ dependency pin.

## Build

### Linux / Android Termux ARM64

The project has previously been built and tested by the project owner on ARM64 Termux. Phase 3 must be revalidated after extraction.

```sh
cd ~/SYJ-LLM

cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DGGML_NATIVE=OFF

cmake --build build --parallel 1
ctest --test-dir build --output-on-failure
```

If memory is constrained during compilation, keep `--parallel 1`.

The vendored `third_party/llama.cpp` tree must be present at the pinned commit.

## Model registry

The default registry is:

```text
models/
└── registry.json
```

The manifest is local and offline. Registry operations do not download models.

### Manifest schema

```json
{
  "schema_version": 1,
  "models": [
    {
      "name": "example-model",
      "architecture": "llama",
      "parameter_count": 135000000,
      "quantization": "Q4_K_M",
      "file_size_bytes": 105454560,
      "expected_ram_tier": "edge",
      "context_support": 2048,
      "local_path": "models/example-model.gguf",
      "estimated_required_bytes": 158442521,
      "metadata_complete": true
    }
  ]
}
```

Discovery uses GGUF metadata through `llama.cpp` where available. It does not infer architecture or quantization from filenames. Fields that cannot be safely established are recorded as `unknown`, and manual registration can provide declared metadata.

### CLI

List registered models:

```sh
./build/syj model list
```

Show one entry:

```sh
./build/syj model show <name>
```

Scan a local directory for GGUF files:

```sh
./build/syj model scan ~/models
```

The existing inference path remains available:

```sh
./build/syj model.gguf "What is 2 plus 2?"
```

## Memory tiers

Phase 3 reuses the Phase 2 admission policy:

| Tier | Runtime budget |
|---|---:|
| Edge | <= 1.50 GiB |
| Standard | <= 2.50 GiB |
| Large | <= 4.00 GiB |
| Above large | > 4.00 GiB |

These are runtime classification/admission tiers, not hardware safety certifications.

A registry entry can be filtered against a caller's actual Phase 2 estimate rather than trusting the stored tier alone.

## Validated measurements

These are **real measurements from the Phase 2 ARM64 Termux validation**, not Phase 3 estimates.

**Environment:** Android/Termux, ARM64, September 2026; measured by the project owner.

| Model | Context | Budget test | Measured peak RSS |
|---|---:|---|---:|
| SmolLM2-135M-Instruct-Q4_K_M.gguf | 1024 | admitted at 1.50 GiB | ~157.6 MB |
| Llama-3.2-3B-Instruct-Q4_K_M.gguf | 1024 | rejected at 1.50 GiB | not loaded in rejection case |
| Llama-3.2-3B-Instruct-Q4_K_M.gguf | 1024 | admitted at 3.00 GiB | ~2.04 GiB |

The Phase 2 estimator calculated approximately `2.32 GiB` required for the 3B configuration. That is an **admission estimate**, not a measured RSS value.

No Phase 3 benchmark is reported until it is run on the target environment.

## Tests

Phase 3 adds registry tests while retaining the Phase 1 and Phase 2 suites:

```sh
ctest --test-dir build --output-on-failure
```

The Phase 3 suite covers:

- memory-tier boundary classification
- manifest write/read round trip
- local directory scanning behavior
- safe handling of invalid GGUF fixtures

Real GGUF discovery should additionally be exercised against the local model directory on the target ARM64 Termux system.

## Roadmap

| Phase | Scope | Status |
|---|---|---|
| 0 | Bootstrap & repository structure | Complete |
| 1 | Core inference runtime | Complete |
| 2 | Memory safety & budget management | Complete |
| 3 | Model registry & GGUF management | **Current** |
| 4 | CLI/API foundation for Studio | Planned |
| 5 | Fine-tuning pipeline | Planned |
| 5.5 | Agents / function calling | Planned |
| 6 | Windows packaging | Planned |
| 6.5 | Linux packaging | Planned |
| 7 | iOS bridge | Planned |
| 8 | iOS UI | Planned |
| 8.5 | macOS + Metal | Planned |
| 9 | Performance | Planned |
| 9.5 | Android JNI | Planned |
| 10 | Testing | Planned |
| 11 | Release candidate / open-source publish | Planned |
| 12 | WASM / GitHub Pages | Planned |

## Developer

**Developed by Syed Ali Hasan Moosavi**
Founder & CTO, SAYANJALI NEXUS PRIVATE LIMITED
GitHub: [SHalimoosavi](https://github.com/SHalimoosavi)

## License

SYJ LLM is licensed under the **Apache License 2.0**. See [LICENSE](LICENSE).
