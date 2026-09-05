#!/usr/bin/env python3
"""Validate the v2.5.26 exact Fix29 hive and liquid-equilibrium recovery."""

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def require(relative: str, *tokens: str) -> None:
    text = (ROOT / relative).read_text(encoding="utf-8")
    missing = [token for token in tokens if token not in text]
    if missing:
        raise SystemExit(f"{relative} missing v2.5.26 contract tokens: {missing}")


def reject(relative: str, *tokens: str) -> None:
    text = (ROOT / relative).read_text(encoding="utf-8")
    found = [token for token in tokens if token in text]
    if found:
        raise SystemExit(f"{relative} retains rejected v2.5.26 tokens: {found}")


require("CMakeLists.txt", "VERSION 2.5.28", "sandhybrid_v2522_source_contract")
require("RELEASE_NOTES.md", "# SandHybrid v2.5.28",
        "SandHybrid-Windows-x64-v2.5.28", "SandHybrid-Linux-x64-v2.5.28")
require(".github/workflows/ci-release.yml", "refs/tags/v2.5.26",
        "SandHybrid-Windows-x64-v2.5.26", "SandHybrid-Linux-x64-v2.5.26",
        "gh release create v2.5.26", "group: sandhybrid-v2526-")
reject(".github/workflows/ci-release.yml", "v2.5.26-test", "prerelease: true")

require("shaders/beehive.glsl",
        "ivec2 cell = queen + offset;",
        "uint cellIndex = uint(cell.y) * BEEHIVE_CANONICAL_WIDTH + uint(cell.x);",
        "hash32(cellIndex ^ BEEHIVE_CANONICAL_SEED)")
require("shaders/reset.comp", "beehivePrefabEntropy(queen, offset)")
require("shaders/paint.comp", "beePersistentAddress(center, pc.width, pc.height")
require("shaders/paint.comp", "beehivePrefabEntropy(prefabQueen, delta)")
require("include/sandhybrid/actor_medium.hpp",
        "fix29_hive_entropy(", "fix29_hive_sandbox_queen_y", "fix29_hive_ecosystem_queen_y")
require("tests/actor_medium_contract.cpp", "0x1c707b05u", "0x04572a8au")
require("tests/scene_image_contract.cpp", "const auto exact_hive", "queen_y")

require("shaders/move.comp",
        "if ((cell.aux & AUX_MOVED) != 0u) return false;",

        "int liquidDropDistance",
        "forwardDrop == oppositeDrop && direction > 0",
        "((a.aux | b.aux) & AUX_MOVED) == 0u")
require("shaders/tiles.comp", "mediumTravelSteps >= TILE_MEDIUM_PROGRESS_LIMIT",
        "mediumBlockedAttempts >= TILE_MEDIUM_PROGRESS_LIMIT")
require("shaders/tiles.comp",
        "uint liquidMinimumAge = 0xffffffffu;",
        "liquidMinimumAge = min(liquidMinimumAge, cell.age);",
        "!settledFineMedium && !settledHalfWater")
require("src/vulkan_renderer.cpp",
        "std::array<std::int32_t, 7>",
        'append("placed_fix29_hive_exact"',
        'append("water_surface_zero_jitter"',
        'append("half_water_bounded_attraction_merge"',
        'append("half_water_keeps_dripping"',
        'append("full_water_crosses_unsupported_ledge"')
reject("src/vulkan_renderer.cpp", "std::array<std::int32_t, 15>")

require("missioncache.md",
        "## Local post-v2.5.21 hive and liquid-surface recovery — 2026-08-10",
        "A coordinate mismatch fails even when aggregate counts match.",
        "then an observation window with conserved half-units")
require("MISSION_LEDGER.md", "## Active local post-v2.5.21 recovery")

print("v2.5.26 exact Fix29 hive and liquid-equilibrium contracts valid.")
