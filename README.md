# SYJ LLM

**SYJ — by SAYANJALI NEXUS PRIVATE LIMITED**

A lightweight, offline-first LLM and agent runtime designed around a shared C/C++ inference core.

Founder & CTO / Lead Developer: **Syed Ali Hasan Moosavi**

Repository: https://github.com/SHalimoosavi/SYJ-LLM

## Project direction

SYJ is planned as a cross-platform runtime for Windows, Linux, macOS, iOS, Android, and a browser/WASM demonstration target.

The native runtime will use a C/C++ core with llama.cpp and GGUF models. The inference core will not depend on Python, Node.js, Electron, cloud APIs, telemetry, or API keys.

## Phase 0 status

Phase 0 establishes a clean, independently buildable repository skeleton and a minimal C++17 core/CLI/test target. No model, llama.cpp source, network functionality, or agent functionality is included yet.

## Planned phases

0. Bootstrap & repo structure
1. Core inference runtime (C/C++, llama.cpp integration)
2. Memory safety & budget management
3. Model registry & GGUF loading
4. CLI interface
5. Model fine-tuning pipeline (SYJ-Model-v1)
5.5. Agent/function-calling layer (SYJ-Agents)
6. Windows packaging
6.5. Linux build target
7. iOS bridge
8. iOS native UI
8.5. macOS + Metal backend
9. Performance tuning
9.5. Android bridge (JNI)
10. Testing (unit, integration, cross-platform)
11. Release candidate & open-source publish
12. WASM build + GitHub Pages demo

## Repository principles

- Native runtime: C/C++ only.
- Tooling may use other languages, but must remain outside the inference core.
- Offline-first native execution.
- No telemetry by default.
- Explicit ownership of resources and deterministic cleanup.
- Small dependencies and portable build system.
- Every phase must build and test independently.
- Models are distributed separately from source code where appropriate.

## Build

Requirements:

- CMake 3.20+
- A C++17 compiler

### Linux / macOS

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/syj
```

### Windows PowerShell

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
.\build\Release\syj.exe
```

## License

Apache License 2.0. See `LICENSE`.
