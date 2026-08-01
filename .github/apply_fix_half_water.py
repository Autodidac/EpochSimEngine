from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

def replace(path: str, old: str, new: str) -> None:
    file = ROOT / path
    text = file.read_text(encoding="utf-8")
    if new in text:
        return
    if old not in text:
        raise SystemExit(f"Expected source block missing in {path}: {old[:80]!r}")
    file.write_text(text.replace(old, new, 1), encoding="utf-8")

# Tile placement must cover all canonical terrain-forming materials.
replace(
    "shaders/paint.comp",
    "Cell cell = isBlockCapable(material) ? makeStructuralCell(material, anchored)\n                                             : makeCell(material);",
    "Cell cell = isReconstructableMaterial(material) ? makeStructuralCell(material, anchored)\n                                                       : makeCell(material);",
)

move = ROOT / "shaders/move.comp"
text = move.read_text(encoding="utf-8")
if "bool solidSupportsWaterAt" not in text:
    begin = text.index("bool halfWaterAhead(")
    end = text.index("\nbool isMovementHazard", begin)
    text = text[:begin] + """bool solidSupportsWaterAt(ivec2 position) {
    Cell below = sampleAt(position + ivec2(0, 1));
    return isStructural(below) || isCellImmovable(below) ||
           (isCellSolid(below) && !isCellLiquid(below) && !isCellGas(below));
}

bool solidLedgePrecedesDrop(ivec2 position, int direction) {
    if (!solidSupportsWaterAt(position)) return false;
    ivec2 beyond = position + ivec2(direction, 0);
    return moveInside(beyond) && isOpenGas(sampleAt(beyond)) &&
           isOpenGas(sampleAt(beyond + ivec2(0, 1)));
}
""" + text[end:]
text = text.replace(
    "    // Fractional surface cells converge in one direction so chains terminate instead of chasing forever.\n"
    "    if (isHalfWaterCell(a) && isOpenGas(b) && halfWaterAhead(left, 1)) { swapCells(left, right); return; }\n\n",
    "    // Half-water is a pre-fall solid-ledge state only. It never hops along\n"
    "    // exposed water edges and never crawls after entering open air.\n\n",
)
text = text.replace(
    "    if (isCellLiquid(a) && isOpenGas(b) && liquidCanSpread(left, right, a, randomValue)) {\n"
    "        swapCells(left, right);\n"
    "    } else if (isCellLiquid(b) && isOpenGas(a) &&\n"
    "               liquidCanSpread(right, left, b, randomValue >> 1u)) {\n"
    "        swapCells(left, right);\n",
    "    if (isCellLiquid(a) && isOpenGas(b) && liquidCanSpread(left, right, a, randomValue)) {\n"
    "        if (a.material == MAT_WATER && !isHalfWaterCell(a) &&\n"
    "            solidLedgePrecedesDrop(right, 1)) splitFullWaterPair(left, right, a, b);\n"
    "        else swapCells(left, right);\n"
    "    } else if (isCellLiquid(b) && isOpenGas(a) &&\n"
    "               liquidCanSpread(right, left, b, randomValue >> 1u)) {\n"
    "        if (b.material == MAT_WATER && !isHalfWaterCell(b) &&\n"
    "            solidLedgePrecedesDrop(left, -1)) splitFullWaterPair(right, left, b, a);\n"
    "        else swapCells(left, right);\n",
)
move.write_text(text, encoding="utf-8")

chemistry = ROOT / "shaders/chemistry.comp"
text = chemistry.read_text(encoding="utf-8")
text = text.replace(
    "    bool freshContact = hasNeighbor(p, MAT_WATER) || hasNeighbor(p, MAT_DIRTY_WATER);\n"
    "    uint supportBelow = at(p + ivec2(0, 1)).material;\n",
    "    bool freshContact = hasNeighbor(p, MAT_WATER) || hasNeighbor(p, MAT_DIRTY_WATER);\n"
    "    bool shallowSandContact = freshContact &&\n"
    "        (isGas(at(p + ivec2(0, -1)).material) ||\n"
    "         isGas(at(p + ivec2(0, -2)).material) ||\n"
    "         isGas(at(p + ivec2(-1, 0)).material) ||\n"
    "         isGas(at(p + ivec2(1, 0)).material));\n"
    "    uint supportBelow = at(p + ivec2(0, 1)).material;\n",
)
text = text.replace(
    "    if (source.material == MAT_SAND || source.material == MAT_SILT) {\n"
    "        if (freshContact) result.aux |= AUX_WET;\n"
    "        else if ((result.aux & AUX_WET) != 0u && source.age > 600u && (randomValue & 127u) == 0u)\n"
    "            result.aux &= ~AUX_WET;\n",
    "    if (source.material == MAT_SAND || source.material == MAT_SILT) {\n"
    "        // Wet granular feed retains its represented wash water. Only the\n"
    "        // shallow surface band can absorb new water; drying may not delete it.\n"
    "        if (shallowSandContact) result.aux |= AUX_WET;\n",
)
text = text.replace(
    "    if (machine == MAT_SLUICE_BOX)\n"
    "        return resourceMaterial == MAT_SAND || resourceMaterial == MAT_SILT ? 0 : -1;",
    "    if (machine == MAT_SLUICE_BOX)\n"
    "        return (resourceMaterial == MAT_SAND || resourceMaterial == MAT_SILT) &&\n"
    "               (resourceCell.aux & AUX_WET) != 0u ? 0 : -1;",
)
text = text.replace(
    "    if (machine == MAT_SLUICE_BOX) return inventory.z > 0u ? MAT_GOLD : MAT_SILT;",
    "    if (machine == MAT_SLUICE_BOX) return inventory.z > 0u ? MAT_GOLD : MAT_WATER;",
)
chemistry.write_text(text, encoding="utf-8")

replace(
    "shaders/fullscreen.frag",
    "            text = text || materialPixel(pixel, ivec2(int(contentLeft + 10u), int(cardTop + 9u)), 3, cardMaterial);",
    "            if (renderPc.inspectMode != 0u && isHalfWater(inspected))\n"
    "                text = text || fixedPixel(pixel, ivec2(int(contentLeft + 10u), int(cardTop + 9u)), 3, 157u);\n"
    "            else\n"
    "                text = text || materialPixel(pixel, ivec2(int(contentLeft + 10u), int(cardTop + 9u)), 3, cardMaterial);",
)
replace(
    "tools/generate_ui_text.py",
    '    "VOLCANO GAS", "GAS EDGE", "REACTIONS",\n]',
    '    "VOLCANO GAS", "GAS EDGE", "REACTIONS", "HALF WATER",\n]',
)

renderer = ROOT / "src/vulkan_renderer.cpp"
text = renderer.read_text(encoding="utf-8")
text = text.replace(
    "        const bool debug_visible = state.debug_visualization.load(std::memory_order_relaxed);\n"
    "        bool collect_debug_stats = false;\n"
    "        if (debug_visible) {\n"
    "            collect_debug_stats = !debug_was_visible || (debug_sample_frame % 16u) == 0u;\n"
    "            ++debug_sample_frame;\n"
    "        } else {\n"
    "            debug_sample_frame = 0u;\n"
    "        }\n"
    "        debug_was_visible = debug_visible;\n"
    "        if (collect_debug_stats) reset_debug_stats(frame.command_buffer);\n"
    "        const bool step_once = state.single_step.exchange(false, std::memory_order_acq_rel);\n"
    "        const bool run_simulation = !reset_this_frame && (step_once ||\n"
    "            (simulation_tick && !state.paused.load(std::memory_order_relaxed)));\n",
    "        const bool debug_visible = state.debug_visualization.load(std::memory_order_relaxed);\n"
    "        const bool step_once = state.single_step.exchange(false, std::memory_order_acq_rel);\n"
    "        const bool run_simulation = !reset_this_frame && (step_once ||\n"
    "            (simulation_tick && !state.paused.load(std::memory_order_relaxed)));\n"
    "        bool collect_debug_stats = false;\n"
    "        if (debug_visible && run_simulation) {\n"
    "            collect_debug_stats = !debug_was_visible || (debug_sample_frame % 16u) == 0u;\n"
    "            ++debug_sample_frame;\n"
    "        } else if (!debug_visible) {\n"
    "            debug_sample_frame = 0u;\n"
    "        }\n"
    "        debug_was_visible = debug_visible;\n"
    "        if (collect_debug_stats) reset_debug_stats(frame.command_buffer);\n",
)
text = text.replace(
    "            const auto movement_pair_tests = run_simulation\n"
    "                ? config.grid_width * config.grid_height * 13u / 2u\n"
    "                : 0u;",
    "            const auto movement_pair_tests = config.grid_width * config.grid_height * 13u / 2u;",
)
renderer.write_text(text, encoding="utf-8")

cache = ROOT / "missioncache.md"
text = cache.read_text(encoding="utf-8")
text = text.replace(
    "| MC-014 | PARTIAL | Fractional-water consolidation |",
    "| MC-084 | REGRESSION | Half-water solid-ledge behavior and card identity | Inspected fractional fresh water is titled `HALF WATER`. Fractional-water creation and movement occur only as the pre-fall state on a solid-supported ledge. It never hops along exposed water edges, propagates across a water surface, or continues as a post-fall crawling/dripping artifact. |\n"
    "| MC-014 | PARTIAL | Fractional-water consolidation |",
)
text = text.replace(
    "| MC-080 | PARTIAL | Resource-first activity debug |",
    "| MC-085 | REGRESSION | Pair-test and skipped-work debug samples | `PAIR TESTS` and `SKIPPED` preserve the last completed simulation sample. Render-only frames never clear them to zero. Pair tests reflect actual fine/macro pair dispatch scope and skipped work reflects real tile/chunk rejection. |\n"
    "| MC-080 | PARTIAL | Resource-first activity debug |",
)
text = text.replace(
    "| MC-051 | PARTIAL | Wet-material, mud-erosion, and sluicing proof |",
    "| MC-086 | REGRESSION | Tile-mode terrain placement and dirt stability | `TILES` mode paints aligned structural 8x8 packets for sand, dirt, silt, salt, ice, and all block-capable materials. Dirt tiles remain stable while supported and release only through damage, lost support, phase change, or the established collapse threshold. |\n"
    "| MC-087 | REGRESSION | Conserved shallow wet sand | Sand/silt absorb water only in a bounded shallow surface band. Once wet, the water remains represented in the wet granular cell and cannot disappear through a timer-based dry flag. |\n"
    "| MC-088 | REGRESSION | Wet-feed sluice output | Wet sand or wet silt dropped onto a water-supplied sluice is accepted deterministically and outputs retained wash water plus trace gold. Dry feed is rejected; input/output counters and conservation remain visible. |\n"
    "| MC-051 | PARTIAL | Wet-material, mud-erosion, and sluicing proof |",
)
cache.write_text(text, encoding="utf-8")

validator = ROOT / "tools/validate_shader_contracts.py"
text = validator.read_text(encoding="utf-8")
text = text.replace(
    'Cell cell = isBlockCapable(material) ? makeStructuralCell(material, anchored)',
    'Cell cell = isReconstructableMaterial(material) ? makeStructuralCell(material, anchored)',
)
validator.write_text(text, encoding="utf-8")
