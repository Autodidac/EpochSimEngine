from __future__ import annotations

from pathlib import Path


def read(path: str) -> str:
    return Path(path).read_text(encoding="utf-8")


def write(path: str, text: str) -> None:
    Path(path).write_text(text, encoding="utf-8")


def replace_once(path: str, old: str, new: str) -> None:
    text = read(path)
    if text.count(old) != 1:
        raise RuntimeError(f"Expected one marker in {path}, found {text.count(old)}: {old[:140]!r}")
    write(path, text.replace(old, new, 1))


replace_once(
    "shaders/materials.glsl",
    "const uint AUX_MOVED = 0x01000000u;\n"
    "const uint AUX_STATE_MASK = 0x000000ffu;\n"
    "const uint AUX_RANDOM_MASK = 0x00ffff00u;\n",
    "const uint AUX_MOVED = 0x01000000u;\n"
    "const uint AUX_WATER_HALF = 0x00800000u;\n"
    "const uint AUX_STATE_MASK = 0x000000ffu;\n"
    "const uint AUX_RANDOM_MASK = 0x007fff00u;\n",
)
replace_once(
    "shaders/materials.glsl",
    "struct Cell {\n"
    "    uint material;\n"
    "    uint age;\n"
    "    int temperature;\n"
    "    uint aux;\n"
    "};\n",
    "struct Cell {\n"
    "    uint material;\n"
    "    uint age;\n"
    "    int temperature;\n"
    "    uint aux;\n"
    "};\n\n"
    "bool isHalfWater(Cell cell) {\n"
    "    return cell.material == MAT_WATER && (cell.aux & AUX_WATER_HALF) != 0u;\n"
    "}\n\n"
    "uint waterHalfUnits(Cell cell) {\n"
    "    return cell.material == MAT_WATER ? (isHalfWater(cell) ? 1u : 2u) : 0u;\n"
    "}\n\n"
    "void setHalfWater(inout Cell cell, bool halfState) {\n"
    "    if (cell.material != MAT_WATER) return;\n"
    "    if (halfState) cell.aux |= AUX_WATER_HALF;\n"
    "    else cell.aux &= ~AUX_WATER_HALF;\n"
    "}\n",
)

replace_once(
    "shaders/move.comp",
    "const uint AUX_MOVED = 0x01000000u;\n"
    "const uint AUX_STATE_MASK = 0x000000ffu;\n",
    "const uint AUX_MOVED = 0x01000000u;\n"
    "const uint AUX_WATER_HALF = 0x00800000u;\n"
    "const uint AUX_STATE_MASK = 0x000000ffu;\n",
)
replace_once(
    "shaders/move.comp",
    "void setStateValue(inout Cell cell, uint value) {\n"
    "    cell.aux = (cell.aux & ~AUX_STATE_MASK) | min(value, 255u);\n"
    "}\n",
    "void setStateValue(inout Cell cell, uint value) {\n"
    "    cell.aux = (cell.aux & ~AUX_STATE_MASK) | min(value, 255u);\n"
    "}\n\n"
    "bool isHalfWaterCell(Cell cell) {\n"
    "    return cell.material == MAT_WATER && (cell.aux & AUX_WATER_HALF) != 0u;\n"
    "}\n\n"
    "uint waterHalfUnits(Cell cell) {\n"
    "    return cell.material == MAT_WATER ? (isHalfWaterCell(cell) ? 1u : 2u) : 0u;\n"
    "}\n\n"
    "void setHalfWaterCell(inout Cell cell, bool halfState) {\n"
    "    if (cell.material != MAT_WATER) return;\n"
    "    if (halfState) cell.aux |= AUX_WATER_HALF;\n"
    "    else cell.aux &= ~AUX_WATER_HALF;\n"
    "}\n\n"
    "bool isOpenGas(Cell cell) {\n"
    "    return cell.material == MAT_EMPTY || cell.material == MAT_OXYGEN;\n"
    "}\n\n"
    "Cell oxygenFrom(Cell source) {\n"
    "    source.material = MAT_OXYGEN;\n"
    "    source.age = 0u;\n"
    "    source.temperature = 20;\n"
    "    source.aux &= ~(AUX_WATER_HALF | AUX_MOVED | AUX_STATE_MASK);\n"
    "    source.aux |= 220u;\n"
    "    return source;\n"
    "}\n\n"
    "void mergeHalfWaterPair(ivec2 keepPosition, ivec2 releasePosition, Cell keep, Cell released) {\n"
    "    setHalfWaterCell(keep, false);\n"
    "    keep.age = 0u;\n"
    "    keep.aux |= AUX_MOVED;\n"
    "    cells[moveIndex(keepPosition)] = keep;\n"
    "    cells[moveIndex(releasePosition)] = oxygenFrom(released);\n"
    "    markChunkDirty(keepPosition);\n"
    "    markChunkDirty(releasePosition);\n"
    "}\n\n"
    "void splitFullWaterPair(ivec2 sourcePosition, ivec2 targetPosition, Cell source, Cell target) {\n"
    "    setHalfWaterCell(source, true);\n"
    "    source.age = 0u;\n"
    "    source.aux |= AUX_MOVED;\n"
    "    Cell halfCell = source;\n"
    "    halfCell.temperature = (source.temperature + target.temperature) / 2;\n"
    "    cells[moveIndex(sourcePosition)] = source;\n"
    "    cells[moveIndex(targetPosition)] = halfCell;\n"
    "    markChunkDirty(sourcePosition);\n"
    "    markChunkDirty(targetPosition);\n"
    "}\n",
)
replace_once(
    "shaders/move.comp",
    "    if (target.material == MAT_EMPTY) return true;\n",
    "    if (isOpenGas(target)) return true;\n",
)
replace_once(
    "shaders/move.comp",
    "    if (moveAt(targetPosition).material != MAT_EMPTY || !movementAllows(source, randomValue)) return false;\n",
    "    if (!isOpenGas(moveAt(targetPosition)) || !movementAllows(source, randomValue)) return false;\n",
)
replace_once(
    "shaders/move.comp",
    "    bool firstDrop = moveInside(next) && moveAt(next).material == MAT_EMPTY && canCellFallInto(source, moveAt(next + ivec2(0, 1)));\n"
    "    bool secondDrop = moveInside(next2) && moveAt(next).material == MAT_EMPTY && moveAt(next2).material == MAT_EMPTY && canCellFallInto(source, moveAt(next2 + ivec2(0, 1)));\n",
    "    bool firstDrop = moveInside(next) && isOpenGas(moveAt(next)) &&\n"
    "                     canCellFallInto(source, moveAt(next + ivec2(0, 1)));\n"
    "    bool secondDrop = moveInside(next2) && isOpenGas(moveAt(next)) &&\n"
    "                      isOpenGas(moveAt(next2)) &&\n"
    "                      canCellFallInto(source, moveAt(next2 + ivec2(0, 1)));\n",
)
replace_once(
    "shaders/move.comp",
    "    uint randomValue = moveHash(top, uint(movePc.phase) + 1u);\n\n"
    "    if (isInsect(a.material)",
    "    uint randomValue = moveHash(top, uint(movePc.phase) + 1u);\n\n"
    "    if (isHalfWaterCell(a) && isHalfWaterCell(b)) {\n"
    "        mergeHalfWaterPair(bottom, top, b, a);\n"
    "        return;\n"
    "    }\n\n"
    "    if (isInsect(a.material)",
)
replace_once(
    "shaders/move.comp",
    "    bool cohesiveMud = source.material == MAT_MUD &&\n"
    "        (below.material == MAT_DIRT || below.material == MAT_STONE || below.material == MAT_MUD);\n"
    "    if (!upward && !cohesiveMud &&\n",
    "    bool cohesiveMud = source.material == MAT_MUD &&\n"
    "        (below.material == MAT_DIRT || below.material == MAT_STONE || below.material == MAT_MUD);\n"
    "    if (!upward && source.material == MAT_WATER && supportedAt(sourcePosition, source) &&\n"
    "        isOpenGas(target)) {\n"
    "        int direction = targetPosition.x > sourcePosition.x ? 1 : -1;\n"
    "        Cell trailing = sampleAt(sourcePosition - ivec2(direction, 0));\n"
    "        if (waterHalfUnits(source) != 2u || waterHalfUnits(trailing) < 1u) return;\n"
    "    }\n"
    "    if (!upward && !cohesiveMud &&\n",
)
replace_once(
    "shaders/move.comp",
    "    if (isCellLiquid(a) && b.material == MAT_EMPTY && liquidCanSpread(left, right, a, randomValue)) {\n"
    "        swapCells(left, right);\n"
    "    } else if (isCellLiquid(b) && a.material == MAT_EMPTY &&\n"
    "               liquidCanSpread(right, left, b, randomValue >> 1u)) {\n"
    "        swapCells(left, right);\n",
    "    if (isHalfWaterCell(a) && isHalfWaterCell(b)) {\n"
    "        bool keepLeft = (randomValue & 1u) == 0u;\n"
    "        mergeHalfWaterPair(keepLeft ? left : right, keepLeft ? right : left,\n"
    "                           keepLeft ? a : b, keepLeft ? b : a);\n"
    "        return;\n"
    "    }\n\n"
    "    if (movePc.reserved1 != 0u && !isHalfWaterCell(a) && !isHalfWaterCell(b)) return;\n\n"
    "    if (isCellLiquid(a) && isOpenGas(b) && liquidCanSpread(left, right, a, randomValue)) {\n"
    "        if (a.material == MAT_WATER && !isHalfWaterCell(a)) splitFullWaterPair(left, right, a, b);\n"
    "        else swapCells(left, right);\n"
    "    } else if (isCellLiquid(b) && isOpenGas(a) &&\n"
    "               liquidCanSpread(right, left, b, randomValue >> 1u)) {\n"
    "        if (b.material == MAT_WATER && !isHalfWaterCell(b)) splitFullWaterPair(right, left, b, a);\n"
    "        else swapCells(left, right);\n",
)

replace_once(
    "shaders/chemistry.comp",
    "void commitResult(uint index, Cell before, Cell after) {\n"
    "    recordConservation(before, after);\n",
    "void commitResult(uint index, Cell before, Cell after) {\n"
    "    if (after.material != MAT_WATER || before.material != MAT_WATER)\n"
    "        after.aux &= ~AUX_WATER_HALF;\n"
    "    recordConservation(before, after);\n",
)
replace_once(
    "shaders/macro_move.comp",
    "            if (cell.material != representative.material || !bulkMovable(cell) ||\n"
    "                (cell.aux & AUX_MOVED) != 0u) return false;\n",
    "            if (cell.material != representative.material || !bulkMovable(cell) ||\n"
    "                (cell.aux & AUX_MOVED) != 0u) return false;\n"
    "            if (cell.material == MAT_WATER && isHalfWater(cell)) return false;\n",
)
replace_once(
    "shaders/fullscreen.frag",
    "    vec4 base = materialColor(cell.material, cell.age, cell.aux, grid);\n",
    "    vec4 base = materialColor(cell.material, cell.age, cell.aux, grid);\n"
    "    if (isHalfWater(cell)) base.rgb = mix(backgroundColor(grid), base.rgb, 0.50);\n",
)
replace_once(
    "src/vulkan_renderer.cpp",
    "        const std::array<std::int32_t, 9> phases = (simulation_step & 1u) == 0u\n"
    "            ? std::array<std::int32_t, 9>{0, 1, 2, 3, 4, 5, 5, 5, 5}\n"
    "            : std::array<std::int32_t, 9>{0, 2, 1, 4, 3, 5, 5, 5, 5};\n",
    "        const std::array<std::int32_t, 13> phases = (simulation_step & 1u) == 0u\n"
    "            ? std::array<std::int32_t, 13>{0, 1, 2, 3, 4, 5, 5, 5, 5, 5, 5, 5, 5}\n"
    "            : std::array<std::int32_t, 13>{0, 2, 1, 4, 3, 5, 5, 5, 5, 5, 5, 5, 5};\n",
)
replace_once(
    "src/vulkan_renderer.cpp",
    "                .reserved0 = collect_debug_stats ? 1u : 0u,\n"
    "            };\n",
    "                .reserved0 = collect_debug_stats ? 1u : 0u,\n"
    "                .reserved1 = phase_index >= 9u ? 1u : 0u,\n"
    "            };\n",
)
replace_once(
    "include/epoch/sand/simulation_policy.hpp",
    "inline constexpr std::uint32_t water_pressure_depth = 8u;\n",
    "inline constexpr std::uint32_t water_pressure_depth = 8u;\n"
    "inline constexpr std::uint32_t water_half_units_per_full_cell = 2u;\n"
    "inline constexpr std::uint32_t water_ledge_release_units = 3u;\n"
    "inline constexpr std::uint32_t water_full_horizontal_passes = 4u;\n"
    "inline constexpr std::uint32_t water_half_horizontal_passes = 8u;\n",
)
replace_once(
    "include/epoch/sand/simulation_policy.hpp",
    "[[nodiscard]] constexpr bool bulk_region_eligible(\n",
    "[[nodiscard]] constexpr bool water_ledge_can_release(\n"
    "    const std::uint32_t edge_units,\n"
    "    const std::uint32_t trailing_units) noexcept {\n"
    "    return edge_units == water_half_units_per_full_cell &&\n"
    "           edge_units + trailing_units >= water_ledge_release_units;\n"
    "}\n\n"
    "[[nodiscard]] constexpr bool bulk_region_eligible(\n",
)
replace_once(
    "tests/behavior_contract.cpp",
    "static_assert(epoch::sand::policy::water_pressure_depth == 8u);\n",
    "static_assert(epoch::sand::policy::water_pressure_depth == 8u);\n"
    "static_assert(epoch::sand::policy::water_half_units_per_full_cell == 2u);\n"
    "static_assert(!epoch::sand::policy::water_ledge_can_release(2u, 0u));\n"
    "static_assert(epoch::sand::policy::water_ledge_can_release(2u, 1u));\n"
    "static_assert(epoch::sand::policy::water_ledge_can_release(2u, 2u));\n"
    "static_assert(epoch::sand::policy::water_half_horizontal_passes ==\n"
    "              epoch::sand::policy::water_full_horizontal_passes * 2u);\n",
)

Path("HALF_WATER.md").write_text(
    "# Half-volume fresh water\n\n"
    "Fresh water uses conserved half-units without changing the canonical 16-byte cell layout.\n\n"
    "- faint fresh-water cell: one half-unit\n"
    "- full-color fresh-water cell: two half-units\n"
    "- two adjacent halves merge into one full cell and oxygen\n"
    "- a full cell spreading laterally splits into two halves\n"
    "- ledge release requires one full edge cell plus at least one trailing half-unit\n"
    "- half-water receives eight horizontal passes while full water receives four\n"
    "- half-water never enters an 8x8 macro transfer; it stays in the fine simulation\n"
    "- chemistry clears the half flag across material conversions\n\n"
    "All other materials and FastFreddy behavior remain unchanged.\n",
    encoding="utf-8",
)

readme = read("README.md")
if "## Half-volume fresh water" not in readme:
    write(
        "README.md",
        readme
        + "\n\n## Half-volume fresh water\n\n"
        + "Fresh water supports conserved faint half-cells, a three-half-unit ledge release threshold, "
        + "and half-only extra horizontal passes. See `HALF_WATER.md`.\n",
    )

print("Applied conserved Fix35 half-volume fresh water.")
