# SYJ LLM

**Offline-first, low-RAM LLM runtime, local GGUF registry, localhost API foundation, and external fine-tuning pipeline in C++17.**

SYJ is a native edge runtime built around an exact pinned `llama.cpp` commit. Phase 4 turns the Phase 1-3 core into a usable command-line interface and adds an optional embedded localhost HTTP layer for the future SYJ LLM Studio track.

## Phase 5 baseline

The project owner confirmed this baseline using local `git log`, `git ls-remote`, and `git show` checks. This documentation records that owner verification; it is not an independently reproduced Git history check by the artifact preparation environment.

```text
Commit: 721962eafe245ced6e9507027748dd7e25377697
Tag:    v0.4.0
History: v0.1.0 -> v0.2.0 -> v0.3.0 -> v0.4.0
llama.cpp: 391fac16460f15233a7740550d858ac96df3419d
```

## Status

**Current phase: Phase 5 — Fine-Tuning Pipeline**

Implemented:

- [x] Phase 0 bootstrap
- [x] Phase 1 native inference runtime
- [x] GGUF mmap loading and streaming generation
- [x] Phase 2 memory estimation and hard admission budget
- [x] Actionable `SYJ_ERROR_INSUFFICIENT_MEMORY`
- [x] Phase 3 local GGUF model registry
- [x] Registry metadata discovery and RAM-tier classification
- [x] Registry CLI list/show/scan
- [x] Phase 4 named-model CLI load/run commands
- [x] CLI memory-budget and context flags
- [x] Optional embedded localhost HTTP API
- [x] `/v1/models` registry endpoint
- [x] `/v1/models/load` memory-aware model load endpoint
- [x] `/v1/generate` SSE streaming endpoint
- [x] CLI/API share the same `ModelRegistry` and `Runtime` core APIs
- [x] API is localhost-only by default
- [x] CTest coverage for CLI behavior and API endpoint contracts
- [x] Phase 5 external LoRA/QLoRA fine-tuning pipeline tooling
- [x] Qwen/Qwen3-1.7B model-card and Apache-2.0 attribution package
- [x] HF-to-GGUF conversion and Q4_K_M quantization tooling
- [x] Phase 5 pipeline asset CTest coverage

Not implemented / later:

- [ ] SYJ LLM Studio web dashboard
- [ ] Public Web Playground exposure
- [ ] Authentication, multi-user access, rate limiting
- [ ] Remote model downloading/synchronization
- [ ] Final trained SYJ-Model-v1 artifact validation
- [ ] Agents/function calling
- [ ] Windows packaging
- [ ] Linux packaging/release artifacts
- [ ] iOS bridge/UI
- [ ] macOS/Metal integration
- [ ] Performance phase
- [ ] Android JNI
- [ ] Release candidate/open-source publication
- [ ] WASM/GitHub Pages runtime

Phase 4 was manually validated by the project owner on ARM64/Termux. The validation included a clean CMake build with Clang 21.1.8 and CMake 4.4.3, 5/5 CTest tests passing, real SmolLM2-135M-Instruct-Q4_K_M.gguf CLI loading and streamed generation, and the localhost HTTP API model-list, model-load, and SSE generation paths. The small-model validation used a 1.50 GiB configured memory budget with 1024-token context and observed approximately 156 MB current/peak RSS.

## Architecture

```text
Browser / future Studio
          |
          | HTTP localhost / SSE
          v
+---------------------------+
| SYJ Local API (optional)  |
| cpp-httplib + JSON        |
+-------------+-------------+
              |
              v
+---------------------------+
| SYJ Core                  |
| ModelRegistry + Runtime   |
| Phase 2 memory estimator  |
+-------------+-------------+
              |
              v
        pinned llama.cpp
              |
              v
          local GGUF
```

The HTTP layer is an optional add-on. `syj_core` has no HTTP-library dependency.

## HTTP library choice

Phase 4 uses the **already-vendored `cpp-httplib` 0.56.0 source inside the pinned llama.cpp tree**. This avoids adding Python, Node, Electron, or a separate runtime service. It is a small C++ HTTP server/client library, supports localhost HTTP and chunked content providers, and provides the exact streaming primitive needed for Server-Sent Events.

The API also uses the **already-vendored nlohmann JSON header** in the pinned llama.cpp tree for request/response serialization. Neither dependency is linked into `syj_core`.

No network fetch is performed by the Phase 4 build when the vendored llama.cpp tree is already present.

## Build

```sh
cd ~/SYJ-LLM
rm -rf build
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DGGML_NATIVE=OFF \
  -DSYJ_BUILD_HTTP_API=ON
cmake --build build --parallel 1
ctest --test-dir build --output-on-failure
```

If API support is intentionally not wanted:

```sh
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DGGML_NATIVE=OFF \
  -DSYJ_BUILD_HTTP_API=OFF
```

## CLI

List registered models:

```sh
./build/syj model list
```

Show one model:

```sh
./build/syj model show SmolLM2-135M-Instruct-Q4_K_M
```

Scan local GGUF files:

```sh
./build/syj model scan ~/models
```

Load a model by registry name with a hard memory budget:

```sh
./build/syj model load SmolLM2-135M-Instruct-Q4_K_M \
  --memory-budget 1572864000 \
  --context 1024
```

Run a streamed single prompt through the registry-resolved model:

```sh
./build/syj run SmolLM2-135M-Instruct-Q4_K_M \
  "What is 2 plus 2? Answer with only the number." \
  --memory-budget 1572864000 \
  --context 1024 \
  --max-tokens 32
```

The previous positional form remains supported:

```sh
./build/syj <model.gguf> "What is 2 plus 2?"
```

Errors continue to come from the Core `Status` path. A memory admission failure retains the Phase 2 structured form:

```text
SYJ_ERROR_INSUFFICIENT_MEMORY
Required: 2.32 GiB
Available budget: 1.46 GiB
Suggested: Reduce context from 1024 to 512, or use a smaller quantized model.
```

## Local HTTP API

Start the optional server. The default bind address is `127.0.0.1`:

```sh
./build/syj serve
```

Default:

```text
http://127.0.0.1:8080
```

### Endpoints

| Method | Endpoint | Purpose |
|---|---|---|
| GET | `/v1/models` | List registry models including RAM tier and local metadata |
| POST | `/v1/models/load` | Resolve a registry name, preflight memory, then load it |
| POST | `/v1/generate` | Run inference and stream SSE token events |

Load request example:

```sh
curl -sS http://127.0.0.1:8080/v1/models/load \
  -H 'Content-Type: application/json' \
  -d '{"name":"SmolLM2-135M-Instruct-Q4_K_M","memory_budget_bytes":1572864000,"context_size":1024,"max_output_tokens":32}'
```

SSE inference example:

```sh
curl -N http://127.0.0.1:8080/v1/generate \
  -H 'Content-Type: application/json' \
  -d '{"model":"SmolLM2-135M-Instruct-Q4_K_M","prompt":"What is 2 plus 2?"}'
```

The stream uses `text/event-stream`. Token events are emitted as `data: {"token":"..."}` and completion is signalled with `event: done`.

### Security boundary

The server is **localhost-only by default and has no authentication**. This is intentional for the local Studio foundation. Binding it to a non-loopback address is not a public deployment feature and should not be treated as one. Authentication, multi-user controls, rate limiting, and public Web Playground hardening belong to a later track.

## Memory tiers

| Tier | Required runtime budget |
|---|---:|
| Edge | <= 1.50 GiB |
| Standard | > 1.50 GiB and <= 2.50 GiB |
| Large | > 2.50 GiB and <= 4.00 GiB |
| Above large | > 4.00 GiB |

These are runtime classification/admission thresholds, not hardware safety certifications.

## Validated historical measurements

These are real Phase 2 ARM64 Termux measurements supplied by the project owner and carried forward without modification:

| Model | Configuration | Result |
|---|---|---|
| SmolLM2-135M-Instruct-Q4_K_M.gguf | 1024 context, 1.50 GiB budget | ~157.6 MB peak RSS; admitted |
| Llama-3.2-3B-Instruct-Q4_K_M.gguf | 1024 context, 1.50 GiB budget | rejected; estimated requirement ~2.32 GiB |
| Llama-3.2-3B-Instruct-Q4_K_M.gguf | 1024 context, 3.00 GiB budget | ~2.04 GiB peak RSS; admitted |

### Phase 4 owner validation

The following Phase 4 results were run on the project owner's ARM64/Termux environment:

| Item | Result |
|---|---|
| Platform | ARM64 / aarch64 Termux |
| Compiler | Clang 21.1.8 |
| CMake | 4.4.3 |
| CTest | 5/5 tests passed |
| Test model | SmolLM2-135M-Instruct-Q4_K_M.gguf |
| Model file size | 105,454,560 bytes |
| Context | 1024 tokens |
| Configured memory budget | 1,572,864,000 bytes (1.50 GiB) |
| Phase 4 estimated requirement | 158,442,521 bytes |
| Observed current/peak RSS during CLI load | 156,082,176 bytes |
| CLI generation | Successful; streamed output; exit 0 |
| Local API `/v1/models` | HTTP 200 |
| Local API `/v1/models/load` | HTTP 200 |
| Local API `/v1/generate` | HTTP 200; SSE token stream and `done` event observed |
| API bind address | `127.0.0.1:18080` during validation |

These are functional target-device validation results, not a latency or throughput benchmark.

## Developer / Company

**Syed Ali Hasan Moosavi**
Founder & CTO / Lead Developer
**SAYANJALI NEXUS PRIVATE LIMITED**

Repository: https://github.com/SHalimoosavi/SYJ-LLM

## Roadmap

| Phase | Scope | Status |
|---|---|---|
| 0 | Bootstrap & repository structure | Complete |
| 1 | Core inference runtime | Complete |
| 2 | Memory safety & budget management | Complete |
| 3 | Model registry & GGUF management | Complete |
| 4 | CLI enhancement + local API foundation | Complete |
| 5 | Fine-tuning pipeline | **Current** |
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

## License

Apache License 2.0. See `LICENSE`.
