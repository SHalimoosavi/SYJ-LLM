# Phase 3 artifact source note

This Phase 3 delivery is an overlay artifact because the preparation environment
cannot copy the repository's ~174 MB vendored `third_party/llama.cpp` source tree.

The artifact therefore does **not** claim to be a byte-for-byte self-contained
repository archive.

Extract it over the existing `~/SYJ-LLM` checkout at the verified Phase 2
baseline:

```text
c54ae4cf7563abb4a4c5d51a62c63863d295fbde
```

Do not delete or replace:

```text
third_party/llama.cpp/
```

The required dependency identity remains:

```text
391fac16460f15233a7740550d858ac96df3419d
```

All Phase 3 source and documentation changes required by the phase are included
in this artifact. The repository owner must perform the actual CMake build,
CTest run, real-GGUF discovery, and ARM64 Termux validation after extraction.
