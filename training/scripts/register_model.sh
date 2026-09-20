#!/usr/bin/env bash
set -euo pipefail

MODEL="${1:?GGUF model path required}"

if [[ ! -f "$MODEL" ]]; then
  echo "ERROR: GGUF not found: $MODEL" >&2
  exit 2
fi

BASENAME="$(basename "$MODEL")"

if [[ "$BASENAME" != "SYJ-Model-v1.gguf" ]]; then
  echo "ERROR: final Phase 5 artifact must be named SYJ-Model-v1.gguf" >&2
  echo "Actual: $BASENAME" >&2
  exit 3
fi

echo "Register the final artifact with the existing Phase 3 registry:"
echo
echo "./build/syj model scan \"$(dirname "$MODEL")\""
echo "./build/syj model show SYJ-Model-v1"
echo
echo "The registry derives the model name from SYJ-Model-v1.gguf."
echo "The Phase 2 estimator determines the RAM tier."
echo "Do not manually assign the RAM tier."
