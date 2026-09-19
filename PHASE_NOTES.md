# SYJ-LLM Phase 1 — Core Inference Runtime

## Pinned llama.cpp revision

**v0.4.1 / commit `391fac16460f15233a7740550d858ac96df3419d`** (14 Sep 2026).

This is the stable v0.4.1 release, not a moving nightly. The official release metadata identifies target commit `391fac16460f15233a7740550d858ac96df3419d` and notes API/core fixes including the new load-mode API and updates to ggml 0.24.0. SYJ uses `LLAMA_LOAD_MODE_MMAP`, which is the current mmap-first load path at this revision.

## Packaging note

The current build environment used to prepare this artifact cannot retrieve the ~36 MB upstream llama.cpp source archive. Therefore this bundle contains the complete SYJ integration layer plus a deterministic vendor script, but **is not falsely represented as self-contained** until `third_party/llama.cpp` is populated at `b29c606`.

Run `tools/vendor_llama.sh` once before configuring. Inference itself performs no network access.

## Low-RAM design

- CPU-only by default.
- mmap model loading enabled by default.
- 1024-token context default.
- 2 generation threads / 2 batch threads default.
- 256 batch ceiling.
- 128 output-token default in API; CLI uses 64.
- Q4_K_M and smaller GGUF formats are the primary deployment target; llama.cpp performs the actual quantized tensor handling.
- No Phase 2 memory-budget manager is introduced.

## SYJ API

`include/syj/core/runtime.hpp` deliberately hides llama.cpp types from callers. `Runtime` owns model/context/sampler and performs deterministic cleanup.

## Error handling

The wrapper maps missing files, model-load failures, context creation failures, tokenization failures, context overflow, and decode failures to `StatusCode` values.

## Build

After vendoring:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Termux/ARM64:

```sh
pkg install clang cmake git make
./tools/vendor_llama.sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DGGML_NATIVE=OFF
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
```

Windows:

```powershell
.\tools\vendor_llama.sh
cmake -S . -B build -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

## Model smoke test

Use a small GGUF model already downloaded locally. No model is shipped in source control.

```sh
./build/syj models/small-q4_k_m.gguf "Say hello from SYJ in one sentence."
```

The CLI prints model bytes, generated text, and peak RSS where supported by the host OS.

## Sanitizer

On Linux/Termux where supported:

```sh
cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=Debug -DSYJ_ENABLE_ASAN=ON
cmake --build build-asan --parallel 2
ctest --test-dir build-asan --output-on-failure
```

## Benchmark status

No honest RAM benchmark is claimed in this package because the upstream llama.cpp source could not be retrieved into this preparation environment and no GGUF model was executed here. Phase 1 validation must record the user's real-device peak RSS with the selected small Q4_K_M model.

## Before pushing

1. Verify `third_party/llama.cpp` is exactly commit `391fac16460f15233a7740550d858ac96df3419d`.
2. Confirm CMake config/build succeeds on Termux ARM64.
3. Confirm CTest passes.
4. Run one local GGUF prompt and capture peak RSS.
5. Run the sanitizer build if supported.
6. Confirm `git diff --check` and no build artifacts are staged.
7. Confirm no GGUF/model binaries are committed.
8. Confirm inference works after disabling network connectivity.
