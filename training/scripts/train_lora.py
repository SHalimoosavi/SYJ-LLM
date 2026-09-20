#!/usr/bin/env python3
"""Train Qwen3-1.7B with LoRA/QLoRA using Transformers + PEFT.

Training only. This script is never imported by syj_core and is not part of
the C/C++ inference runtime.
"""
import argparse
from pathlib import Path
import yaml
import torch
from datasets import load_dataset
from transformers import (
    AutoModelForCausalLM,
    AutoTokenizer,
    BitsAndBytesConfig,
    DataCollatorForLanguageModeling,
    Trainer,
    TrainingArguments,
)
from peft import LoraConfig, get_peft_model, prepare_model_for_kbit_training

def load_cfg(path):
    return yaml.safe_load(Path(path).read_text(encoding="utf-8"))

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--config", required=True, type=Path)
    ap.add_argument("--dataset", required=True, type=Path)
    ap.add_argument("--output-dir", required=True, type=Path)
    ap.add_argument("--qlora", action="store_true",
                    help="load the base model in 4-bit NF4; requires bitsandbytes")
    args = ap.parse_args()

    cfg = load_cfg(args.config)
    base = cfg["base_model"]
    if base != "Qwen/Qwen3-1.7B":
        raise SystemExit(f"unsupported base model: {base}")

    max_len = int(cfg.get("max_seq_length", 1024))
    train_cfg = cfg["training"]
    lora_cfg = cfg["lora"]

    ds = load_dataset("json", data_files={"train": str(args.dataset)})["train"]
    tokenizer = AutoTokenizer.from_pretrained(base, use_fast=True)
    if tokenizer.pad_token is None:
        tokenizer.pad_token = tokenizer.eos_token

    def format_example(example):
        text = tokenizer.apply_chat_template(
            example["messages"],
            tokenize=False,
            add_generation_prompt=False,
        )
        return {"text": text}

    ds = ds.map(format_example)

    def tokenize(batch):
        return tokenizer(
            batch["text"],
            truncation=True,
            max_length=max_len,
            padding=False,
        )

    tokenized = ds.map(tokenize, batched=True, remove_columns=ds.column_names)

    kwargs = {"torch_dtype": torch.bfloat16 if train_cfg.get("bf16", True) else torch.float16}
    if args.qlora:
        if not torch.cuda.is_available():
            raise SystemExit("QLoRA requires a CUDA GPU in this Phase 5 pipeline.")
        kwargs["quantization_config"] = BitsAndBytesConfig(
            load_in_4bit=True,
            bnb_4bit_quant_type="nf4",
            bnb_4bit_compute_dtype=torch.bfloat16,
            bnb_4bit_use_double_quant=True,
        )
        kwargs["device_map"] = {"": 0}

    model = AutoModelForCausalLM.from_pretrained(base, **kwargs)
    model.config.use_cache = False

    if args.qlora:
        model = prepare_model_for_kbit_training(model)

    peft_cfg = LoraConfig(
        r=int(lora_cfg["r"]),
        lora_alpha=int(lora_cfg["alpha"]),
        lora_dropout=float(lora_cfg["dropout"]),
        bias="none",
        task_type="CAUSAL_LM",
        target_modules=list(lora_cfg["target_modules"]),
    )
    model = get_peft_model(model, peft_cfg)
    model.print_trainable_parameters()

    out = args.output_dir
    out.mkdir(parents=True, exist_ok=True)

    training_args = TrainingArguments(
        output_dir=str(out),
        num_train_epochs=float(train_cfg["num_train_epochs"]),
        per_device_train_batch_size=int(train_cfg["per_device_train_batch_size"]),
        gradient_accumulation_steps=int(train_cfg["gradient_accumulation_steps"]),
        learning_rate=float(train_cfg["learning_rate"]),
        logging_steps=int(train_cfg["logging_steps"]),
        save_steps=int(train_cfg["save_steps"]),
        bf16=bool(train_cfg.get("bf16", True)),
        fp16=not bool(train_cfg.get("bf16", True)),
        gradient_checkpointing=bool(train_cfg.get("gradient_checkpointing", True)),
        report_to="none",
        remove_unused_columns=False,
    )

    trainer = Trainer(
        model=model,
        args=training_args,
        train_dataset=tokenized,
        data_collator=DataCollatorForLanguageModeling(tokenizer=tokenizer, mlm=False),
    )
    trainer.train()
    model.save_pretrained(out)
    tokenizer.save_pretrained(out)
    print(f"adapter_saved={out}")

if __name__ == "__main__":
    main()
