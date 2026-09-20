# SYJ LLM — Phase 2 Notes

## Baseline verified before implementation

Live GitHub `main` was checked before preparing this Phase 2 artifact.

Live HEAD:

`696529857642f607b86618a7f24c13e888c49bdc`

Commit:

`Pin llama.cpp dependency to exact upstream commit`

Current release tag state at preparation time:

`v0.1.0` points to the same commit.

The older planning note that described `v0.2.0` as the live tag is stale; the live repository currently exposes `v0.1.0`.

Pinned llama.cpp:

`391fac16460f15233a7740550d858ac96df3419d`

## Phase 1 measurement that drives Phase 2

The real Phase 1 Termux/ARM64 validation used:

`Llama-3.2-3B-Instruct-Q4_K_M.gguf`

with context `1024` and recorded:

- Model bytes: `2011539712`
- CPU mapped model buffer: approximately `1918.35 MiB`
- KV cache: `112.00 MiB`
- CPU compute buffer: `131.25 MiB`
- Peak RSS: `2195750912` bytes, approximately `2.04 GiB`

This is why Phase 2 does **not** claim that a 3B model is safe under a 1.5–2.0 GiB budget on a 4 GB device.

## Memory-tier policy

These are admission-policy targets, not hardware safety certifications. The project must continue to validate real models on real devices before calling a tier safe.

| Tier | Runtime budget | Intended model class | Basis |
|---|---:|---|---|
| Edge | `<= 1.50 GiB` | Target for smaller models, including `<= 2B` candidates | Leaves the 3B measured footprint outside the tier. A real small-model measurement is still required. |
| Standard | `2.50 GiB` | 3B-class target | The measured 3B peak was about `2.04 GiB`; Phase 2's estimator adds KV, compute, and 10% runtime overhead, producing roughly `2.3 GiB` at 1024 context. `2.50 GiB` leaves additional admission headroom. |
| Large | `4.00 GiB` | Models above 3B | No Phase 1 measurement certifies this tier. It is an explicit budget tier for later validation rather than a safety claim. |

A device's total physical RAM is not treated as equivalent to the runtime budget. Android/system processes, allocator behavior, filesystem cache, and other applications consume memory outside SYJ.

## Estimator design

The pre-load estimator performs a metadata-only llama.cpp model load with `no_alloc=true`. It does not materialize the real model weights before admission.

The estimate contains:

1. **Model weights** — `llama_model_size()` from the metadata-only model representation.
2. **KV cache** — calculated from context length, layer count, KV head count, and head dimension, assuming the Phase 1 F16 K/V cache. When model geometry cannot be derived, a conservative fallback of `256 KiB/token` is used.
3. **Compute buffer** — calibrated against the Phase 1 3B observation at batch 256 and scaled by batch size and thread count. The calibration coefficient is `6.8%` of model bytes at the Phase 1 baseline.
4. **Runtime overhead** — an additional `10%` of the model + KV + compute subtotal.

The estimate is intentionally conservative and is an admission guard, not a promise of exact allocator usage.

For the Phase 1 3B model, the model + KV + compute components are approximately `2.12 GiB`, and the 10% overhead produces an estimated requirement of roughly `2.32 GiB` at context 1024/batch 256/2 threads.

## Hard budget enforcement

`MemoryBudget.max_bytes == 0` disables admission control for backward compatibility.

For a non-zero budget, `Runtime::load_model()` performs this sequence:

1. Validate runtime configuration.
2. Perform metadata-only model inspection.
3. Estimate weights + KV + compute + overhead.
4. Compare the estimate to `MemoryBudget.max_bytes`.
5. Return `SYJ_ERROR_INSUFFICIENT_MEMORY` before real model loading if the estimate exceeds the budget.
6. Load the real GGUF only after admission succeeds.
7. If the actual context allocation still fails, return a clean status rather than truncating or silently continuing.

## Exact error shape

Implemented budget rejection shape:

```text
SYJ_ERROR_INSUFFICIENT_MEMORY
Required: <estimated GiB>
Available budget: <budget GiB>
Suggested: Reduce context from <current> to <reduced>, or use a smaller quantized model.
```

For example:

```text
SYJ_ERROR_INSUFFICIENT_MEMORY
Required: 2.32 GiB
Available budget: 1.50 GiB
Suggested: Reduce context from 1024 to 512, or use a smaller quantized model.
```

## Queryable memory state

`Runtime::memory_usage()` exposes:

- current RSS
- peak RSS
- configured budget
- estimated required bytes

This is intentionally a runtime API rather than CLI-only output so a later Studio/dashboard layer can consume the same state without adding dashboard code to Phase 2.

## Files added/modified by Phase 2

Modified:

- `CMakeLists.txt`
- `include/syj/core/runtime.hpp`
- `src/core/runtime.cpp`
- `src/cli/main.cpp` — retained Phase 1 behavior while using the updated runtime API
- `include/syj/core/version.hpp` — unchanged content retained for complete source overlay
- `src/core/version.cpp` — unchanged content retained for complete source overlay
- `tests/core_tests.cpp` — unchanged Phase 1 test content retained
- `README.md`
- `PHASE_NOTES.md`

Added:

- `src/core/memory_budget.cpp`
- `src/core/memory_budget.hpp`
- `tests/memory_budget_tests.cpp`

No llama.cpp source files are changed.

No model files are added.

No HTTP server, Studio dashboard, registry, agent, fine-tuning, packaging, or WASM code is added.

## Validation status

No Phase 2 build or CTest result is claimed in this artifact.

The preparation environment cannot access the user's Termux build tree, and the public GitHub connector is read-only. The full vendored llama.cpp tree could not be copied into this artifact from the connector, so the ZIP is explicitly an **overlay ZIP** that must be extracted over the existing Phase 2 baseline containing `third_party/llama.cpp`.

This is intentional transparency: the artifact is not falsely described as self-contained when the pinned ~174 MB vendor tree is not available to the file-generation environment.

## Required manual validation on ARM64/Termux

From the existing repository root after extracting this overlay:

```sh
cd ~/SYJ-LLM

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 1
ctest --test-dir build --output-on-failure
./build/syj --help
```

The existing Phase 1 smoke test is expected to remain the same CTest target:

`syj_core_tests`

For real-model Phase 2 checks, set the two existing local GGUF paths from the Phase 1 environment:

```sh
export SYJ_PHASE2_SMALL_MODEL="$HOME/SYJ-EdgeMind/models/local/SmolLM2-135M-Instruct-Q4_K_M.gguf"
export SYJ_PHASE2_3B_MODEL="$HOME/llama.cpp/models/Llama-3.2-3B-Instruct-Q4_K_M.gguf"

ctest --test-dir build --output-on-failure
```

The new `syj_memory_budget_tests` will then:

1. Load the small model under a `1.50 GiB` budget and require successful admission/loading.
2. Attempt the known 3B model under the same `1.50 GiB` budget and require `SYJ_ERROR_INSUFFICIENT_MEMORY` before real model loading.
3. Verify the structured error fields.
4. Verify queryable runtime memory state for the admitted model.

Finally, repeat the real Phase 1 inference smoke test with the same 3B GGUF using a budget large enough to admit the estimator, for example:

```sh
./build/syj "$SYJ_PHASE2_3B_MODEL" "What is 2 plus 2? Answer with only the number."
```

For a direct API-level budget test, compile/run a small caller using:

```cpp
syj::core::MemoryBudget budget;
budget.max_bytes = 1536ULL * 1024ULL * 1024ULL;

syj::core::RuntimeConfig config;
config.memory_budget = budget;

syj::core::Runtime runtime(config);
syj::core::Status status = runtime.load_model("model.gguf");
```

## What was not validated by the preparation environment

- Full CMake build against the complete vendored llama.cpp tree
- Termux ARM64 compilation
- Real GGUF preflight execution
- Real RSS after Phase 2 changes
- Real small-model admission
- Real 3B pre-load rejection
- Full sanitizer build

Do not treat any of those as completed until the user runs them.

## Phase 3 gate

Do not begin Phase 3 until Phase 2 is manually built, tested, real-model validated, committed, and pushed by the repository owner.
