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
        'replace(\n    "tests/behavior_contract.cpp",\n    "static_assert(sizeof(epoch::sand::Cell) == 16u);",\n    "static_assert(sizeof(epoch::sand::Cell) == 16u);\\n"\n    "static_assert(epoch::sand::material_count == 66u);\\n"\n    "static_assert(epoch::sand::is_block_material(epoch::sand::Material::sluice_box));",\n)\n',
        'replace(\n    "tests/behavior_contract.cpp",\n    "static_assert(epoch::sand::policy::water_half_units_per_full_cell == 2u);",\n    "static_assert(epoch::sand::policy::water_half_units_per_full_cell == 2u);\\n"\n    "static_assert(epoch::sand::material_count == 66u);\\n"\n    "static_assert(epoch::sand::is_block_material(epoch::sand::Material::sluice_box));",\n)\n',
        "current behavior contract anchor",
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

root = path.parents[1]

# Preserve the established ecology audit vocabulary in the new cached classifier.
tiles_path = root / "shaders/tiles.comp"
tiles = tiles_path.read_text(encoding="utf-8")
old = """            bool loose = !isStructural(cell) && !isReconstructableMaterial(cell.material) &&
                         !isCellImmovable(cell);
            activeContent = activeContent || agent || loose || fluid;
"""
new = """            bool activeLoose = !isStructural(cell) && !isReconstructableMaterial(cell.material) &&
                               !isCellImmovable(cell);
            activeContent = activeContent || agent || activeLoose || fluid;
"""
if old not in tiles:
    raise SystemExit("tiles activeLoose compatibility marker missing")
tiles = tiles.replace(old, new, 1)
old = "bool sleepingTerrain = terrainStable && !damaged && !moving && !hot && !reacting && !activeAgent;"
new = "bool sleepingTerrain = terrainStable && !damaged && !moving && !hot && !reacting &&\n                           !activeAgent && !activeContent;"
if old not in tiles:
    raise SystemExit("tiles sleeping activeContent marker missing")
tiles = tiles.replace(old, new, 1)
tiles_path.write_text(tiles, encoding="utf-8", newline="\n")

# The movement shader deliberately contains no runtime loops. Keep the three-cell
# half-water attraction explicit so offline optimization and driver startup stay bounded.
move_path = root / "shaders/move.comp"
move = move_path.read_text(encoding="utf-8")
old = """bool halfWaterAhead(ivec2 position, int direction) {
    for (int distance = 2; distance <= 4; ++distance) {
        Cell candidate = sampleAt(position + ivec2(direction * distance, 0));
        if (isHalfWaterCell(candidate)) return true;
        if (!isOpenGas(candidate)) return false;
    }
    return false;
}
"""
new = """bool halfWaterAhead(ivec2 position, int direction) {
    Cell candidate2 = sampleAt(position + ivec2(direction * 2, 0));
    if (isHalfWaterCell(candidate2)) return true;
    if (!isOpenGas(candidate2)) return false;
    Cell candidate3 = sampleAt(position + ivec2(direction * 3, 0));
    if (isHalfWaterCell(candidate3)) return true;
    if (!isOpenGas(candidate3)) return false;
    Cell candidate4 = sampleAt(position + ivec2(direction * 4, 0));
    return isHalfWaterCell(candidate4);
}
"""
if old not in move:
    raise SystemExit("half-water unroll marker missing")
move = move.replace(old, new, 1)
move_path.write_text(move, encoding="utf-8", newline="\n")
