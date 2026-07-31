#!/usr/bin/env python3
from pathlib import Path

path = Path(__file__).with_name("apply_fix35_macro_hierarchy.py")
source = path.read_text(encoding="utf-8")
source = source.replace(
    '        count=3)\nreplace(\n    "shaders/materials.glsl",',
    '        count=1)\nreplace(\n    "shaders/materials.glsl",',
    1,
)
if source == path.read_text(encoding="utf-8"):
    raise SystemExit("materials marker correction was not applied")
namespace = {"__file__": str(path), "__name__": "__main__"}
exec(compile(source, str(path), "exec"), namespace)
