#!/usr/bin/env sh
set -eu

REV=391fac16460f15233a7740550d858ac96df3419d

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
DEST="$ROOT/third_party/llama.cpp"
TMP="${TMPDIR:-/tmp}/syj-llama-$REV"

rm -rf "$TMP"
mkdir -p "$TMP"

echo "Fetching llama.cpp v0.4.1 ($REV) ..."

git clone \
    --filter=blob:none \
    --no-checkout \
    https://github.com/ggml-org/llama.cpp.git \
    "$TMP/repo"

cd "$TMP/repo"

git fetch --depth 1 origin "$REV"
git checkout --detach "$REV"

rm -rf "$DEST"
mkdir -p "$DEST"

git archive "$REV" | tar -x -C "$DEST"

printf '%s\n' "$REV" > "$DEST/SYJ_LLAMA_REVISION"
printf '%s\n' "v0.4.1" > "$DEST/SYJ_LLAMA_VERSION"

echo "Vendored llama.cpp v0.4.1 at $REV"
