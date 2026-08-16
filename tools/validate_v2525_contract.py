#!/usr/bin/env python3
"""Validate the v2.5.25 distributed World, common terrain, and persistent Fix29 hive gate."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def read(relative: str) -> str:
    return (ROOT / relative).read_text(encoding="utf-8")


def require(relative: str, *tokens: str) -> None:
    text = read(relative)
    missing = [token for token in tokens if token not in text]
    if missing:
        raise SystemExit(f"{relative} missing v2.5.25 contract tokens: {missing}")


require("CMakeLists.txt", "VERSION 2.5.25", "sandhybrid_v2525_source_contract")
require("RELEASE_NOTES.md", "# SandHybrid v2.5.25",
        "SandHybrid-Windows-x64-v2.5.25", "SandHybrid-Linux-x64-v2.5.25")
require(".github/workflows/ci-release.yml", "refs/tags/v2.5.25",
        "SandHybrid-Windows-x64-v2.5.25", "SandHybrid-Linux-x64-v2.5.25",
        "gh release create v2.5.25", "group: sandhybrid-v2525-")
require("include/sandhybrid/world_layout.hpp",
        "persistent_world_gap_slots =", "persistent_world_district_gap",
        "persistent_world_surface_y", "persistent_world_district_origin_y")
require("tests/world_layout_contract.cpp", "persistent_world_district_gap",
        "persistent_world_surface_y", "persistent_world_district_origin_x(resident_world_width, 7u) == 9576u")
require("tests/scene_spawn_contract.cpp", "SceneSpawn{4272, 1111, 24u, true}",
        "large_startup_district_overlap_count() == 3u")
require("shaders/reset.comp", "persistentWorldGap", "persistentWorldSurfaceY",
        "beehiveFixedContentForScene")
require("shaders/paint.comp", "makeStructuralCell(prefab, true)")
require("shaders/move.comp", "isHiveBoundContent")
require("shaders/chemistry.comp", "fixedHiveContent", "durableStructural = durableStructural || fixedHiveContent")
require("src/scene_image.cpp", "fixed_hive_content")
require("src/vulkan_renderer.cpp", "placed_fix29_hive_delayed_body_exact",
        "persistent_world_startup_sparse_footprint", "startup_districts <= 3u")
require("missioncache.md", "2026-08-16 v2.5.25 distributed-World and persistent-hive recovery cache")
require("MISSION_LEDGER.md", "v2.5.25 distributed-World runtime gate")

print("v2.5.25 distributed World, common grass level, sparse startup, and persistent Fix29 hive contracts valid.")