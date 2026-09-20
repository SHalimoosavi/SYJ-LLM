#!/usr/bin/env python3
import argparse, json
from pathlib import Path

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("dataset", type=Path)
    args = ap.parse_args()
    count = 0
    for n, line in enumerate(args.dataset.read_text(encoding="utf-8").splitlines(), 1):
        if not line.strip(): continue
        obj = json.loads(line)
        messages = obj.get("messages")
        if not isinstance(messages, list) or not messages:
            raise SystemExit(f"line {n}: messages must be a non-empty list")
        for msg in messages:
            if not isinstance(msg, dict) or msg.get("role") not in {"system","user","assistant"}:
                raise SystemExit(f"line {n}: invalid message role")
            if not isinstance(msg.get("content"), str) or not msg["content"].strip():
                raise SystemExit(f"line {n}: message content must be non-empty")
        count += 1
    if not count: raise SystemExit("dataset contains no examples")
    print(f"dataset_valid examples={count}")

if __name__ == "__main__":
    main()
