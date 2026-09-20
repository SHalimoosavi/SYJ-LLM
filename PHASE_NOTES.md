# SYJ LLM — Phase 3 Notes

## Baseline verified before implementation

The Phase 3 design baseline was checked against the public GitHub repository before implementation.

```text
Repository:
SHalimoosavi/SYJ-LLM

Live HEAD:
c54ae4cf7563abb4a4c5d51a62c63863d295fbde

Verified local tags:
v0.1.0
v0.2.0

Pinned llama.cpp:
391fac16460f15233a7740550d858ac96df3419d
```

The Phase 2 release tag is `v0.2.0` and points to the Phase 2 commit.

Phase 3 is implemented as a child of that exact baseline. No Phase 4 work is included.

## Phase 2 measurements carried into Phase 3

These are real measurements supplied by the Phase 2 ARM64 Termux validation:

- SmolLM2-135M-Instruct-Q4_K_M.gguf: approximately `157.6 MB` peak RSS under the validated small-model run.
- Llama-3.2-3B-Instruct-Q4_K_M.gguf: approximately `2.04 GiB` peak RSS when admitted with a `3.00 GiB` budget.
- The same 3B model was rejected at a `1.50 GiB` budget before full model loading.
- Phase 2's estimator calculated approximately `2.32 GiB` required for the 3B configuration.

The `2.32 GiB` value is an estimator result, not a measured RSS result.

## Phase 3 scope

Phase 3 adds a local model registry without replacing the Phase 2 `Runtime` API.

The registry provides:

1. A versioned local JSON manifest.
2. A configured local models directory.
3. GGUF discovery by extension and GGUF magic.
4. Metadata extraction through llama.cpp's metadata-only model path where available.
5. Manual registration for declared metadata.
6. Name-to-path resolution.
7. Fresh Phase 2 memory preflight during resolution.
8. RAM-tier classification using the same Phase 2 budget boundaries.
9. A minimal CLI for list/show/scan.

No registry operation performs a network request.

## Manifest format

Default file:

```text
models/registry.json
```

Schema:

```json
{
  "schema_version": 1,
  "models": [
    {
      "name": "string",
      "architecture": "string",
      "parameter_count": 0,
      "quantization": "string",
      "file_size_bytes": 0,
      "expected_ram_tier": "edge|standard|large|above_large|unknown",
      "context_support": 0,
      "local_path": "string",
      "estimated_required_bytes": 0,
      "metadata_complete": false
    }
  ]
}
```

`estimated_required_bytes` is a cached discovery value. Callers that need an admission decision use the registry's `resolve()` or `list_models_fitting_budget()` APIs, which invoke the Phase 2 estimator again against the current runtime configuration.

## Metadata policy

The scanner must not guess model architecture, parameter count, or quantization from the filename.

For a GGUF file that can be inspected through llama.cpp metadata:

- architecture is read from `general.architecture`
- parameter count is obtained from the model API
- training context is obtained from the model API
- file size comes from the local filesystem
- RAM tier comes from the Phase 2 memory estimate

Quantization is deliberately reported as `unknown` when it cannot be safely extracted through the available public model metadata API. A caller can supply a declared quantization value using manual registration.

This avoids turning a filename convention into a false fact.

## Public registry API

Primary types:

```cpp
syj::core::RegistryConfig
syj::core::ModelEntry
syj::core::ResolvedModel
syj::core::ModelRegistry
```

Important operations:

```cpp
ModelRegistry registry(config);

registry.load_manifest();
registry.save_manifest();

registry.scan_directory("models", runtime_config);

registry.register_model(entry);

registry.get_model("model-name", entry);

registry.resolve("model-name", runtime_config, resolved);

registry.list_models_fitting_budget(
    budget.max_bytes,
    runtime_config,
    fitting_models);
```

`resolve()` and `list_models_fitting_budget()` reuse the Phase 2 public `Runtime::estimate_memory()` path. The registry does not duplicate the memory estimator.

## RAM-tier classification

Phase 3 uses the Phase 2 policy:

| Tier | Required runtime budget |
|---|---:|
| Edge | `<= 1.50 GiB` |
| Standard | `> 1.50 GiB` and `<= 2.50 GiB` |
| Large | `> 2.50 GiB` and `<= 4.00 GiB` |
| Above large | `> 4.00 GiB` |
| Unknown | no usable estimate |

These are admission/classification thresholds, not claims about total device RAM safety.

## CLI

```text
syj model list
syj model show <name>
syj model scan <directory>
```

The existing inference invocation remains:

```text
syj <model.gguf> <prompt>
```

No Studio HTTP API, dashboard, download manager, or remote registry command is added.

## Files added

- `include/syj/core/model_registry.hpp`
- `src/core/model_registry.cpp`
- `tests/model_registry_tests.cpp`
- `models/registry.json`

## Files modified

- `CMakeLists.txt`
- `src/cli/main.cpp`
- `include/syj/core/version.hpp`
- `README.md`
- `PHASE_NOTES.md`

Phase 1 and Phase 2 runtime/memory implementation files are intentionally not rewritten by Phase 3.

## Test coverage

The new CTest target covers:

- Edge/Standard/Large/Above-large boundary classification.
- JSON manifest write/read round trip.
- Manual metadata preservation.
- Local scan handling of non-GGUF files.
- Safe rejection/ignoring of an invalid GGUF fixture.

The existing targets remain:

```text
syj_core_tests
syj_memory_budget_tests
```

New target:

```text
syj_model_registry_tests
```

## Validation performed by the preparation environment

The implementation environment could not execute the project's CMake build against the user's vendored llama.cpp tree.

A public GitHub read check verified the Phase 2 baseline commit, tag state, and dependency pin, but that is not a build or hardware validation.

Therefore this artifact makes **no claim of a successful Phase 3 build, CTest run, real-GGUF scan, or ARM64 Termux result**.

## Required manual validation on ARM64 Termux

Extract the Phase 3 artifact over the existing repository checkout without deleting `third_party/llama.cpp`.

Then:

```sh
cd ~/SYJ-LLM

git rev-parse HEAD
git tag -l --sort=version:refname
cat third_party/llama.cpp/SYJ_LLAMA_VERSION

rm -rf build

cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DGGML_NATIVE=OFF

cmake --build build --parallel 1

ctest --test-dir build --output-on-failure
```

Expected test targets after a successful build:

```text
syj_core_tests
syj_memory_budget_tests
syj_model_registry_tests
```

### Registry fixture validation

Create a disposable local model directory:

```sh
mkdir -p ~/SYJ-LLM-REGISTRY-CHECK
```

Copy a real local GGUF into it, for example:

```sh
cp "$HOME/SYJ-EdgeMind/models/local/SmolLM2-135M-Instruct-Q4_K_M.gguf" \
  ~/SYJ-LLM-REGISTRY-CHECK/
```

Then scan:

```sh
./build/syj model scan ~/SYJ-LLM-REGISTRY-CHECK
```

Inspect:

```sh
./build/syj model list
```

The manifest should now contain a local entry under:

```text
models/registry.json
```

Then show it:

```sh
./build/syj model show \
  "SmolLM2-135M-Instruct-Q4_K_M"
```

### Phase 2 regression

Run the existing real inference path:

```sh
./build/syj \
  "$HOME/SYJ-EdgeMind/models/local/SmolLM2-135M-Instruct-Q4_K_M.gguf" \
  "What is 2 plus 2? Answer with only the number."
```

### Memory-admission regression

The registry must not bypass Phase 2 admission. Re-run the established real-model checks using the existing Phase 2 validation procedure and confirm:

- small model remains admissible at the 1.50 GiB budget;
- 3B model remains rejected at 1.50 GiB;
- 3B model remains admissible at 3.00 GiB.

## What remains unvalidated

Until the project owner runs the commands above, these are explicitly **not yet measured/validated for Phase 3**:

- Phase 3 compilation on ARM64 Termux.
- Phase 3 CTest result.
- Real GGUF discovery result.
- Real GGUF metadata values produced by the new registry.
- Registry scan runtime/RSS overhead.
- Phase 3 CLI behavior on the target device.
- Windows/macOS/Linux builds.

## Out of scope

Not implemented in Phase 3:

- Network model downloads.
- Remote registry synchronization.
- Studio API.
- Web dashboard.
- Fine-tuning.
- Agents/function calling.
- Packaging.
- iOS.
- macOS Metal.
- Android JNI.
- WASM.

## Phase gate

Do not proceed to Phase 4 until Phase 3 has been manually built, tested, real-GGUF scanned, regression-tested, committed, pushed, and confirmed by the repository owner.
