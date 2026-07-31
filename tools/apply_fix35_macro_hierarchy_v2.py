#!/usr/bin/env python3
from pathlib import Path

path = Path(__file__).with_name("apply_fix35_macro_hierarchy.py")
original = path.read_text(encoding="utf-8")
source = original

corrections = (
    (
        '        count=3)\nreplace(\n    "shaders/materials.glsl",',
        '        count=1)\nreplace(\n    "shaders/materials.glsl",',
        "materials suffix count",
    ),
    (
        '    count=3,\n)\n\n# Wet sand/silt and the sluice machine',
        '    count=2,\n)\n\n# Wet sand/silt and the sluice machine',
        "move machine suffix count",
    ),
    (
        'replace(\n    "shaders/chemistry.comp",\n    "           material == MAT_ASSEMBLER || material == MAT_INSECT_HABITAT ||\\n           material == MAT_FACTORY_CORE;",\n    "           material == MAT_ASSEMBLER || material == MAT_INSECT_HABITAT ||\\n"\n    "           material == MAT_FACTORY_CORE || material == MAT_SLUICE_BOX;",\n    count=2,\n)\n',
        '',
        "nonexistent chemistry machine suffix",
    ),
    (
        'replace(\n    "HALF_WATER.md",\n    "Half-water",\n    "Half-water",\n)\n',
        '',
        "half-water no-op assertion",
    ),
    (
        'replace(\n    "tools/validate_shader_contracts.py",\n    "    for token in (\\"oxygenVolume > 0u\\", \\"fullyChoked\\", \\"state.health -= 1u\\"):",\n    "    for token in (\\"oxygenVolume > 0u\\", \\"fullyChoked\\", \\"state.health -= 1u\\"):",\n)\n',
        '',
        "validator no-op assertion",
    ),
)

for old, new, label in corrections:
    if old not in source:
        raise SystemExit(f"missing correction marker: {label}")
    source = source.replace(old, new, 1)

if source == original:
    raise SystemExit("no macro hierarchy corrections were applied")

namespace = {"__file__": str(path), "__name__": "__main__"}
exec(compile(source, str(path), "exec"), namespace)
