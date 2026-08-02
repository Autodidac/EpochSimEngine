from pathlib import Path


def replace_once(path: str, old: str, new: str) -> None:
    file = Path(path)
    text = file.read_text(encoding="utf-8")
    if old in text:
        file.write_text(text.replace(old, new, 1), encoding="utf-8")
    elif new not in text:
        raise SystemExit(f"{path}: expected source block not found")


replace_once(
    "shaders/tiles.glsl",
    """const uint TILE_STABILITY_OCCUPANCY = 52u;
const uint TILE_MIN_COHESIVE_CELLS = 32u;
const uint TILE_COLLAPSE_OCCUPANCY = TILE_MIN_COHESIVE_CELLS;""",
    """const uint TILE_STABILITY_OCCUPANCY = 52u;
// 31 destroyed cells is 48.4375% of an 8x8 tile: the first representable
// whole-cell count at or above the requested 48% destruction threshold.
const uint TILE_DESTROYED_CELLS_TO_CRUMBLE = 31u;
const uint TILE_MIN_COHESIVE_CELLS =
    TILE_CELL_COUNT - TILE_DESTROYED_CELLS_TO_CRUMBLE + 1u;
const uint TILE_COLLAPSE_OCCUPANCY = TILE_MIN_COHESIVE_CELLS;""",
)
replace_once(
    "shaders/tiles.glsl",
    """const uint TILE_MEDIUM_ENCLOSED = 0x02000000u;
const uint TILE_MEDIUM_BREAKUP = 0x04000000u;""",
    """const uint TILE_MEDIUM_ENCLOSED = 0x02000000u;
const uint TILE_MEDIUM_BREAKUP = 0x04000000u;
// Cohesive solid metadata for sleeping/debug only. This is never permission
// to translate the represented 8x8 solid as one movement packet.
const uint TILE_BULK_READY = 0x08000000u;""",
)

replace_once(
    "shaders/tiles.comp",
    """    bool macroLiquid = fullLiquid && (moving || liquidEnclosed);
    bool macroGas = fullGas && (moving || gasEnclosed);
    bool macroPowder = fullRegion && isPowder(dominant);
    bool macroSolid = fullRegion && isBlockCapable(dominant) && !structuralTile;
    bool macroMovable = (macroLiquid || macroGas || macroPowder || macroSolid) &&
                        !activeAgent && !hot && !reacting;""",
    """    bool macroLiquid = fullLiquid && (moving || liquidEnclosed);
    bool macroGas = fullGas && (moving || gasEnclosed);
    bool macroPowder = fullRegion && isPowder(dominant);
    // Block-capable solids may use tile metadata for cohesion, support, sleep,
    // and debug state, but they never become whole-tile movement packets.
    bool bulkReadySolid = fullRegion && isBlockCapable(dominant) && !hot && !reacting;
    bool macroMovable = (macroLiquid || macroGas || macroPowder) &&
                        !activeAgent && !hot && !reacting;""",
)
replace_once(
    "shaders/tiles.comp",
    """    if (wetContent) flags |= TILE_WET_CONTENT;
    if (macroSolid) flags |= TILE_MACRO_SOLID;
    if (macroPowder) flags |= TILE_MACRO_POWDER;""",
    """    if (wetContent) flags |= TILE_WET_CONTENT;
    if (bulkReadySolid) flags |= TILE_MACRO_SOLID | TILE_BULK_READY;
    if (macroPowder) flags |= TILE_MACRO_POWDER;""",
)

replace_once(
    "shaders/macro_move.comp",
    """bool macroSourceAllows(TileState state, Cell representative, uint randomValue) {
    if (!tileHas(state, TILE_MACRO_MOVABLE) || tileHas(state, TILE_MACRO_MOVED)) return false;""",
    """bool macroSourceAllows(TileState state, Cell representative, uint randomValue) {
    // Solid bulk readiness is a storage/sleep classification, never movement authority.
    if (tileHas(state, TILE_MACRO_SOLID) || tileHas(state, TILE_BULK_READY)) return false;
    if (!tileHas(state, TILE_MACRO_MOVABLE) || tileHas(state, TILE_MACRO_MOVED)) return false;""",
)
replace_once(
    "shaders/macro_move.comp",
    """    if (isCellPowder(source) || isBlockCapable(source.material))
        return isCellLiquid(target) && materialDensity(source.material) > materialDensity(target.material);""",
    """    if (isCellPowder(source))
        return isCellLiquid(target) && materialDensity(source.material) > materialDensity(target.material);""",
)

replace_once(
    "shaders/move.comp",
    """bool canCellFallInto(Cell moving, Cell target) {
    if (isStructural(moving) || isCellImmovable(moving)) return false;
    if (isOpenGas(target)) return true;
    if (isCellImmovable(target)) return false;
    if (moving.material == MAT_BEE || moving.material == MAT_QUEEN_BEE) return false;
    if (isCellGas(target) && !isCellGas(moving)) return true;
    if (isCellPowder(moving) || isLooseSolid(moving)) {
        return isCellLiquid(target) && effectiveDensity(moving) > effectiveDensity(target);
    }""",
    """bool canCellFallInto(Cell moving, Cell target) {
    bool looseSolid = isLooseSolid(moving);
    if (isStructural(moving) || (!looseSolid && isCellImmovable(moving))) return false;
    if (isOpenGas(target)) return true;
    if (isCellImmovable(target)) return false;
    if (moving.material == MAT_BEE || moving.material == MAT_QUEEN_BEE) return false;
    if (isCellGas(target) && !isCellGas(moving)) return true;
    if (isCellPowder(moving) || looseSolid) {
        return isCellLiquid(target) && effectiveDensity(moving) > effectiveDensity(target);
    }""",
)

replace_once(
    "shaders/chemistry.comp",
    """    if (tileHas(tile, TILE_STABLE) && !isStructural(source) &&
        source.material == tile.material && source.material != MAT_EMPTY &&
        !isCellGas(source) && !isCellLiquid(source)) {
        // Stability qualification never creates, replaces, fills, or snaps cells.
        // It marks only the already represented pixels coherent and supported.
        // Material, temperature, age, random variation, and damage are preserved.
        result.aux |= AUX_STRUCTURAL | AUX_SUPPORTED;
        result.aux &= ~AUX_MOVED;
    }
""",
    """    // TILE_STABLE and TILE_BULK_READY are metadata only. They may suppress
    // unnecessary work, but they never reconstruct loose or damaged cells into
    // structural cells. Dislodged stone therefore stays dislodged.
""",
)

replace_once(
    "shaders/fullscreen.frag",
    """        } else if (tileHas(tile, TILE_MACRO_MOVABLE)) {
            overlay = debugKeyColor(4u); alpha = 0.44;""",
    """        } else if (tileHas(tile, TILE_BULK_READY) || tileHas(tile, TILE_MACRO_MOVABLE)) {
            overlay = debugKeyColor(4u); alpha = 0.44;""",
)

replace_once(
    "missioncache.md",
    "| MC-012 | REGRESSION | 8x8 bulk-element movement | Full aligned liquids, gases, falling solids, mud, and wet materials use the same gravity, diagonal, lateral, density, erosion, and displacement rules as fine cells. Real scenes show non-zero bulk moves. A valid downward or erosive bulk move is never throttled by settling damping. |",
    "| MC-012 | REGRESSION | 8x8 bulk-element movement | Full aligned liquids, gases, powders, mud, and wet granular materials use the same gravity, diagonal, lateral, density, erosion, and displacement rules as fine cells. Block-capable solids such as stone never translate as whole tiles; tile metadata is limited to cohesion, support, sleep, and debug state. Real eligible scenes show non-zero bulk moves. |",
)
replace_once(
    "missioncache.md",
    "| MC-078 | PARTIAL | Authored structures remain intact | Authored beams, walls, machines, tanks, hives, and platforms retain structural support unless phase change, mining/damage, or the established fewer-than-32-of-64 collapse threshold releases them. Side-connected and suspended authored construction never collapses merely because four cells are not directly underneath an 8x8 tile. Runtime scene cycling must show no spontaneous disassembly. |",
    "| MC-078 | PARTIAL | Authored structures remain intact | Authored beams, walls, machines, tanks, hives, and platforms retain structural support unless phase change, mining/damage, or destruction reaches 31 of 64 cells (48.4375%, the first whole-cell count at or above 48%), leaving 33 or fewer represented cells. The remaining cells then crumble individually. Side-connected and suspended construction never collapses merely because four cells are not directly underneath an 8x8 tile. Runtime scene cycling must show no spontaneous disassembly. |",
)
replace_once(
    "missioncache.md",
    "| MC-086 | REGRESSION | Tile-mode terrain placement and soil stability | `TILES` mode paints aligned structural 8x8 packets for sand, soil (internal material ID 3), silt, salt, ice, and all block-capable materials. Soil tiles remain stable while supported and release only through damage, lost support, phase change, or the established collapse threshold. |",
    "| MC-086 | REGRESSION | Tile-mode terrain placement and soil stability | `TILES` mode paints aligned structural 8x8 packets for sand, soil (internal material ID 3), silt, salt, ice, and all block-capable materials. Soil tiles remain stable while supported and release only through damage, lost support, phase change, or the 48%-destroyed collapse threshold. Released block-capable solids move only as fine cells. |",
)
mission_path = Path("missioncache.md")
mission_text = mission_path.read_text(encoding="utf-8")
if "| MC-100 |" not in mission_text:
    anchor = "| MC-012 | REGRESSION | 8x8 bulk-element movement |"
    line_end = mission_text.index("\n", mission_text.index(anchor))
    mission_text = (
        mission_text[: line_end + 1]
        + "| MC-100 | PARTIAL | Solid tiles never move wholesale | Full stone and other block-capable solid regions may report `BULK READY` and sleep through tile metadata, but only individual cells may fall after damage or support loss. No macro dispatch may swap a block-capable 8x8 region. At 31 destroyed cells (48.4375%), the remaining 33 cells crumble to fine loose cells. Static contracts pass; packaged runtime acceptance remains required. |\n"
        + mission_text[line_end + 1 :]
    )
    mission_path.write_text(mission_text, encoding="utf-8")

release_path = Path("RELEASE_NOTES_v2.4.9.md")
release_text = release_path.read_text(encoding="utf-8")
solid_section = """## Solid tile correction

- Stone and every block-capable solid are excluded from whole-tile movement.
- A complete solid region may still report `BULK READY` for cohesion, support, sleeping, and debug visibility.
- Stable tile metadata no longer reattaches damaged loose cells as structural cells.
- Released stone and other block-capable solids fall only as individual fine cells.
- A tile crumbles when 31 of 64 cells are destroyed: 48.4375%, the first whole-cell count at or above 48%.

"""
if "## Solid tile correction" not in release_text:
    release_text = release_text.replace("## Regression coverage\n", solid_section + "## Regression coverage\n", 1)
    release_path.write_text(release_text, encoding="utf-8")

validator = Path("tools/validate_shader_contracts.py")
validator_text = validator.read_text(encoding="utf-8")
anchor = """    if \"WET\" not in labels:
        errors.append(\"derived wet material card label is missing\")
"""
contract = """    tile_defs = (SHADERS / \"tiles.glsl\").read_text(encoding=\"utf-8\")
    macro_move_comp = (SHADERS / \"macro_move.comp\").read_text(encoding=\"utf-8\")
    for token in (
        \"TILE_DESTROYED_CELLS_TO_CRUMBLE = 31u\",
        \"TILE_MIN_COHESIVE_CELLS =\\n    TILE_CELL_COUNT - TILE_DESTROYED_CELLS_TO_CRUMBLE + 1u\",
        \"TILE_BULK_READY = 0x08000000u\",
    ):
        if token not in tile_defs: errors.append(f\"48-percent solid-collapse contract missing {token!r}\")
    for token in (
        \"bool bulkReadySolid = fullRegion && isBlockCapable(dominant)\",
        \"bool macroMovable = (macroLiquid || macroGas || macroPowder)\",
        \"if (bulkReadySolid) flags |= TILE_MACRO_SOLID | TILE_BULK_READY;\",
    ):
        if token not in tiles_comp: errors.append(f\"solid tile classification contract missing {token!r}\")
    for token in (
        \"tileHas(state, TILE_MACRO_SOLID) || tileHas(state, TILE_BULK_READY)\",
        \"if (isCellPowder(source))\",
    ):
        if token not in macro_move_comp: errors.append(f\"solid macro-movement rejection contract missing {token!r}\")
    if \"isCellPowder(source) || isBlockCapable(source.material)\" in macro_move_comp:
        errors.append(\"block-capable solids can still displace as whole macro tiles\")
    for token in (\"bool looseSolid = isLooseSolid(moving);\",
                  \"(!looseSolid && isCellImmovable(moving))\"):
        if token not in move_comp: errors.append(f\"fine-cell solid crumble contract missing {token!r}\")
    for forbidden in (\"tileHas(tile, TILE_STABLE) && !isStructural(source)\",
                      \"result.aux |= AUX_STRUCTURAL | AUX_SUPPORTED\"):
        if forbidden in chemistry_comp:
            errors.append(f\"stable tile metadata still reconstructs damaged cells: {forbidden!r}\")
    if \"TILE_BULK_READY) || tileHas(tile, TILE_MACRO_MOVABLE)\" not in fullscreen:
        errors.append(\"bulk-ready debug state is not decoupled from macro movement\")
"""
if contract not in validator_text:
    if anchor not in validator_text:
        raise SystemExit("tools/validate_shader_contracts.py: insertion anchor not found")
    validator_text = validator_text.replace(anchor, anchor + contract, 1)
    validator.write_text(validator_text, encoding="utf-8")

Path("tools/apply_v249_solid_policy.py").unlink(missing_ok=True)
