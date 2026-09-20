#!/usr/bin/env python3
import argparse, yaml
from pathlib import Path

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("config", type=Path)
    args = ap.parse_args()
    data = yaml.safe_load(args.config.read_text(encoding="utf-8"))
    for key in ("base_model","output_name","dataset_format","max_seq_length"):
        if key not in data: raise SystemExit(f"missing config key: {key}")
    if data["base_model"] != "Qwen/Qwen3-1.7B":
        raise SystemExit("base_model must remain Qwen/Qwen3-1.7B")
    print("config_valid")

if __name__ == "__main__":
    main()
