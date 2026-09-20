# Phase 5 source references

- Qwen/Qwen3-1.7B model card:
  https://huggingface.co/Qwen/Qwen3-1.7B
- llama.cpp conversion interface at the pinned revision:
  https://github.com/ggml-org/llama.cpp/blob/391fac16460f15233a7740550d858ac96df3419d/convert_hf_to_gguf.py
- llama.cpp quantization source at the pinned revision:
  https://github.com/ggml-org/llama.cpp/tree/391fac16460f15233a7740550d858ac96df3419d/tools/quantize

Qwen/Qwen3-1.7B is listed as Apache-2.0 by the Qwen model repository.

The pinned llama.cpp converter source was inspected directly and the Phase 5
flags --outfile and --outtype were verified at commit
391fac16460f15233a7740550d858ac96df3419d.
