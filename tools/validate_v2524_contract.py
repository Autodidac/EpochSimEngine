#!/usr/bin/env python3
"""Validate the v2.5.26 horizontal-World, packet, Volcano, and Fix29 hive gate."""
from __future__ import annotations

import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def read(relative: str) -> str:
    return (ROOT / relative).read_text(encoding="utf-8")


def require(relative: str, *tokens: str) -> None:
    text = read(relative)
    missing = [token for token in tokens if token not in text]
    if missing:
        raise SystemExit(f"{relative} missing v2.5.26 contract tokens: {missing}")


def reject(relative: str, *tokens: str) -> None:
    text = read(relative)
    found = [token for token in tokens if token in text]
    if found:
        raise SystemExit(f"{relative} retains rejected v2.5.26 tokens: {found}")


require("CMakeLists.txt", "VERSION 2.5.29", "sandhybrid_v2524_source_contract")
require("RELEASE_NOTES.md", "# EpochSimEngine v2.5.29",
        "SandHybrid-Windows-x64-v2.5.29", "SandHybrid-Linux-x64-v2.5.29")
require(".github/workflows/ci-release.yml", "refs/tags/v2.5.26",
        "SandHybrid-Windows-x64-v2.5.26", "SandHybrid-Linux-x64-v2.5.26",
        "gh release create v2.5.26")
reject(".github/workflows/ci-release.yml", "refs/tags/v2.5.23", "prerelease: true")
require("include/sandhybrid/world_layout.hpp",
        "persistent_world_district_columns = 8u",
        "persistent_world_district_rows = 1u",
        "Scene::sandbox, Scene::ecosystem, Scene::engineering_lab, Scene::frontier_base",
        "Scene::volcano, Scene::waterworks, Scene::gold_mine, Scene::demolition")
require("shaders/reset.comp", "PERSISTENT_WORLD_DISTRICTS = ivec2(8, 1)",
        "int buriedBase = min(world.y - 4, surfaceRow + 13)",
        "Compact far-right stepped cone from the supplied silhouette")
require("shaders/chemistry.comp", "result.aux &= ~AUX_MOVED;")
require("shaders/tiles.comp", "mediumTravelSteps >= TILE_MEDIUM_PROGRESS_LIMIT",
        "mediumBlockedAttempts >= TILE_MEDIUM_PROGRESS_LIMIT",
        "productiveMediumMove")
require("shaders/bee_swarm.glsl",
        "beeUsesPersistentWorldHome(uint width, uint height)",
        "int(width) >= BEE_PERSISTENT_WORLD_CELLS.x",
        "district << 20u", "BEE_FORMATION_COUNT = 60u")
reject("shaders/bee_swarm.glsl", "BEE_PERSISTENT_HOME_BIT")
require("src/vulkan_renderer.cpp", "macro_liquid_consecutive_packets",
        "enclosed_air_remains_tiled", "bee_metadata_mismatches",
        "unique_bee_slots == fix29_bee_formation_count")
require("missioncache.md", "2026-08-16 v2.5.24 horizontal-World and stuck-tile contradiction cache")
require("MISSION_LEDGER.md", "v2.5.24 contradiction audit")

for validator in ("audit_fix34.py", "audit_ecology_motion.py", "validate_shader_contracts.py"):
    subprocess.run([sys.executable, str(ROOT / "tools" / validator)], cwd=ROOT, check=True)

print("v2.5.26 horizontal World, productive packet, sunken Volcano, and Fix29 hive contracts valid.")
