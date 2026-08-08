#!/usr/bin/env python3
"""Validate the v2.5.20 scenic full-tile, live-tool, player, and fixed-step recovery."""

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


# Stable, visible native release contract.
require("CMakeLists.txt", "VERSION 2.5.20")
require("RELEASE_NOTES.md", "# SandHybrid v2.5.20")
for token in (
    "SandHybrid-Windows-x64-v2.5.20",
    "SandHybrid-Linux-x64-v2.5.20",
    "refs/tags/v2.5.20",
    "gh release create v2.5.20",
    "--verify-tag",
):
    require(".github/workflows/ci-release.yml", token)
reject(".github/workflows/ci-release.yml", "v2.5.20-test")
reject(".github/workflows/ci-release.yml", "--prerelease")

# Every authored scene owns a full 8x8 surface row and intentional interior air.
for token in (
    "scene_surface_tile_row",
    "case Scene::frontier_base: return 17u",
    "static_cast<std::int32_t>(scene_surface_tile_row(scene) * foundation)",
):
    require("include/sandhybrid/world_layout.hpp", token)
for token in (
    "sceneSurfaceTileRow",
    "scene == SCENE_FRONTIER_BASE) return 17",
    "scene == SCENE_BLANK && !authoredFoundation",
):
    require("shaders/reset.comp", token)
require("src/vulkan_renderer.cpp", "inside_authored && scene != Scene::blank")
for token in ("Scene::volcano", "Scene::ecosystem", "Scene::frontier_base", "for (std::uint32_t y = surface; y < surface + 8u; ++y)"):
    require("tests/world_layout_contract.cpp", token)

# Sidebar tools remain live: labelled `CLEAR` is isolated and Fill uses bounded resident writes.
for token in ("clear_designer_grid", "designer_workspace &&", "clear_designer_grid(shared_state)"):
    require("src/app.cpp", token)
require("tools/generate_ui_text.py", "CLEAR")
require("shaders/fullscreen.frag", "renderPc.selectedWorkspace == 3u ? 160u : 108u")
for token in ("flood_replace_connected", "upload_bounded_cells", '"Air fill"'):
    require("src/vulkan_renderer.cpp", token)

# Player scenes recover to deterministic supported, breathable positions.
for token in ("spawnSupportedAndBreathable", "recoverActorSpawn", "bodyClear(center)"):
    require("shaders/actor.comp", token)
require("include/sandhybrid/scene_spawn.hpp", "Scene::demolition: return {80, 327, 12u, true}")
require("include/sandhybrid/scene_spawn.hpp", "Scene::frontier_base: return {168, 207, 24u, true}")
require("tests/scene_spawn_contract.cpp", "SceneSpawn{1360, 1047, 12u, true}")
require("tests/scene_spawn_contract.cpp", "SceneSpawn{1448, 927, 24u, true}")

# Macro packets run at half cadence and survive two classifier ticks before breakup.
for token in (
    "macro_packet_interval_ticks = 2u",
    "exposed_packet_breakup_ticks = 2u",
    "macro_packet_step_due",
):
    require("include/sandhybrid/simulation_policy.hpp", token)
for token in ("macroCadenceCarry", "macroStepDue", "mediumExposureTicks >= 2u"):
    require("shaders/tiles.comp", token)
for token in ("macro_step_due", "if (macro_step_due)", ".reserved = 2u, // Focused acceptance"):
    require("src/vulkan_renderer.cpp", token)
require("tests/behavior_contract.cpp", "macro_packet_step_due(2u)")
for token in (
    "bool sourceEligible",
    "TILE_ACTIVE | TILE_FINE_ACTIVE",
    "TILE_SLEEPING | TILE_FINE_ACTIVE | TILE_SETTLED_MEDIUM",
):
    require("shaders/macro_move.comp", token)

# Fixed 60 Hz ticks, one-submit debt shedding, and once-only input edges make simulation frame independent.
for token in (
    "const bool simulation_due = before_draw >= next_simulation",
    "const std::uint32_t simulation_ticks = simulation_due ? 1u : 0u",
    "max_time_debt_ticks = 2u",
    "next_simulation = before_draw + simulation_interval",
    "consume_one_shots",
    "const bool deposit_pressed = consume_one_shots",
):
    require("src/vulkan_renderer.cpp", token)
for token in (
    "full 8x8",
    "labelled `CLEAR`",
    "fixed simulation ticks",
):
    require("missioncache.md", token)
for token in ("aligned `8x8`", "Designer exposes an explicit sidebar `CLEAR`", "A presented frame submits at most one complete tick"):
    require("AGENTS.md", token)

# Every release carries the complete current EpochGui dependency and an exact upstream pin.
for token in (
    "d8decc9ee2e73e0009f1e8c49d86a52db6748b28",
    "v0.88.75",
    "complete vendored EpochGui dependency",
):
    require("third_party/EpochGui/SNAPSHOT.md", token)
for path in (
    "third_party/EpochGui/include/gui/panel_host.hpp",
    "third_party/EpochGui/include/gui/text_control.hpp",
    "third_party/EpochGui/modules/epoch.gui.ixx",
    "third_party/EpochGui/src/epochgui/panel_host.cpp",
    "third_party/EpochGui/tests/text_control_tests.cpp",
):
    if not (ROOT / path).is_file():
        raise SystemExit(f"missing complete EpochGui dependency file: {path}")
for token in ("EPOCHGUI_BUILD_MODULES", "add_library(EpochGui STATIC", "add_library(EpochGui INTERFACE"):
    require("third_party/EpochGui/CMakeLists.txt", token)
require("CMakeLists.txt", "add_subdirectory(third_party/EpochGui)")
reject("CMakeLists.txt", "add_subdirectory(third_party/EpochGui EXCLUDE_FROM_ALL)")
require("AGENTS.md", "Never silently retain an older EpochGui pin")
print("v2.5.20 scenic full-tile, live-tool, player, macro cadence, and fixed-step contracts valid.")
