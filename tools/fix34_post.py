#!/usr/bin/env python3
from pathlib import Path

root = Path(__file__).resolve().parents[1]

reset_path = root / "shaders/reset.comp"
reset = reset_path.read_text(encoding="utf-8")
if "tileOrigin" not in reset:
    raise SystemExit("Fix34 reset output is missing the expected brick-origin identifier")
reset_path.write_text(reset.replace("tileOrigin", "brickOrigin"), encoding="utf-8")

validator_path = root / "tools/validate_shader_contracts.py"
validator = validator_path.read_text(encoding="utf-8")
old = '".descriptorCount = 14",'
new = '".descriptorCount = 16",'
if old not in validator:
    raise SystemExit("shader validator is missing the pre-chunk descriptor count contract")
validator_path.write_text(validator.replace(old, new, 1), encoding="utf-8")

print("Fix34 post-patch contracts updated: brick naming and 16-descriptor chunk layout.")
