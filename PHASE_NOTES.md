# SYJ LLM — Phase 4 Notes

## Baseline

Phase 4 starts from the project-owner-confirmed Phase 3 release baseline:

```text
Repository: SHalimoosavi/SYJ-LLM
Commit:     431c7b2872064a1d5525a8538c287f321d4ad583
Tag:        v0.3.0
History:    v0.1.0 -> v0.2.0 -> v0.3.0
llama.cpp:  391fac16460f15233a7740550d858ac96df3419d
```

The project owner independently verified this state with `git log`, `git ls-remote`, and `git show`. The preparation environment trusts that verification and does not claim an independent local checkout of the repository.

## Phase 4 objective

Phase 4 has two connected layers:

1. A real CLI interface over the existing Phase 1-3 Core APIs.
2. An optional thin localhost HTTP API foundation for the future Studio track.

The HTTP layer does not become part of `syj_core`. Both CLI and HTTP call the same `ModelRegistry` and `Runtime` APIs.

## Files added

```text
include/syj/api/local_api.hpp
src/api/local_api.cpp
include/syj/core/cli.hpp
src/cli/commands.cpp
tests/cli_tests.cpp
tests/local_api_tests.cpp
```

## Files modified

```text
CMakeLists.txt
src/cli/main.cpp
include/syj/core/version.hpp
README.md
PHASE_NOTES.md
```

The Phase 1 runtime, Phase 2 memory estimator, and Phase 3 registry implementation are not replaced by this phase.

## HTTP library choice

Phase 4 uses the already-vendored **cpp-httplib 0.56.0** source located under the pinned llama.cpp tree:

```text
third_party/llama.cpp/vendor/cpp-httplib/
```

The exact HTTP implementation is compiled into a separate `syj_http_api` library. `syj_core` does not include or link cpp-httplib.

JSON serialization uses the already-vendored nlohmann JSON header:

```text
third_party/llama.cpp/vendor/nlohmann/json.hpp
```

This choice preserves the offline/no-Python/no-Node/no-Electron runtime rule and avoids introducing a network package manager or another runtime dependency. The API layer is optional through:

```text
-DSYJ_BUILD_HTTP_API=ON|OFF
```

## API endpoint contract

### GET `/v1/models`

Returns the current local registry entries. Each model includes its name, architecture, parameter count, quantization declaration, file size, RAM tier, context support, local path, cached estimate, and metadata completeness flag.

### POST `/v1/models/load`

Request JSON:

```json
{
  "name": "SmolLM2-135M-Instruct-Q4_K_M",
  "memory_budget_bytes": 1572864000,
  "context_size": 1024,
  "max_output_tokens": 32
}
```

The server resolves the model through `ModelRegistry`, performs the Phase 2 metadata-only memory preflight, and then loads through the existing `Runtime` API. Memory rejection retains the structured Phase 2 error message.

### POST `/v1/generate`

Request JSON:

```json
{
  "model": "SmolLM2-135M-Instruct-Q4_K_M",
  "prompt": "What is 2 plus 2?"
}
```

The response is `text/event-stream`. Token events use:

```text
data: {"token":"..."}
```

Completion:

```text
event: done
data: {"ok":true}
```

Generation errors are sent as an `event: error` SSE message containing the Core status code and human-readable message.

If the requested model is not loaded, `/v1/generate` returns HTTP 409. If a model is included and differs from the loaded model, the same registry resolve + Core load path is used before generation.

## Local security boundary

The default server address is:

```text
127.0.0.1:8080
```

There is no authentication, multi-user isolation, rate limiting, or public deployment hardening in Phase 4. This is explicitly a local API foundation for the future Studio track. Public "SYJ Web Playground" exposure is a later security track and is not implied by this release.

## CLI contract

```text
syj model list
syj model show <name>
syj model scan <directory>
syj model load <name> [--memory-budget BYTES] [--context TOKENS]
syj run <name> <prompt> [--memory-budget BYTES] [--context TOKENS] [--max-tokens TOKENS]
syj serve [--host 127.0.0.1] [--port 8080]
```

The existing positional form remains accepted:

```text
syj <model.gguf> <prompt>
```

CLI load/run resolves model names using the Phase 3 registry and calls `Runtime::load_model()` after the same registry preflight. No duplicate memory estimator was introduced.

## CMake targets

Existing targets retained:

```text
syj_core
syj_core_tests
syj_memory_budget_tests
syj_model_registry_tests
```

Phase 4 targets:

```text
syj_http_api
syj_cli_tests
syj_api_tests
```

The CLI executable remains:

```text
syj
```

## Test coverage

### CLI tests

`tests/cli_tests.cpp` verifies argument-validation behavior in the command layer without requiring a model file.

### API tests

`tests/local_api_tests.cpp` verifies, against a real local HTTP server:

- `/v1/models` returns a JSON registry response;
- missing-model load returns HTTP 404 with a structured error;
- when `SYJ_TEST_MODEL` is set, the test creates a local registry entry and exercises real model load and SSE generation.

If `SYJ_TEST_MODEL` is not set, the real-GGUF load/generate portion is explicitly skipped rather than fabricated.

## Validation status

Phase 4 was manually validated by the project owner on the target ARM64/Termux environment after extraction over the verified Phase 3 checkout.

Validation environment:
- Platform: ARM64 / aarch64 Termux
- Compiler: Clang 21.1.8
- CMake: 4.4.3
- llama.cpp dependency remains pinned to `391fac16460f15233a7740550d858ac96df3419d`

Observed Phase 4 results:
- Clean CMake build completed successfully.
- 5/5 CTest tests passed.
- Real `SmolLM2-135M-Instruct-Q4_K_M.gguf` was discovered through the Phase 3 registry.
- CLI model load succeeded with a 1.50 GiB configured budget and 1024-token context.
- Estimated requirement: `158442521` bytes.
- Observed current/peak RSS during CLI load: `156082176` bytes.
- CLI streamed real model generation successfully with exit code 0.
- Local API `/v1/models` returned HTTP 200.
- Local API `/v1/models/load` returned HTTP 200.
- Local API `/v1/generate` returned HTTP 200 with SSE token events followed by the `done` event.
- API validation was performed on localhost at `127.0.0.1:18080`.

These are functional target-device validation results. No latency or throughput benchmark is claimed.

No Git commit, tag, or push was performed by the preparation environment; repository history changes remain the project owner's responsibility.

## Required ARM64 / Termux validation

The following commands are the authoritative manual validation procedure.

### 1. Extract over the Phase 3 checkout

Keep the existing `third_party/llama.cpp` tree intact. Extract this Phase 4 artifact into the repository root.

Then verify:

```sh
cd ~/SYJ-LLM

git rev-parse HEAD
git tag -l --sort=version:refname
cat third_party/llama.cpp/SYJ_LLAMA_VERSION

git diff --check
```

Before committing, the working tree should contain only the intended Phase 4 changes.

### 2. Clean configure/build

```sh
rm -rf build
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DGGML_NATIVE=OFF \
  -DSYJ_BUILD_HTTP_API=ON
cmake --build build --parallel 1
```

### 3. Run all tests

```sh
ctest --test-dir build --output-on-failure
```

Expected test names:

```text
syj_core_tests
syj_memory_budget_tests
syj_model_registry_tests
syj_cli_tests
syj_api_tests
```

Do not replace the actual output with these expected names if a test fails.

### 4. Phase 3 registry regression

```sh
rm -f models/registry.json
printf '{\n  "schema_version": 1,\n  "models": []\n}\n' > models/registry.json

mkdir -p ~/SYJ-LLM-REGISTRY-CHECK
cp "$HOME/SYJ-EdgeMind/models/local/SmolLM2-135M-Instruct-Q4_K_M.gguf" \
  ~/SYJ-LLM-REGISTRY-CHECK/

./build/syj model scan ~/SYJ-LLM-REGISTRY-CHECK
./build/syj model list
./build/syj model show SmolLM2-135M-Instruct-Q4_K_M
```

### 5. CLI named-model load/run

For the previously validated small model budget:

```sh
./build/syj model load SmolLM2-135M-Instruct-Q4_K_M \
  --memory-budget 1572864000 \
  --context 1024

./build/syj run SmolLM2-135M-Instruct-Q4_K_M \
  "What is 2 plus 2? Answer with only the number." \
  --memory-budget 1572864000 \
  --context 1024 \
  --max-tokens 32
```

### 6. Structured insufficient-memory regression

Use the known 3B model:

```sh
./build/syj model scan "$HOME/llama.cpp/models"
```

If the registry entry name is different in the local scan, use `./build/syj model list` to obtain the exact registered name.

Then run the established 1.50 GiB admission case with that name:

```sh
./build/syj model load "<3B-registered-name>" \
  --memory-budget 1572864000 \
  --context 1024
```

The command should report the existing `SYJ_ERROR_INSUFFICIENT_MEMORY` structure if the same Phase 2 configuration is used.

### 7. Start localhost API

Terminal 1:

```sh
cd ~/SYJ-LLM
./build/syj serve
```

Terminal 2:

```sh
curl -sS http://127.0.0.1:8080/v1/models
```

### 8. API model load

```sh
curl -sS http://127.0.0.1:8080/v1/models/load \
  -H 'Content-Type: application/json' \
  -d '{"name":"SmolLM2-135M-Instruct-Q4_K_M","memory_budget_bytes":1572864000,"context_size":1024,"max_output_tokens":32}'
```

### 9. API SSE inference

```sh
curl -N http://127.0.0.1:8080/v1/generate \
  -H 'Content-Type: application/json' \
  -d '{"model":"SmolLM2-135M-Instruct-Q4_K_M","prompt":"What is 2 plus 2? Answer with only the number."}'
```

Confirm that the response is `text/event-stream`, token events arrive incrementally, and a final `event: done` or explicit `event: error` is received.

### 10. Real-model API CTest

To exercise the optional real-GGUF section of `syj_api_tests`:

```sh
SYJ_TEST_MODEL="$HOME/SYJ-EdgeMind/models/local/SmolLM2-135M-Instruct-Q4_K_M.gguf" \
ctest --test-dir build --output-on-failure -R syj_api_tests
```

This is the manual target-hardware validation for the API's model-load and SSE path.

## What is validated vs what remains owner-controlled

| Item | Phase 4 status |
|---|---|
| Phase 3 baseline identity | Verified by project owner |
| llama.cpp exact hash | `391fac16460f15233a7740550d858ac96df3419d` |
| Phase 4 CMake build | **Validated on ARM64/Termux** |
| CLI compile/behavior | **Validated on ARM64/Termux** |
| HTTP compile/server | **Validated on ARM64/Termux** |
| CTest | **5/5 passed** |
| Real GGUF CLI generation | **Validated with SmolLM2-135M-Instruct-Q4_K_M.gguf** |
| Real GGUF API load/generation | **Validated with localhost API and SSE** |
| ARM64 RSS measurement | **Validated; 156,082,176 bytes observed during CLI load** |
| Latency/throughput benchmark | **Not performed; not claimed** |
| Git commit/tag/push | **Owner-controlled; not performed by preparation environment** |

## Out of scope

Phase 4 does not add:

- HTML/JS/React Studio dashboard
- public Web Playground deployment
- authentication
- multi-user management
- rate limiting
- remote model downloads
- fine-tuning
- agents/function calling
- packaging
- iOS
- macOS Metal
- Android JNI
- WASM

## Roadmap

| Phase | Scope | Status |
|---|---|---|
| 0 | Bootstrap & repository structure | Complete |
| 1 | Core inference runtime | Complete |
| 2 | Memory safety & budget management | Complete |
| 3 | Model registry & GGUF management | Complete |
| 4 | CLI enhancement + local API foundation | **Current** |
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

## Phase gate

Do not proceed to the Studio dashboard track until Phase 4 has been manually built, all applicable tests have been run, real small-model CLI/API validation has been performed on the target environment, the changes have been committed and pushed by the project owner, and the owner confirms acceptance.

# SYJ LLM — Phase 5 Notes

## Baseline

Phase 5 starts from the project-owner-confirmed Phase 4 release baseline:

Repository: SHalimoosavi/SYJ-LLM
Commit: 721962eafe245ced6e9507027748dd7e25377697
Tag: v0.4.0
History: v0.1.0 -> v0.2.0 -> v0.3.0 -> v0.4.0
llama.cpp: 391fac16460f15233a7740550d858ac96df3419d

## Objective

Phase 5 adds an external training pipeline for producing the
SYJ-Model-v1 model artifact while preserving the existing SYJ C/C++
runtime boundary.

The runtime remains Python-free. Python is used only by the training
and model-conversion tooling.

## Training pipeline

Dataset JSONL
    |
    v
Dataset validation
    |
    v
Qwen/Qwen3-1.7B base model
    |
    v
LoRA / QLoRA fine-tuning
    |
    v
LoRA adapter
    |
    v
Adapter merge
    |
    v
Merged Hugging Face model
    |
    v
llama.cpp HF -> GGUF conversion
    |
    v
F16 GGUF
    |
    v
Q4_K_M quantization
    |
    v
SYJ-Model-v1.gguf
    |
    v
Existing Phase 3 registry
    |
    v
Existing Phase 2 memory estimator
    |
    v
Existing Phase 4 CLI/API validation

## Base model

Target training base:

Qwen/Qwen3-1.7B

The Phase 5 model card contains the upstream attribution and Apache
License 2.0 notice required for the base model.

Phase 5 does not distribute trained weights or claim that a trained
SYJ-Model-v1 artifact has been produced by the preparation package.

## Training configuration

The supplied smoke-test configuration uses:

method: qlora
epochs: 1
batch size: 1
gradient accumulation: 16
learning rate: 0.0002
max sequence length: 1024

LoRA:
r: 16
alpha: 32
dropout: 0.05

Training requires a suitable GPU/cloud/desktop environment. The
Android/Termux runtime environment is not treated as a training
environment.

## Exact llama.cpp converter qualification

Phase 5 uses the existing pinned llama.cpp source:

391fac16460f15233a7740550d858ac96df3419d

The converter source at that pinned revision was inspected and confirms
the interfaces used by the Phase 5 conversion script:

--outfile
--outtype

The conversion step requests:

--outtype f16

The conversion script also performs a local --help check before
execution.

Actual conversion execution remains owner-controlled and requires the
Python dependencies needed by the pinned converter.

## Quantization

The Phase 5 pipeline targets:

Q4_K_M

The pinned llama.cpp source contains the llama-quantize target and
supports the requested Q4_K_M quantization path.

## Final artifact

The required final Phase 5 artifact name is:

SYJ-Model-v1.gguf

The existing Phase 3 registry is used for registration. The registry
derives the model identity from the GGUF filename and calculates the
RAM tier using the existing Phase 2 estimator.

The RAM tier is not manually assigned by Phase 5.

## Runtime boundary

Phase 5 does not modify:

- syj_core inference architecture
- Phase 2 memory-budget enforcement
- Phase 3 registry architecture
- Phase 4 HTTP API architecture
- existing llama.cpp pin
- existing C/C++ inference path

The final model must be validated through the same existing runtime
path used by earlier phases.

## Phase 5 validation

Phase 5 includes:

- dataset validation
- training configuration validation
- training-script interface checks
- converter interface checks
- model registration tooling checks
- CTest coverage for Phase 5 assets
- existing Phase 1-4 regression coverage

The training and conversion steps require an appropriate external
training environment and are not expected to execute on the low-RAM
Android/Termux validation device.

## Acceptance boundary

Phase 5 is not considered fully validated merely because the pipeline
scripts exist.

Final acceptance requires owner-controlled execution of the training,
merge, GGUF conversion, Q4_K_M quantization, registry registration, and
runtime validation of the resulting SYJ-Model-v1.gguf.

## Out of scope

Phase 5 does not add:

- agents
- function calling
- Studio dashboard
- multi-user authentication
- remote model downloading
- Windows packaging
- Linux packaging
- iOS
- macOS Metal
- Android JNI
- WASM
- production cloud training infrastructure
