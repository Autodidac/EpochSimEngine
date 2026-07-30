#!/usr/bin/env python3
from pathlib import Path

root = Path(__file__).resolve().parents[1]

shader_suffixes = {".glsl", ".comp", ".frag", ".vert"}
renamed = 0
for shader_path in (root / "shaders").iterdir():
    if shader_path.suffix not in shader_suffixes:
        continue
    shader = shader_path.read_text(encoding="utf-8")
    if "tileOrigin" in shader:
        shader_path.write_text(shader.replace("tileOrigin", "brickOrigin"), encoding="utf-8")
        renamed += 1

validator_path = root / "tools/validate_shader_contracts.py"
validator = validator_path.read_text(encoding="utf-8")
old = '".descriptorCount = 14",'
new = '".descriptorCount = 16",'
if old in validator:
    validator = validator.replace(old, new, 1)
elif new not in validator:
    raise SystemExit("shader validator has neither the 14- nor 16-descriptor contract")
validator_path.write_text(validator, encoding="utf-8")

reset_lines = (root / "shaders/reset.comp").read_text(encoding="utf-8").splitlines()
for line_number in range(190, min(202, len(reset_lines) + 1)):
    print(f"RESET[{line_number}]: {reset_lines[line_number - 1]}")

print(f"Fix34 post-patch contracts ready: renamed {renamed} shader file(s); 16-descriptor chunk layout.")
