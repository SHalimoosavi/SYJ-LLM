#!/usr/bin/env bash
set -euo pipefail
MODEL_DIR="${1:?merged model directory required}"
OUTFILE="${2:?output GGUF path required}"
CONVERTER="third_party/llama.cpp/convert_hf_to_gguf.py"
[[ -d "$MODEL_DIR" ]] || { echo "ERROR: model directory not found: $MODEL_DIR" >&2; exit 2; }
[[ -f "$CONVERTER" ]] || { echo "ERROR: converter not found: $CONVERTER" >&2; exit 2; }
HELP="$(python3 "$CONVERTER" --help 2>&1 || true)"
grep -q -- "--outfile" <<<"$HELP" || { echo "ERROR: pinned converter does not advertise --outfile" >&2; exit 3; }
grep -q -- "--outtype" <<<"$HELP" || { echo "ERROR: pinned converter does not advertise --outtype" >&2; exit 3; }
python3 "$CONVERTER" "$MODEL_DIR" --outfile "$OUTFILE" --outtype f16
