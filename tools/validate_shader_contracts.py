#!/usr/bin/env python3
"""Run the legacy shader contract suite while enforcing v2.5.10 replacements.

The legacy validator still names eight v2.5.8/v2.5.9 implementation strings that
v2.5.10 deliberately replaced. This wrapper permits exactly those stale failures,
rejects every other legacy failure, and then validates the replacement contracts.
"""

from __future__ import annotations

import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SHADERS = ROOT / "shaders"

EXPECTED_STALE_ERRORS = {
    "authored bee-home origin contract missing 'return beeUsesAuthoredHome(aux) ? home + beeAuthoredWorldOrigin'",
    "medium presentation contract missing 'cell.material == MAT_ATMOSPHERE || cell.material == MAT_OXYGEN'",
    "medium presentation contract missing 'stateEdge ? 0.16 : 0.0'",
    "medium presentation contract missing 'mediumCell ? 0.0 : 0.045'",
    "camera/pause input contract missing 'input.fill_modifier && primary_pressed'",
    "medium-preserving debug/interface contract missing 'mediumCell'",
    "medium-preserving debug/interface contract missing 'stateEdge'",
    "v2.5.8 movement equilibrium contract missing 'source.material != MAT_HONEY && source.material != MAT_OIL'",
    "debug legend contract missing 'Legend order matches the state precedence'",
    "debug legend contract missing 'hierarchy state is an edge key'",
    "debug legend contract missing 'vec3(0.05, 0.78, 1.00)'",
    "debug legend contract missing 'vec3(0.025, 0.075, 0.22)'",
    "Air fill/ignition action contract missing 'Ignited upper-left connected Air region'",
    "generated control label contract missing '\"IGNITE AIR\"'",
    "context-sensitive tool contract missing 'state.shotTimer = plasma ? 14u : 7u'",
    "transient medium-tile contract missing 'bool macroLiquid = fullLiquid && (moving || liquidEnclosed || macroCadenceCarry)'",
    "transient medium-tile contract missing 'bool macroGas = fullGas && (moving || gasEnclosed || macroCadenceCarry)'",
    "shader terrain-generation contract missing 'terrainTrapResourceCell'",
    "library terrain-generation contract missing 'trap_resource_cell'",
    "fine Water/Atmosphere equilibrium contract missing 'bool macroLiquid = fullLiquid && (moving || liquidEnclosed || macroCadenceCarry)'",
    "fine Water/Atmosphere equilibrium contract missing 'bool macroGas = fullGas && (moving || gasEnclosed || macroCadenceCarry)'",
    "movement shader reintroduced driver-expensive loops",
    "durable structural retention contract missing 'bool collapsing = structuralTile && !durableStructuralTile'",
    "tile grid is not isolated behind debug/map visualization",
    "camera/placement input contract missing 'authored_scene_origin_x(config.grid_width)'",
    "camera/placement input contract missing 'authored_scene_origin_y(config.grid_height)'",
    "resident ground deposit contract missing 'terrainTrapResourceCell'",
    "CPU resident ground deposit contract missing 'trap_resource_cell'",
    "resource-first debug contract missing 'vec3(1.00, 0.08, 0.72)'",
    "resource-first debug contract missing 'vec3(0.025, 0.075, 0.22)'",
    "resource-first debug contract missing 'debugStats[STAT_STRUCTURAL_COLLAPSES]'",
    "resource-first debug contract missing 'debugStats[STAT_CONVEYOR_MOVES]'",
    "resource-first debug contract missing 'debugStats[STAT_MACHINE_INPUTS]'",
    "resource-first debug contract missing 'debugStats[STAT_MACHINE_OUTPUTS]'",
    "resource-first debug contract missing 'debugStats[STAT_VOLCANO_LAVA_OUTPUTS]'",
    "resource-first debug contract missing 'debugStats[STAT_VOLCANO_GAS_OUTPUTS]'",
    "v2.4.8 debug readability contract missing 'vec3(0.62, 0.18, 1.00)'",
    "v2.4.8 debug readability contract missing 'vec3(0.08, 0.94, 0.30)'",
    "v2.4.8 debug readability contract missing 'separatorYs'",
    "v2.4.8 debug readability contract missing 'keyColorMap'",
    "click-confirmed Fill input contract missing 'const bool fill_click = editor_workspace && input.fill_modifier && primary_pressed'",
    "click-confirmed Fill input contract missing 'if (fill_click) shared_state.fill_region.store(true'",
    "renderer UI text descriptor contract missing '.descriptorCount = 20'",
    "legacy project branding remains in CMakeLists.txt: 'EpochSimEngine'",
    "legacy project branding remains in README.md: 'EpochSimEngine'",
}

def require(text: str, token: str, errors: list[str], contract: str) -> None:
    if token not in text:
        errors.append(f"{contract} missing {token!r}")


def main() -> int:
    legacy = subprocess.run(
        [sys.executable, str(ROOT / "tools/validate_shader_contracts_legacy.py")],
        cwd=ROOT,
        capture_output=True,
        text=True,
        check=False,
    )
    legacy_errors = {
        line[4:]
        for line in legacy.stderr.splitlines()
        if line.startswith("  - ")
    }
    if legacy.returncode == 0:
        legacy_errors = set()
    elif legacy_errors != EXPECTED_STALE_ERRORS:
        unexpected = sorted(legacy_errors - EXPECTED_STALE_ERRORS)
        missing = sorted(EXPECTED_STALE_ERRORS - legacy_errors)
        print(legacy.stdout, end="", file=sys.stderr)
        print(legacy.stderr, end="", file=sys.stderr)
        if unexpected:
            print("Unexpected legacy shader-contract failures:", file=sys.stderr)
            for error in unexpected:
                print(f"  - {error}", file=sys.stderr)
        if missing:
            print("Expected stale failures changed; update the migration explicitly:", file=sys.stderr)
            for error in missing:
                print(f"  - {error}", file=sys.stderr)
        return 1

    move = (SHADERS / "move.comp").read_text(encoding="utf-8")
    bee_move = (SHADERS / "bee_move.comp").read_text(encoding="utf-8")
    paint = (SHADERS / "paint.comp").read_text(encoding="utf-8")
    actor = (SHADERS / "actor.comp").read_text(encoding="utf-8")
    actor_defs = (SHADERS / "actor.glsl").read_text(encoding="utf-8")
    tiles = (SHADERS / "tiles.comp").read_text(encoding="utf-8")
    tile_defs = (SHADERS / "tiles.glsl").read_text(encoding="utf-8")
    terrain_glsl = (SHADERS / "terrain_generation.glsl").read_text(encoding="utf-8")
    terrain_hpp = (ROOT / "include/sandhybrid/terrain_generation.hpp").read_text(encoding="utf-8")
    fullscreen = (SHADERS / "fullscreen.frag").read_text(encoding="utf-8")
    rainfall = (SHADERS / "rainfall.comp").read_text(encoding="utf-8")
    rain_membership = (SHADERS / "rain_membership.glsl").read_text(encoding="utf-8")
    renderer = (ROOT / "src/vulkan_renderer.cpp").read_text(encoding="utf-8")
    app = (ROOT / "src/app.cpp").read_text(encoding="utf-8")
    window_header = (ROOT / "include/sandhybrid/window.hpp").read_text(encoding="utf-8")
    window_win32 = (ROOT / "src/window_win32.cpp").read_text(encoding="utf-8")
    window_xcb = (ROOT / "src/window_xcb.cpp").read_text(encoding="utf-8")
    input_routing = (ROOT / "include/sandhybrid/input_routing.hpp").read_text(encoding="utf-8")
    scene = (ROOT / "include/sandhybrid/scene.hpp").read_text(encoding="utf-8")
    world_layout = (ROOT / "include/sandhybrid/world_layout.hpp").read_text(encoding="utf-8")
    scene_spawn = (ROOT / "include/sandhybrid/scene_spawn.hpp").read_text(encoding="utf-8")
    simulation_policy = (ROOT / "include/sandhybrid/simulation_policy.hpp").read_text(encoding="utf-8")
    section_scheduler = (ROOT / "include/sandhybrid/section_scheduler.hpp").read_text(encoding="utf-8")
    ui_layout = (ROOT / "include/sandhybrid/ui_layout.hpp").read_text(encoding="utf-8")
    macro_move = (SHADERS / "macro_move.comp").read_text(encoding="utf-8")
    structural_repair = (SHADERS / "structural_repair.comp").read_text(encoding="utf-8")
    reset = (SHADERS / "reset.comp").read_text(encoding="utf-8")
    chemistry = (SHADERS / "chemistry.comp").read_text(encoding="utf-8")
    conservation_corrections = (SHADERS / "conservation_corrections.comp").read_text(
        encoding="utf-8")
    copy_cells = (SHADERS / "copy_cells.comp").read_text(encoding="utf-8")
    materials = (SHADERS / "materials.glsl").read_text(encoding="utf-8")
    sunlight = (SHADERS / "sunlight.comp").read_text(encoding="utf-8")
    swarm = (SHADERS / "bee_swarm.glsl").read_text(encoding="utf-8")
    generator = (ROOT / "tools/generate_ui_text.py").read_text(encoding="utf-8")
    main_cpp = (ROOT / "src/main.cpp").read_text(encoding="utf-8")
    validation_doc = (ROOT / "VALIDATION.md").read_text(encoding="utf-8")
    runtime_doc = (ROOT / "docs/sandhybrid.md").read_text(encoding="utf-8")

    errors: list[str] = []
    for token in (
        "int liquidSpreadReach(uint material)",
        "int liquidDropDistance",
        "for (int offset = 1; offset <= 8; ++offset)",
        "Interior cells therefore settle",
        "releaseCollapsingStructural",
    ):
        require(move, token, errors, "v2.5.10 liquid/fracture movement contract")
    if "source.age < 18u" in move:
        errors.append("legacy 18-frame liquid surface cutoff remains")

    # Repair owns a separate shallow two-phase footprint, not a remote write
    # from ordinary fine pairs. These are source guards, not GPU acceptance.
    repair_code = re.sub(r"//[^\n]*|/\*.*?\*/", "", structural_repair, flags=re.S)
    for token in (
        'layout(local_size_x = 16, local_size_y = 8) in;',
        'layout(std430, binding = 4) readonly buffer Tiles',
        'TILE_MACRO_MOVABLE | TILE_MACRO_MOVED',
        'AUX_BEE_POLLEN | AUX_MOVED',
        '!isReconstructableMaterial(donor.material)',
        'phase == PHASE_SOLID || phase == PHASE_SOFTENED || phase == PHASE_POWDER',
        '!isStructural(cell) && (cell.material == MAT_EMPTY || isCellGas(cell))',
        'firstTileY += int((pc.reserved ^ uint(firstTileY)) & 1u);',
        'int tileY = firstTileY + 2 * int(gl_GlobalInvocationID.y);',
        'x >= activeEnd.x || originY >= activeEnd.y',
        '!tileHas(targetTile, TILE_STRUCTURAL)',
        '!tileHas(targetTile, TILE_DAMAGED)',
        'tileHas(targetTile, TILE_COLLAPSING)',
        'Cell column[9];',
        'int firstRow = max(-1, activeOrigin.y - originY);',
        'int lastRow = min(7, activeEnd.y - originY - 1);',
        'for (int row = 7; row >= 0; --row)',
        'for (int row = min(6, holeRow - 2); row >= -1; --row)',
        'contact.material != donor.material || !isStructural(contact)',
        'repairMacroOwned(tiles[tileIndex(sourcePosition, pc.width)])',
        'Cell displaced = column[holeRow + 1];',
        'donor.aux |= AUX_STRUCTURAL | AUX_SUPPORTED;',
        'cells[indexOf(holePosition)] = donor;',
        'cells[indexOf(sourcePosition)] = displaced;',
        'atomicOr(chunks[chunkIndex(holePosition, pc.width)].flags,',
        'atomicOr(chunks[chunkIndex(sourcePosition, pc.width)].flags,',
    ):
        require(repair_code, token, errors, "exclusive conservative structural repair")
    writes = re.findall(r"\bcells\s*\[[^\n;]+\]\s*=\s*([^;]+);", repair_code)
    if writes != ["donor", "displaced"]:
        errors.append("structural repair must write exactly donor and displaced owners")
    for token in (".age =", ".temperature =", "setStateValue(", "debugStats[",
                  "snapshotCells["):
        if token in repair_code:
            errors.append(f"structural repair changed preserved payload/ownership: {token!r}")
    if re.search(r"\btiles\s*\[[^\n;]+\]\s*=", repair_code):
        errors.append("structural repair must leave shared tile metadata readonly")
    for token in ("repairCandidateAt(", "repairDamagedTileFromLooseCell("):
        if token in move:
            errors.append(f"ordinary fine movement retains remote repair writer {token!r}")
    require((ROOT / "CMakeLists.txt").read_text(encoding="utf-8"),
            "        structural_repair.comp", errors,
            "structural repair build/deployment source")

    require(actor, "state.shotTimer = plasma ? 6u : 4u", errors,
            "compact tool-burst contract")
    require(fullscreen, "bool tinyDash", errors, "sparse tool-render contract")
    for token in (
        "const int ACTOR_HALF_WIDTH = 4;",
        "const int ACTOR_HEIGHT = 23;",
        "const int ACTOR_TOP_OFFSET = 1 - ACTOR_HEIGHT;",
        "const int ACTOR_HEAD_CENTER_OFFSET = -18;",
        "const int ACTOR_TOOL_ORIGIN_OFFSET = -13;",
    ):
        require(actor_defs, token, errors, "shared 23-cell player geometry contract")
    for token in (
        "for (int y = ACTOR_TOP_OFFSET; y <= 0; ++y)",
        "for (int x = -ACTOR_HALF_WIDTH; x <= ACTOR_HALF_WIDTH; ++x)",
        "state.y + ACTOR_HEAD_CENTER_OFFSET",
        "state.y + ACTOR_TOOL_ORIGIN_OFFSET",
        "state.y = clamp(center.y, -ACTOR_TOP_OFFSET",
    ):
        require(actor, token, errors, "scaled player simulation contract")
    for token in (
        "presentation share this 9x23 footprint",
        "playerDelta.y >= ACTOR_TOP_OFFSET",
        "actor.y + ACTOR_TOOL_ORIGIN_OFFSET",
    ):
        require(fullscreen, token, errors, "scaled player presentation contract")
    for token in (
        "player_half_width_cells = 4",
        "player_body_height_cells = 23",
        "player_head_center_offset_cells = -18",
        "player_tool_origin_offset_cells = -13",
    ):
        require(scene_spawn, token, errors, "CPU player geometry contract")

    for token in (
        "terrainTrapResource",
        "terrainNeighboringVeinMaterial",
        "uint coreDepth",
        "uint coreDeposit",
        "TERRAIN_FLAG_DELIBERATE_LOOSE",
    ):
        require(terrain_glsl, token, errors, "GLSL coherent terrain contract")
    for token in (
        "trap_resource",
        "neighboring_vein_material",
        "const auto core_depth",
        "const auto core_deposit",
        "deliberate_loose",
    ):
        require(terrain_hpp, token, errors, "C++ coherent terrain contract")

    for token in (
        "TILE_FRACTURE_ARMED",
        "TILE_DESTROYED_CELLS_TO_CRUMBLE = 31u",
    ):
        require(tile_defs, token, errors, "31-cell fracture definition contract")
    for token in (
        "bool thresholdCollapse = fractureArmed && structural < TILE_MIN_COHESIVE_CELLS;",
        "bool collapsing = thresholdCollapse || unsupportedStructural;",
        "if (fractureArmed) flags |= TILE_FRACTURE_ARMED;",
    ):
        require(tiles, token, errors, "fracture-armed tile contract")

    require(renderer, "std::array<std::int32_t, 7> phases", errors,
            "six-pass liquid equalization contract")
    for token in (
        "encodeHalfMediumTemperature",
        "halfMediumTemperature",
        "mergeHalfWaterTemperature",
        "mergeHalfMediumTemperature",
        "halfCell.temperature = source.temperature",
        "restoredMediumFrom(Cell source, uint material, uint volume, int temperature)",
        "uint sourceMediumMaterial = ambientAir ? MAT_EMPTY : mediumMaterial;",
        "firstAmbient && secondAmbient",
        "secondMaterial == MAT_ATMOSPHERE) ? MAT_ATMOSPHERE : MAT_EMPTY",
        "cell.material == MAT_WATER && !isHalfWaterCell(cell)",
        "if (isHalfWaterCell(cell)) return;",
    ):
        require(move, token, errors, "Half Water heat-ledger contract")
    for token in (
        "half_water_medium_temperature_min",
        "encode_half_water_medium_temperature",
        "decode_half_water_medium_temperature",
        "merge_half_water_temperature",
    ):
        require(simulation_policy, token, errors, "CPU Half Water heat-ledger contract")
    require(renderer, 'append("half_water_split_merge_heat_ledger"', errors,
            "production Half Water heat-ledger acceptance")
    for token in (
        "tile_boundary=1",
        "atmosphere_owners == 1u && empty_owners == 1u",
    ):
        require(renderer, token, errors,
                "production Half Water integer medium-owner acceptance")

    for token in (
        "const double debug_mean_overhead",
        "const double debug_p95_overhead",
        "debug_p95_delta <= ten_fps_tail_loss_ms",
        "interactive_debug_pair_measurement",
        "interactive_debug_frame_intervals",
        "interactive_samples[0].size() ==",
        "interactive_debug_frame_intervals[0].size() ==",
        "if (force_debug_sample && debug_region_visible)",
        "debug_present_p95_delta <= ten_fps_tail_loss_ms",
        "debug_interval_p95s[0] <= fifty_fps_interval_ms",
        '\"schema\\\": 3',
        '\"debug_mean_overhead_percent\\\"',
        '\"debug_p95_overhead_percent\\\"',
        '\"debug_p95_delta_ms\\\"',
        '\"debug_pairing\\\"',
        '\"debug_present_intervals\\\"',
    ):
        require(renderer, token, errors,
                "interactive Debug tail-latency acceptance")
    for token in (
        "options.runtime_acceptance_report.empty()",
        "options.long_cycle_acceptance_report.empty()",
        "options.interactive_acceptance_report.empty()",
        "bool visible = true",
        "visible ? WS_VISIBLE : 0u",
        "if (visible)",
    ):
        require(app + window_header + window_win32 + window_xcb, token, errors,
                "hidden automated acceptance window contract")

    for token in (
        "bool macroLiquid = fullLiquid && !liquidBoundaryFine;",
        "bool macroGas = fullGas && !gasBoundaryFine;",
        "mediumTravelSteps >= TILE_MEDIUM_PROGRESS_LIMIT",
        "mediumBlockedAttempts >= TILE_MEDIUM_PROGRESS_LIMIT",
        "packTileMediumProgress(mediumTravelSteps, mediumBlockedAttempts)",
    ):
        require(tiles, token, errors, "eight-step macro classifier contract")
    for token in (
        "const uint TILE_MEDIUM_PROGRESS_LIMIT = 8u;",
        "uint tileMediumTravelSteps(TileState state)",
        "uint tileMediumBlockedAttempts(TileState state)",
    ):
        require(tile_defs, token, errors, "packet progress packing contract")
    for token in (
        "TileState advanceMediumPacket(TileState state)",
        "packTileMediumProgress(tileMediumTravelSteps(state) + 1u, 0u)",
        "activeDispatchTileOrigin(",
    ):
        require(macro_move, token, errors, "successful exact-packet contract")
    for token in (
        "macro_packet_travel_steps = 8u",
        "macro_packet_blocked_attempts = 8u",
        "medium_packet_breaks_to_fine(",
    ):
        require(simulation_policy, token, errors, "CPU macro policy contract")
    for token in (
        "route_world_primary_action({",
        "WorldPrimaryAction::editor_fill",
        "WorldPrimaryAction::editor_paint",
        "persistent_world_spawn(config.grid_width, config.grid_height)",
    ):
        require(app, token, errors, "centralized running/paused editor contract")
    for token in (
        "input.fill_modifier || input.fill_armed",
        "WorldPrimaryAction::blueprint_place",
        "WorldPrimaryAction::player_mine",
    ):
        require(input_routing, token, errors, "single-owner input routing contract")

    require(scene, "inline constexpr std::uint32_t scene_count = 1u", errors,
            "single World runtime contract")
    require(world_layout, "persistent_world_district_count = 8u", errors,
            "eight-district layout contract")
    require(reset, "const ivec2 PERSISTENT_WORLD_DISTRICTS = ivec2(8, 1)", errors,
            "one-buffer district reset contract")
    require(world_layout, "persistent_world_district_columns = 8u", errors,
            "one-row district address contract")
    require(world_layout, "persistent_world_district_rows = 1u", errors,
            "one-row district address contract")
    require(chemistry, "result.aux &= ~AUX_MOVED;", errors,
            "one-tick movement ownership contract")
    require(renderer, "macro_liquid_consecutive_packets", errors,
            "consecutive macro packet acceptance contract")
    require(renderer, "macro_bubble_eight_step_breakup", errors,
            "eight-step Water bubble acceptance contract")
    require(renderer, "active_cell_dispatch(", errors,
            "active-window compute dispatch contract")
    require(reset, "int buriedBase = min(world.y - 4, surfaceRow + 13)", errors,
            "sunken Volcano geometry contract")
    require(swarm, "beeUsesPersistentWorldHome(uint width, uint height)", errors,
            "district-aware bee home contract")
    if "BEE_PERSISTENT_HOME_BIT" in swarm:
        errors.append("persistent bee metadata collides with reserved Half Water state")
    require(swarm, "district << 20u", errors,
            "district-aware bee home contract")
    require(ui_layout, "layout.previous_scene = {{0.0f, 0.0f}, {0.0f, 0.0f}}", errors,
            "no scene carousel contract")
    for token in (
        "bool debugStateMarkerPixel(uint state, ivec2 local)",
        "textColor = debugStatColor(stat)",
        "if (row == 17u || row == 24u) return debugKeyColor(1u)",
        "if (row == 18u || row == 25u) return debugKeyColor(8u)",
        "return false;                                                // stable/candidate",
        "Material color remains authoritative in Debug",
        "color.rgb = mix(color.rgb, debugKeyColor(state), alpha * occupancy)",
        "color = debugStateMarkerPixel(key, markerLocal)",
        "bool activeBoundary = activeArea && (activeLocal.x == 0 || activeLocal.y == 0)",
        "if (renderPc.debugMode == 1u && !mapSample)",
        "WORLD TOTALS and MAP remain clean presentations",
    ):
        require(fullscreen, token, errors,
                "material-preserving debug marker contract")
    if "bool stateEdge = local.x == 0 || local.y == 0" in fullscreen:
        errors.append("debug presentation reintroduced dense full-tile state frames")
    for token in (
        "if (debug_region_visible && run_simulation)",
        "if (collect_debug_stats) reset_debug_stats(frame.command_buffer)",
        "if (collect_debug_stats) {",
    ):
        require(renderer, token, errors,
                "zero-cost hidden debug collection contract")
    for token in (
        "record_nuke_from_space(frame.command_buffer)",
        "cannot combine new Fire with stale Atmosphere ownership",
        "nuke_warning_stage_count * nuke_presentations_per_stage",
        "Nuke from Space committed one GPU high-sky Atmosphere-to-Fire edit above Cloud",
        "nuke_from_space_gpu_high_sky_edit",
        "authored_structures_start_without_false_damage_or_bulk_state",
        "map_snapshot_slice = (map_snapshot_slice + 1u) % slice_count",
        "constexpr std::uint32_t slice_count = 64u",
        "constexpr std::uint32_t map_refresh_steps = 4u",
        "const bool present_frame = present_requested",
    ):
        require(renderer, token, errors,
                "staged Nuke and smooth-frame contract")
    for forbidden in (
        "void ignite_air_region()",
        "pending_scene_export",
        "simulation_overdue",
    ):
        if forbidden in renderer:
            errors.append(f"staged Nuke/performance contract retained {forbidden!r}")
    for token in (
        "if (pc.activeMode == 2u)",
        "previous.material == MAT_ATMOSPHERE",
        "continuous Cloud deck is the",
    ):
        require(paint, token, errors,
                "GPU Nuke edit contract")
    for token in (
        "uint nukeFlashFrames()",
        "Six precomputed light states are each held for eight presentations",
        "const vec3 warningColors[6]",
        "const float warningBase[6]",
        "grid.y < NUKE_HIGH_SKY_BOTTOM_Y",
    ):
        require(fullscreen, token, errors,
                "Nuke warning light contract")
    require(generator, '"NUKE FROM SPACE"', errors,
            "Nuke action label contract")
    require(fullscreen, "if (!mapSample && (x < renderPc.viewportLeft", errors,
            "MAP overlay must be classified before camera letterbox rejection")
    for token in (
        "0x0035ffffu",
        "FIX29_REFERENCE_TONE_BIT0_ROWS",
        "FIX29_REFERENCE_TONE_BIT1_ROWS",
        "FIX29_REFERENCE_TONE_BIT2_ROWS",
        "FIX29_REFERENCE_PALETTE[8]",
        "color.rgb = FIX29_REFERENCE_PALETTE[referenceHive - 1]",
        "Palette values are linearized",
    ):
        require(fullscreen, token, errors,
                "photographed Fix29 hive silhouette/palette contract")
    for token in (
        "bool tryStoreResource(inout ActorState state, uint material)",
        "bool releaseDamagedFragment(ActorState state",
        "bool toolTransparent(Cell cell)",
        "(cell.aux & AUX_MOVED) != 0u",
        "sourcePosition + impactNormal",
        "if (!stableLaserMedium(target.material)) continue",
        "exact pre-hit structural cell",
        "releaseDamagedFragment(state, p, fragment)",
    ):
        require(actor, token, errors, "conserved player-laser transfer contract")
    for token in (
        "bool persistentHighSkyCloudCell(ivec2 worldPosition)",
        "uint contour = hash32(uint(tile.x) ^ pc.seed ^ 0xc10d5u)",
        "return tile.y >= top && tile.y <= bottom",
    ):
        require(reset, token, errors, "world-wide aligned high-sky Cloud contract")
    for token in (
        "bool ventOutletOwnsLava(Cell cell)",
        "One exact Lava unit becomes one ejecta unit",
        "uint emission = outletOwnsLava ? ventEmissionKind(p, pressure) : 0u",
    ):
        require(chemistry, token, errors, "conserved Volcano outlet contract")
    for token in (
        "acidInventedWater",
        "acidInventedSolution",
        "source.material == MAT_WASTE && proposed.material == MAT_DIRTY_WATER",
        "corrected = makeCell(MAT_FERTILIZER)",
        "undoConvertedStat()",
    ):
        require(conservation_corrections, token, errors,
                "Acid/Waste no-invented-Water correction contract")
    for token in (
        "source.material == MAT_BEEHIVE && isStructural(source)",
        '#include "bee_swarm.glsl"',
        "source.material == MAT_EMPTY && proposed.material == MAT_BEE",
        "dedicated Bee movement phase owns replacement births",
        "const uint weatherCycleTicks = 7200u",
        "const uint rainStartTick = 4800u",
        "const uint rainDurationTicks = 600u",
        "const uint emissionCadence = 360u",
        "const uint sectorWidth = 256u",
        "bool rainScheduled =",
        "rainTryReserveEmission(uint(position.x), pc.width)",
        "rainReconcile(position, corrected, pc.width)",
        "corrected.aux |= AUX_RAIN_DROP",
        "sourceAt(position + ivec2(0, 1)).material != MAT_CLOUD",
        "source.material == MAT_STEAM || source.material == MAT_DIRTY_STEAM",
        "corrected.temperature = source.temperature",
    ):
        require(conservation_corrections, token, errors,
                "software-Vulkan-safe hive/lifecycle/weather correction contract")
    if "source.material != MAT_BEE && proposed.material == MAT_BEE" in conservation_corrections:
        errors.append(
            "software-Vulkan-safe hive/lifecycle/weather correction contract "
            "must preserve legitimate Queen-to-Bee migration")
    for token in (
        "const uint BEE_PASS_BIRTH = 1u",
        "layout(std430, binding = 1) buffer NextCells",
        "bool beeMoveAcceptedBirth",
        "void runBeeBirth(ivec2 position, Cell source)",
        "(pc.step & 4095u) != 0u",
        "beeMoveNearNestTile(position)",
        "uint stationaryBees = beeMoveStationaryPopulation",
        "uint districtBees = beeMoveDistrictPopulation",
        "districtBees >= BEE_COLONY_MAX",
        "stationaryBees != districtBees",
        "beeMoveSourceAt(missingPosition).material != MAT_ASH",
        "newborn.aux = beePackMetadata",
        "nextCells[indexOf(position)] = newborn",
        "nextCells[indexOf(position)] = cells[birthOwner]",
        "CurrentCells is immutable throughout this rare bounded phase",
        "runBeeBirth(position, bee)",
    ):
        require(bee_move, token, errors,
                "conserved two-cell Bee replacement transaction")
    for token in (
        '#include "rain_membership.glsl"',
        "if (pc.activeMode == 2u)",
        "if (((pc.step + x) & 3u) != 0u) return",
        "source.aux &= ~AUX_RAIN_DROP",
        "cells[targetIndex] = source",
    ):
        require(rainfall, token, errors,
                "scheduled off-window rain ownership contract")
    for token in (
        "layout(std430, binding = 10) buffer RainColumns",
        "atomicOr(rainColumns[wordIndex], bitMask)",
        "atomicAnd(rainColumns[wordIndex], ~bitMask)",
        "rainRegisterReserved",
        "rainTryReserveEmission",
    ):
        require(rain_membership, token, errors, "complete derived rain membership contract")
    for token in (
        "auto chained_cycle_cells = result",
        "auto hazard_cells = chained_cycle_cells",
        '"bee_repeated_lifecycle_schema2_round_trip"',
        "save_world(",
        "load_world(",
        "chained_cycle_cells = std::move(gpu_loaded)",
        "const bool district_bounded = false",
        "const auto translated_width = district_bounded",
        "? pre_expansion_world_width",
        "const auto translated_height = district_bounded",
        "? pre_expansion_world_height",
        "active_section_x, active_section_y, true, true",
    ):
        require(renderer, token, errors,
                "chained hive lifecycle and schema-2 ownership contract")
    if "auto hazard_cells = result" in renderer:
        errors.append(
            "independently reseeded hive replacement cycle: "
            "forbidden token 'auto hazard_cells = result'")
    for source, tokens, description in (
        (swarm, ("return beeFormationOffset(slot)",),
         "stable idle biohazard-slot contract"),
        (move, ("if (sourceDistance == 0) return false",
                "bool percolateWaterIntoEarth",
                "This is an exact swap, not hidden absorption"),
         "stable bee and conserved percolation contract"),
        (tiles, ("bool alwaysActiveThermal = false",
                 "cell.material == MAT_FIRE ||",
                 "cell.material == MAT_EMBER ||",
                 "cell.material == MAT_LAVA"),
         "always-active thermal-owner contract"),
        (conservation_corrections, ("bool shallowSkylightSurface",
                                    "corrected.material == MAT_GRASS && !shallowSurface",
                                    "uint encodedOpaqueRow = light[",
                                    "shallowSkylightSurface(position, encodedOpaqueRow - 1u)",
                                    "corrected.aux = source.aux & ~AUX_MOVED"),
         "shallow Grass correction contract"),
    ):
        for token in tokens:
            require(source, token, errors, description)
    for token in (
        "conservation_corrections_pipeline =",
        "create_compute_pipeline(\"conservation_corrections.comp.spv\")",
        "bind_compute(command_buffer, conservation_corrections_pipeline, current_set)",
        "auto bee_birth_push = simulation_push",
        "bee_birth_push.material = 1u",
        "record_bee_birth_pass(command_buffer, simulation_push, active_dispatch)",
        "record_bee_birth_pass(command_buffer, push, acceptance_dispatch)",
        "create_compute_pipeline(\"rainfall.comp.spv\")",
        "bind_compute(command_buffer, rainfall_pipeline, current_set)",
    ):
        require(renderer, token, errors,
                "conservative post-chemistry dispatch contract")
    if renderer.count(
            "bind_compute(command_buffer, conservation_corrections_pipeline, current_set)") != 3:
        errors.append(
            "conservative post-chemistry dispatch must cover production and both acceptance paths")
    cmake = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
    for token in (
        "add_custom_target(sandhybrid_runtime_shaders ALL",
        "rainfall.comp",
        "copy_if_different",
        "add_dependencies(SandHybrid_Demo sandhybrid_runtime_shaders)",
    ):
        require(cmake, token, errors,
                "incremental runtime shader deployment contract")
    for token in (
        "world_wide_high_sky_weather_inventory",
        "volcano_converts_owned_lava_without_overwriting_ambient",
        "acid_and_moist_waste_never_manufacture_water",
        "player_laser_world_transfer_conserves_without_collection",
        "player_laser_ignores_moving_cells",
        "player_laser_accepts_exposed_undersides",
        "player_laser_releases_exact_loose_fragment",
        "player_laser_targets_condensed_weather_and_life",
        "laser_fragment_survives_normal_pickup_without_vacuum",
        "blocked_player_laser_retains_exact_world_unit",
        "packed_atmosphere_respiration_conserves_pressure",
        "closed_crop_visible_co2_water_biomass",
        "closed_crop_stored_co2_debug_identity",
        "waterfall_dissolved_oxygen_closed_transaction",
        "waterfall_aeration_debug_identity",
        "water_weather_phase_temperature_ownership",
        "scheduled_rain_continues_off_window_without_pool_disturbance",
        "scheduled_rain_crosses_off_window_vacuum_without_pool_disturbance",
        "water_percolates_as_exact_owner_until_stone",
        "grass_is_three_cell_skylight_skin",
        "grass_rejects_roofed_cave_and_retains_moisture",
        "grass_skylight_ignores_world_shell_and_cloud_deck",
        "wet_skylight_soil_regrows_grass_without_losing_moisture",
        "wet_cave_soil_cannot_regrow_grass",
        "sunlight_distinguishes_world_containment_from_authored_roofs",
        "fire_ember_lava_are_always_active_thermal_owners",
        "fire_extinguish_does_not_duplicate_water",
        "renewable_lava_stone_family_balance",
    ):
        require(renderer, token, errors, "production weather/laser/ecology acceptance contract")
    for token in (
        "bool stableLaserMedium(uint material)",
        "return cell.material == MAT_EMPTY || stableLaserMedium(cell.material)",
        "(cell.aux & AUX_MOVED) != 0u",
        "(resourceCell.aux & AUX_BEE_POLLEN) != 0u",
        "void markLaserFragmentWorldOnly(inout Cell fragment)",
        "if (!stableLaserMedium(target.material)) continue",
    ):
        require(actor, token, errors,
                "post-movement laser world-only/no-Vacuum contract")
    if "material == MAT_EMPTY || isGas(material)" in actor:
        errors.append("laser ray transparency still skips every gas-like material")

    for token in (
        "expanded_cell_dispatch(",
        "const auto origin_x = dispatch.origin_x > halo ? dispatch.origin_x - halo : 0u;",
    ):
        require(section_scheduler, token, errors,
                "bounded active-window cell-buffer snapshot contract")
    for token in (
        "void copy_cell_rectangle(",
        "copy_cells_pipeline = create_compute_pipeline(\"copy_cells.comp.spv\")",
        "bind_compute(command_buffer, copy_cells_pipeline, source_set)",
        "copy_cell_rectangle(command_buffer, next_set, current_set, active_dispatch);",
        "constexpr std::uint32_t movement_snapshot_halo = 16u;",
        "copy_cell_rectangle(command_buffer, current_set, snapshot_set, snapshot_dispatch);",
        "divide_round_up(active_dispatch.width, sunlight_local_size)",
    ):
        require(renderer, token, errors,
                "bounded production GPU transfer/dispatch contract")
    for token in (
        "activeDispatchCellOrigin(",
        "gl_GlobalInvocationID.x + uint(dispatchOrigin.x)",
    ):
        require(sunlight, token, errors,
                "active-window sunlight dispatch contract")
    forbidden_full_tick_copy = (
        "const VkBufferCopy snapshot_copy{.srcOffset = 0, .dstOffset = 0, "
        ".size = cell_buffers[current_set].size};"
    )
    if forbidden_full_tick_copy in renderer:
        errors.append("fixed simulation tick reintroduced a full-world cell-buffer copy")
    if "std::vector<VkBufferCopy> rows" in renderer:
        errors.append("bounded cell copy reintroduced Mesa-hostile per-row transfer regions")
    for token in (
        "destinationCells[index] = sourceCells[index]",
    ):
        require(copy_cells, token, errors,
                "single-dispatch bounded cell-buffer copy contract")
    for token in (
        "bool respirePackedMedium(inout Cell cell)",
        "setPackedAtmosphereComponent(cell, MAT_CARBON_DIOXIDE, carbon + 1u)",
        "bool photosynthesizePackedMedium(inout Cell cell)",
    ):
        require(materials, token, errors, "packed Atmosphere ecology contract")
    for token in (
        "respirePackedMedium(after)",
        "recordConservation(before, after)",
    ):
        require(actor, token, errors, "conserved actor respiration contract")
    require(chemistry, "respirePackedMedium(result)", errors,
            "conserved life/fire respiration contract")
    for token in (
        "material == MAT_CLOUD",
        "material != MAT_EMPTY && !isGas(material)",
    ):
        require(sunlight, token, errors,
                "transparent Atmosphere and bounded weather-light contract")
    for token in (
        "bool fertilizerHarvestReady(ivec2 fertilizerPosition, Cell fertilizer)",
        "bool harvestConsumesCarbon(ivec2 carbonPosition)",
        "photosynthesizePackedMedium(result)",
    ):
        require(chemistry, token, errors,
                "conserved Water/CO2/Oxygen/biomass crop contract")
    for token in (
        "bool hasDissolvedWaterGas(Cell cell)",
        "bool setDissolvedWaterOxygen(inout Cell cell, int gasTemperature)",
        "void clearDissolvedWaterGas(inout Cell cell)",
    ):
        require(materials, token, errors,
                "one-unit dissolved Water/Oxygen ownership contract")
    for token in (
        "bool dissolvedOutgasPair(ivec2 position, Cell source)",
        "bool dissolvedOxygenPair(ivec2 position, Cell source)",
        "returns it through a disjoint compatible Atmosphere pair",
        "ivec2 aerationPairPartner(ivec2 position)",
    ):
        require(chemistry, token, errors,
                "conserved waterfall aeration chemistry contract")
    require(move, "!moveHasDissolvedWaterGas(a) && fullWaterSplitSupplied(left, 1)",
            errors, "dissolved Water cannot enter Half Water payload contract")
    for token in (
        "neighborCount(p, MAT_LAVA) >= 4u",
        "Fire is energy, not a second hidden Water source",
        "result.temperature = source.temperature",
        "int carriedTemperature = max(result.temperature, 100)",
    ):
        require(chemistry, token, errors,
                "renewable Water/weather and Lava/Stone phase ownership contract")

    for token in (
        "--long-cycle-acceptance-report",
        "options.long_cycle_acceptance_report",
    ):
        require(main_cpp, token, errors,
                "separate repeated finite-ledger CLI contract")
    for token in (
        "int run_long_cycle_acceptance()",
        "constexpr std::uint32_t requested_cycles = 12u;",
        "download_scene_cell_prefix(fixture_cell_count)",
        'report_path.stem().string() + "-save-fixture"',
        "completed_cycles == requested_cycles",
        '"  \\"requested_cycles\\": "',
        '"  \\"completed_cycles\\": "',
    ):
        require(renderer, token, errors,
                "repeated finite-ledger runtime contract")
    require(renderer, "for (std::uint32_t tick = 0u; tick < 384u; ++tick)",
            errors, "bounded three-cycle hive acceptance contract")
    if "tick < 1'800u" in renderer:
        errors.append("hive acceptance reintroduced the redundant 1,800-tick window")
    for document, description in (
        (validation_doc, "validation guide"),
        (runtime_doc, "runtime guide"),
    ):
        require(document, "--long-cycle-acceptance-report", errors,
                f"{description} repeated-cycle command contract")

    if errors:
        print("current shader/interface contract validation failed:", file=sys.stderr)
        for error in errors:
            print(f"  - {error}", file=sys.stderr)
        return 1

    if legacy_errors:
        print(
            "Legacy shader contracts passed except for the explicitly superseded "
            "implementation strings checked by the current replacement suite."
        )
    print("Current liquid, terrain, fracture, input, macro, weather, laser, debug, and one-World contracts valid.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
