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
        shader = shader.replace("tileOrigin", "brickOrigin")
        renamed += 1
    if shader_path.name == "reset.comp":
        shader = shader.replace("int q2 = dot(q, q);", "int q2 = q.x * q.x + q.y * q.y;")
    shader_path.write_text(shader, encoding="utf-8")

validator_path = root / "tools/validate_shader_contracts.py"
validator = validator_path.read_text(encoding="utf-8")
old = '".descriptorCount = 14",'
new = '".descriptorCount = 16",'
if old in validator:
    validator = validator.replace(old, new, 1)
elif new not in validator:
    raise SystemExit("shader validator has neither the 14- nor 16-descriptor contract")
validator_path.write_text(validator, encoding="utf-8")

print(f"Fix34 post-patch contracts ready: renamed {renamed} shader file(s); fixed integer hive distance; 16-descriptor chunk layout.")
