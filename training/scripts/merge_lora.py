#!/usr/bin/env python3
"""Merge a completed PEFT LoRA adapter into the base model."""
import argparse
from pathlib import Path
import torch
from transformers import AutoModelForCausalLM, AutoTokenizer
from peft import PeftModel

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--base-model", required=True)
    ap.add_argument("--adapter-dir", required=True, type=Path)
    ap.add_argument("--output-dir", required=True, type=Path)
    args = ap.parse_args()

    if not args.adapter_dir.exists():
        raise SystemExit(f"adapter directory not found: {args.adapter_dir}")

    dtype = torch.bfloat16 if torch.cuda.is_available() else torch.float32
    model = AutoModelForCausalLM.from_pretrained(args.base_model, torch_dtype=dtype)
    model = PeftModel.from_pretrained(model, str(args.adapter_dir))
    model = model.merge_and_unload()
    args.output_dir.mkdir(parents=True, exist_ok=True)
    model.save_pretrained(args.output_dir, safe_serialization=True)
    AutoTokenizer.from_pretrained(args.base_model).save_pretrained(args.output_dir)
    print(f"merged_model_saved={args.output_dir}")

if __name__ == "__main__":
    main()
