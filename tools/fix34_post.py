#!/usr/bin/env python3
from pathlib import Path

root = Path(__file__).resolve().parents[1]

reset_path = root / "shaders/reset.comp"
reset = reset_path.read_text(encoding="utf-8")
if "tileOrigin" in reset:
    reset_path.write_text(reset.replace("tileOrigin", "brickOrigin"), encoding="utf-8")

validator_path = root / "tools/validate_shader_contracts.py"
validator = validator_path.read_text(encoding="utf-8")
old = '".descriptorCount = 14",'
new = '".descriptorCount = 16",'
if old in validator:
    validator = validator.replace(old, new, 1)
elif new not in validator:
    raise SystemExit("shader validator has neither the 14- nor 16-descriptor contract")
validator_path.write_text(validator, encoding="utf-8")

print("Fix34 post-patch contracts ready: brick naming and 16-descriptor chunk layout.")
