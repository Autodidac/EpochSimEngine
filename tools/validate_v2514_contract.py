#!/usr/bin/env python3
"""Validate the v2.5.14 stable recovery and publication contract."""

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


# Stable, visible, tag-gated publication with packaged checksums.
require("CMakeLists.txt", "VERSION 2.5.14")
require("CHANGELOG.md", "## 2.5.14")
require("RELEASE_NOTES.md", "# SandHybrid v2.5.14")
for token in (
    "SandHybrid-Windows-x64-v2.5.14",
    "SandHybrid-Linux-x64-v2.5.14",
    "refs/tags/v2.5.14",
    "Generate Windows SHA-256",
    "Generate Linux SHA-256",
    "Expected two packages and two checksums",
    "gh release create v2.5.14",
    "--verify-tag",
):
    require(".github/workflows/ci-release.yml", token)
reject(".github/workflows/ci-release.yml", "v2.5.14-test")
reject(".github/workflows/ci-release.yml", "--prerelease")

# v2.5.3 macro ownership: moving complete media get a packet attempt.
for token in (
    "bool macroLiquid = fullLiquid && (moving || liquidEnclosed)",
    "bool macroGas = fullGas && (moving || gasEnclosed)",
    "fineFallbackMedium",
):
    require("shaders/tiles.comp", token)
for token in ("gas_tile_eligible(true, true, false)",
              "liquid_tile_eligible(true, true, false)"):
    require("tests/behavior_contract.cpp", token)

# Fix29 hive body and deterministic contents, without importing foreign actor behavior.
for token in (
    "BEEHIVE_SHELL_MIN_RADIUS_SQUARED = 28",
    "BEEHIVE_SHELL_MAX_RADIUS_SQUARED = 108",
    "BEEHIVE_CHAMBER_RADIUS_SQUARED = 28",
    "BEEHIVE_EXIT_MAX_X = 12",
):
    require("shaders/beehive.glsl", token)
for token in (
    "beehive_shell_min_radius_squared = 28",
    "beehive_shell_max_radius_squared = 108",
    "beehive_chamber_radius_squared = 28",
    "beehive_exit_max_x = 12",
):
    require("src/scene_image.cpp", token)

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
for token in ("v2.5.3", "Fix29 Beehive", "sidebar-only", "no prerelease marker"):
    require("AGENTS.md", token)
require("MISSION_LEDGER.md", "Target: `v2.5.14`")
require("missioncache.md", "MC-153")

print("v2.5.14 stable macro, hive, pause-editing, sidebar, and release contracts valid.")
