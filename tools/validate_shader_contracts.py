#!/usr/bin/env python3
"""Run the legacy shader contract suite while enforcing v2.5.10 replacements.

The legacy validator still names eight v2.5.8/v2.5.9 implementation strings that
v2.5.10 deliberately replaced. This wrapper permits exactly those stale failures,
rejects every other legacy failure, and then validates the replacement contracts.
"""

from __future__ import annotations

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
    "v2.5.8 movement equilibrium contract missing 'source.material != MAT_HONEY && source.material != MAT_OIL'",
    "debug legend contract missing 'Legend order matches the state precedence'",
    "debug legend contract missing 'hierarchy state is an edge key'",
    "debug legend contract missing 'vec3(0.05, 0.78, 1.00)'",
    "debug legend contract missing 'vec3(0.025, 0.075, 0.22)'",
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
    actor = (SHADERS / "actor.comp").read_text(encoding="utf-8")
    tiles = (SHADERS / "tiles.comp").read_text(encoding="utf-8")
    tile_defs = (SHADERS / "tiles.glsl").read_text(encoding="utf-8")
    terrain_glsl = (SHADERS / "terrain_generation.glsl").read_text(encoding="utf-8")
    terrain_hpp = (ROOT / "include/sandhybrid/terrain_generation.hpp").read_text(encoding="utf-8")
    fullscreen = (SHADERS / "fullscreen.frag").read_text(encoding="utf-8")
    renderer = (ROOT / "src/vulkan_renderer.cpp").read_text(encoding="utf-8")
    app = (ROOT / "src/app.cpp").read_text(encoding="utf-8")
    input_routing = (ROOT / "include/sandhybrid/input_routing.hpp").read_text(encoding="utf-8")
    scene = (ROOT / "include/sandhybrid/scene.hpp").read_text(encoding="utf-8")
    world_layout = (ROOT / "include/sandhybrid/world_layout.hpp").read_text(encoding="utf-8")
    simulation_policy = (ROOT / "include/sandhybrid/simulation_policy.hpp").read_text(encoding="utf-8")
    ui_layout = (ROOT / "include/sandhybrid/ui_layout.hpp").read_text(encoding="utf-8")
    macro_move = (SHADERS / "macro_move.comp").read_text(encoding="utf-8")
    reset = (SHADERS / "reset.comp").read_text(encoding="utf-8")
    chemistry = (SHADERS / "chemistry.comp").read_text(encoding="utf-8")
    swarm = (SHADERS / "bee_swarm.glsl").read_text(encoding="utf-8")

    errors: list[str] = []
    for token in (
        "int liquidSpreadReach(uint material)",
        "int liquidDropDistance",
        "for (int offset = 1; offset <= 8; ++offset)",
        "Interior cells therefore settle",
        "releaseCollapsingStructural",
        "tileHas(targetTile, TILE_COLLAPSING)",
    ):
        require(move, token, errors, "v2.5.10 liquid/fracture movement contract")
    if "source.age < 18u" in move:
        errors.append("legacy 18-frame liquid surface cutoff remains")

    require(actor, "state.shotTimer = plasma ? 6u : 4u", errors,
            "compact tool-burst contract")
    require(fullscreen, "bool tinyDash", errors, "sparse tool-render contract")

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
    require(fullscreen, "if (readableTileGrid && stateEdge)", errors,
            "square edge-only debug contract")
    require(fullscreen, "if (!mapSample && (x < renderPc.viewportLeft", errors,
            "MAP overlay must be classified before camera letterbox rejection")
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
    print("Current liquid, terrain, fracture, input, macro, debug, and one-World contracts valid.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
