# SYJ-Model-v1

**Base model:** Qwen/Qwen3-1.7B
**Target format:** GGUF
**Target quantization:** Q4_K_M

SYJ-Model-v1 is intended to be a domain-specific fine-tuned derivative of
Qwen/Qwen3-1.7B. No trained model artifact is included in this source ZIP.

## Upstream attribution and license

Qwen/Qwen3-1.7B is currently listed by its publisher under the Apache License,
Version 2.0.

Copyright 2025 Qwen. All rights reserved.

Qwen/Qwen3-1.7B is licensed under the Apache License, Version 2.0.

The applicable Apache License, Version 2.0 text is included in
`LICENSE-APACHE-2.0.txt` in this directory.

When distributing a SYJ-Model-v1 derivative, retain the applicable upstream
copyright, license, attribution, patent, and trademark notices and provide
the Apache License text as required by that license.

The SYJ project does not claim ownership of the Qwen upstream work.

Subject to upstream Apache-2.0 obligations, SYJ-specific modifications and
documentation may be distributed under Apache License 2.0.

## Runtime target

The final GGUF must be tested through the unchanged Phase 1–4 stack:

SYJ CLI → SYJ Core → llama.cpp → GGUF
