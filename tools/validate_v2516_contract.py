#!/usr/bin/env python3
"""Validate the v2.5.16 real-Blueprint, paused-editing, and stable release contract."""

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


# Normal visible release and exact package names.
require("CMakeLists.txt", "VERSION 2.5.18")
require("CHANGELOG.md", "## 2.5.16")
require("RELEASE_NOTES.md", "# SandHybrid v2.5.18")
for token in (
    "SandHybrid-Windows-x64-v2.5.18",
    "SandHybrid-Linux-x64-v2.5.18",
    "refs/tags/v2.5.18",
    "Generate Windows SHA-256",
    "Generate Linux SHA-256",
    "Expected two packages and two checksums",
    "gh release create v2.5.18",
    "--verify-tag",
):
    require(".github/workflows/ci-release.yml", token)
reject(".github/workflows/ci-release.yml", "v2.5.18-test")
reject(".github/workflows/ci-release.yml", "--prerelease")

# Exact bounded payload and transactional transformation core.
for token in (
    "struct SelectionBounds final",
    "align_selection_to_tiles",
    "struct Blueprint final",
    "BlueprintKind",
    "blueprint_destination_coordinate",
    "blueprint_centered_origin",
    "place_blueprint_transactional",
):
    require("include/sandhybrid/blueprint.hpp", token)
for token in (
    "preserve exact cell metadata",
    "Rotation must preserve every source cell",
    "Transparent blueprint cells must not erase",
    "invalid material must reject the full transaction",
    "out-of-bounds paste must not partially write",
):
    require("tests/blueprint_contract.cpp", token)

# Shared honest slots, publication, exclusive clicks, pause, and reset boundaries.
for token in (
    "blueprint_mutex",
    "selected_blueprint_slot",
    "blueprint_placement_active",
    "blueprint_place_requested",
):
    require("include/sandhybrid/shared_state.hpp", token)
for token in (
    "publish_designer_blueprint",
    "blueprint_slot_occupied",
    "const bool blueprint_place_click",
    "!blueprint_placement_active",
    "world_editor_paint_allowed",
):
    require("src/app.cpp", token)
for token in (
    "void place_selected_blueprint",
    "blueprint_payload_valid",
    "VkBufferCopy",
    "bounded transactional upload",
    "BlueprintKind::static_model",
    "BlueprintKind::map_chunk",
    "blueprint_place_requested.exchange",
    "Blueprint placement skipped across a load/reset boundary.",
    "state.blueprint_place_requested.store(false",
):
    require("src/vulkan_renderer.cpp", token)
for token in (
    "true, true, false, false, false, true, true, true));",
    "true, true, false, false, false, true, true, false));",
):
    require("tests/behavior_contract.cpp", token)

# Sidebar rendering is backed by real slot state and the world owns the footprint.
for token in (
    "uint blueprintFlags",
    "blueprintSlotOccupied",
    "selectedBlueprintSlot",
    "blueprintPlacementActive",
    "blueprintWidth",
    "blueprintHeight",
    "occupied ? 181u : 182u",
):
    require("shaders/fullscreen.frag", token)
for token in ("blueprint_flags", "framebuffer_width", "framebuffer_height",
              "sizeof(RenderPush) == 256"):
    require("src/vulkan_renderer.cpp", token)
for token in (
    "designer_blueprint_slot_rect",
    "designer_blueprint_slot_at",
):
    require("include/sandhybrid/ui_layout.hpp", token)
require("tests/ui_layout_contract.cpp", "blueprint_slot_count")

# Repository memory and mission evidence remain honest about incomplete work.
for token in (
    "Blueprint slots are shared",
    "Player presence or mining mode may suppress",
    "hard-coded suspended Ecosystem hive",
):
    require("AGENTS.md", token)
for token in (
    "v2.5.16 paused character-scene editing and real Blueprint slots",
    "World-selection capture, thumbnails, persistence",
    "26/26 tests",
):
    require("missioncache.md", token)
require("MISSION_LEDGER.md", "v2.5.16")
require("MISSION_LEDGER.md", "/releases/tag/v2.5.15")
require("MISSION_LEDGER.md", "/releases/tag/v2.5.16")
require("MISSION_LEDGER.md", "aa497b8e75bb30a55db1b96a94f35c3fc75dd8c3")

print("v2.5.16 real Blueprint, paused-editing, sidebar, and stable release contracts valid.")
