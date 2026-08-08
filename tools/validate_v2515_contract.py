#!/usr/bin/env python3
"""Validate the retained v2.5.15 macro, hive, cursor, pause, and sidebar recovery."""

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def source(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def require(path: str, token: str) -> None:
    if token not in source(path):
        raise SystemExit(f"{path}: missing {token!r}")


def reject(path: str, token: str) -> None:
    if token in source(path):
        raise SystemExit(f"{path}: obsolete token remains {token!r}")


# Canonical history remains while the newest contract owns current publication.
require("CMakeLists.txt", "VERSION 2.5.20")
require("CHANGELOG.md", "## 2.5.15")

# v2.5.3 macro ownership: moving complete media get a packet attempt.
for token in (
    "bool macroLiquid = fullLiquid && (moving || liquidEnclosed || macroCadenceCarry)",
    "bool macroGas = fullGas && (moving || gasEnclosed || macroCadenceCarry)",
    "fineFallbackMedium",
):
    require("shaders/tiles.comp", token)
for token in ("gas_tile_eligible(true, true, false)",
              "liquid_tile_eligible(true, true, false)"):
    require("tests/behavior_contract.cpp", token)

# Exact Fix29 Ecosystem hive body, tiled perch, positions, and deterministic contents without foreign actor behavior.
for token in (
    "BEEHIVE_SHELL_MIN_RADIUS_SQUARED = 28",
    "BEEHIVE_SHELL_MAX_RADIUS_SQUARED = 108",
    "BEEHIVE_CHAMBER_RADIUS_SQUARED = 28",
    "BEEHIVE_EXIT_MAX_X = 12",
    "BEEHIVE_SUPPORT_WIDTH = 72",
    "BEEHIVE_SUPPORT_HEIGHT = 8",
    "BEEHIVE_SUPPORT_TILE_SIZE = 8",
    "BEEHIVE_SUPPORT_LEFT_BIAS = 40",
):
    require("shaders/beehive.glsl", token)
for token in (
    "beehive_shell_min_radius_squared = 28",
    "beehive_shell_max_radius_squared = 108",
    "beehive_chamber_radius_squared = 28",
    "beehive_exit_max_x = 12",
):
    require("src/scene_image.cpp", token)

# Exact production paths for the recovered packet, Half Water, hive, cursor, and Fill behavior.
for token in (
    "if (tileHas(targetState, TILE_MACRO_MOVED)) return false;",
    "tileOccupancy(targetState) != TILE_CELL_COUNT",
):
    require("shaders/macro_move.comp", token)
reject("shaders/macro_move.comp", "TILE_FINE_ACTIVE) || tileHas(targetState, TILE_MACRO_MOVED")
for token in (
    "bool finePair = tileNeedsFine(tileA) || tileNeedsFine(tileB);",
    "if (!finePair && (tileIsMacro(tileA) || tileIsMacro(tileB)))",
    "a.material == MAT_WATER && !isHalfWaterCell(a)",
):
    require("shaders/move.comp", token)
for token in ("halfWaterFallPending", "halfWaterMergePending",
              "!halfWaterFallPending && !halfWaterMergePending"):
    require("shaders/tiles.comp", token)
for token in (
    "AUTHORED_WORLD_CELLS.x * 4 / 5, AUTHORED_WORLD_CELLS.y - 126",
    "AUTHORED_WORLD_CELLS.x * 4 / 5, AUTHORED_WORLD_CELLS.y - 128",
    "beehiveSupportForScene",
):
    require("shaders/reset.comp", token)
for token in ("normalize_pre_pr19_hives", "pre_pr19_beehive_material",
              "beehive_support_width = 72"):
    require("src/scene_image.cpp", token)
for token in ("effective_world_brush_radius", "effective_world_brush_shape"):
    require("include/sandhybrid/simulation_policy.hpp", token)
require("include/sandhybrid/ui_layout.hpp", "pointer_to_grid")
for token in (
    "const auto width = (std::max)(state.window_width.load(std::memory_order_relaxed), 1u);",
    ".cursor_x = pointer_over_world ? cursor_x : -1'000'000",
    "policy::effective_world_brush_radius",
    "input_simulation_viewport",
):
    require("src/vulkan_renderer.cpp", token)
for token in ("fill_armed", "World Fill remains click-confirmed",
              "const bool armed_fill_click"):
    require("src/app.cpp", token)

# Pause freezes simulation time but never authoring mutations.
require("include/sandhybrid/simulation_policy.hpp", "editor_mutation_allowed")
require("tests/behavior_contract.cpp", "editor_mutation_allowed(true, false)")
require("src/vulkan_renderer.cpp", "policy::editor_mutation_allowed(paused, reset_this_frame)")
reject("src/vulkan_renderer.cpp", "Fill ignored while paused.")
reject("src/vulkan_renderer.cpp", "Ignite Air ignored while paused.")
reject("src/vulkan_renderer.cpp", "if (!paused && !reset_this_frame) record_paint")

# Inventory and Designer are sidebar-owned nested workspaces.
for token in (
    "inventory_inventory",
    "inventory_blueprints",
    "designer_grid",
    "designer_material_card",
):
    require("include/sandhybrid/ui_layout.hpp", token)
for token in (
    "inventory_pane",
    "designer_pane",
):
    require("include/sandhybrid/shared_state.hpp", token)
for token in (
    "const bool over_designer_grid",
    "paint_designer_grid(shared_state, designer_grid_viewport",
    "ui::inventory_slot_at(layout, input.height, pointer)",
):
    require("src/app.cpp", token)
for token in (
    "uint inventoryPane()",
    "x - contentLeft, y - cardTop",
    "renderPc.selectedInventorySlot == slot",
    "renderPc.selectedWorkspace == 1u &&",
    "uint paneIds[2] = uint[2](166u, 181u)",
    "tab == 0u ? 166u : 181u",
):
    require("shaders/fullscreen.frag", token)
reject("shaders/fullscreen.frag", "if (!mapSample && renderPc.selectedWorkspace == 3u)")
require("tests/ui_layout_contract.cpp", "layout.designer_grid.position.x < sidebar_left")

# Repository memory and mission status retain the release invariants.
for token in ("v2.5.3", "Fix29", "Half Water falls first", "World cursor controls belong to Editor", "sidebar-only", "no prerelease marker"):
    require("AGENTS.md", token)
require("MISSION_LEDGER.md", "/releases/tag/v2.5.15")
require("missioncache.md", "MC-153")

print("v2.5.15 historical macro, hive, cursor, pause-editing, and sidebar contracts valid.")
