# SYJ-LLM — Phase 0 Notes

## Phase

**Phase 0 — Bootstrap & Repository Structure**

## Objective

Establish a clean, portable, independently buildable C++ foundation for SYJ without introducing the llama.cpp dependency yet.

## License decision

**Apache License 2.0** is selected for the project. It provides a permissive open-source license plus an explicit patent license, which is useful for a systems/ML runtime intended for reuse across commercial and non-commercial applications.

## Files added

- `CMakeLists.txt`
- `LICENSE`
- `README.md`
- `PHASE_NOTES.md`
- `include/syj/core/version.hpp`
- `src/core/version.cpp`
- `src/cli/main.cpp`
- `tests/core_tests.cpp`
- `cmake/.gitkeep`
- `docs/.gitkeep`
- `models/.gitkeep`
- `third_party/llama.cpp/.gitkeep`
- `platform/windows/.gitkeep`
- `platform/linux/.gitkeep`
- `platform/macos/.gitkeep`
- `platform/ios/.gitkeep`
- `platform/android/.gitkeep`
- `platform/wasm/.gitkeep`
- `tools/.gitkeep`
- `.github/workflows/.gitkeep`

## Files modified

None. The target GitHub repository was empty at Phase 0 inspection time.

## Architecture established

```text
SYJ-LLM/
├── include/syj/core/       Public C++ API headers
├── src/core/               Core runtime implementation
├── src/cli/                Native CLI entry point
├── tests/                   CTest-backed native tests
├── third_party/llama.cpp/  Reserved for pinned llama.cpp integration
├── models/                 Local/downloaded model artifacts; not source
├── platform/               Platform-specific adapters/build material
│   ├── windows/
│   ├── linux/
│   ├── macos/
│   ├── ios/
│   ├── android/
│   └── wasm/
├── tools/                  Non-runtime developer tooling
├── cmake/                  Future reusable CMake modules
├── docs/                   Architecture and developer documentation
└── .github/workflows/      Future CI definitions
```

## Build prerequisites

- CMake 3.20 or newer
- C++17-capable compiler
- Git

No Python, Node.js, Electron, llama.cpp checkout, model, network service, or API key is required for Phase 0.

## Linux / macOS validation

From the extracted repository root:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/syj
```

Expected CLI output includes:

```text
SYJ LLM runtime
Version: 0.1.0
Vendor: SAYANJALI NEXUS PRIVATE LIMITED
Stage: Phase 0 - Bootstrap
Status: bootstrap OK
```

## Windows validation

PowerShell:

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
.\build\Release\syj.exe
```

## Memory-safety baseline

Phase 0 deliberately keeps the runtime tiny. There is no model allocation, tokenizer, thread pool, GPU context, mmap region, or network resource yet.

The code uses:

- C++17 standard library types
- No raw dynamic allocation
- No global mutable runtime state
- A static-library boundary for the future inference core
- CTest for an executable smoke/unit test

Later phases must preserve RAII, explicit ownership, bounded allocations, checked integer conversions, and deterministic shutdown.

## Performance baseline

The Phase 0 binary is intentionally minimal. Performance optimization begins only after the actual inference path exists; premature optimization here would create architectural coupling without measurable benefit.

## GitHub validation before push

The repository was checked as `SHalimoosavi/SYJ-LLM`; it is public, uses `main` as the default branch, and had repository size 0 at inspection time.

After extraction:

```sh
git status
git diff --check
```

Then perform the platform build/test commands above.

## What to verify before pushing

1. `cmake -S . -B build` succeeds.
2. The project compiles with the selected C++17 compiler.
3. `ctest` reports `100% tests passed`.
4. The `syj` executable prints the expected bootstrap status.
5. `git diff --check` reports no whitespace errors.
6. No generated `build/` directory is accidentally added to Git.
7. No model binaries are added to the Phase 0 commit.

## Intentionally deferred

- llama.cpp integration
- GGUF loading
- mmap/model memory management
- tokenizer and generation APIs
- model registry
- agent/function calling
- GPU backends
- mobile bridges
- WASM/Emscripten build
- model training/fine-tuning
- packaging and installers

These belong to later phases and must not be mixed into Phase 0.
