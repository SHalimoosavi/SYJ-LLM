# Phase 4 third-party source note

The Phase 4 artifact intentionally does not duplicate the large, unchanged `third_party/llama.cpp` source tree.

The project baseline already contains the exact pinned llama.cpp tree required by this phase:

```text
391fac16460f15233a7740550d858ac96df3419d
```

Phase 4 additionally consumes the already-vendored sources inside that tree:

```text
third_party/llama.cpp/vendor/cpp-httplib/
third_party/llama.cpp/vendor/nlohmann/
```

Preserve the existing `third_party/llama.cpp` directory when extracting the Phase 4 artifact. Do not replace it with another llama.cpp version or an unpinned checkout.

This file is a packaging note for the Phase 4 source artifact; it is not a claim that the artifact contains the complete ~174 MB llama.cpp tree.
