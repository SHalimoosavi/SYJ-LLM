#!/usr/bin/env bash
set -euo pipefail
INPUT="${1:?input GGUF required}"
OUTPUT="${2:?output GGUF required}"
[[ -f "$INPUT" ]] || { echo "ERROR: input GGUF not found: $INPUT" >&2; exit 2; }
QUANT=""
for candidate in build/bin/llama-quantize build/bin/quantize build/llama-quantize build/quantize; do
  if [[ -x "$candidate" ]]; then QUANT="$candidate"; break; fi
done
[[ -n "$QUANT" ]] || { echo "ERROR: no pinned llama.cpp quantizer found" >&2; exit 3; }
"$QUANT" "$INPUT" "$OUTPUT" Q4_K_M
