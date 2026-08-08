#!/usr/bin/env python3
"""Validate the v2.5.19 packaged macro/Half Water/hive/cursor recovery gate."""

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


# Stable visible publication and exact native package names.
require("CMakeLists.txt", "VERSION 2.5.19")
require("CHANGELOG.md", "## 2.5.17")
require("RELEASE_NOTES.md", "# SandHybrid v2.5.19")
for token in (
    "SandHybrid-Windows-x64-v2.5.19",
    "SandHybrid-Linux-x64-v2.5.19",
    "refs/tags/v2.5.19",
    "gh release create v2.5.19",
    "--verify-tag",
    "Expected two packages and two checksums",
):
    require(".github/workflows/ci-release.yml", token)
reject(".github/workflows/ci-release.yml", "v2.5.19-test")
reject(".github/workflows/ci-release.yml", "--prerelease")

# The reserved Half Water bit cannot leak out of ordinary CPU full-Water constructors.
require("src/vulkan_renderer.cpp", "fill_aux_random_mask = 0x007fff00u")
require("src/scene_image.cpp", "aux_random_mask = 0x007fff00u")
require("tests/scene_image_contract.cpp", "aux_water_half")
require("tests/scene_image_contract.cpp", "Material::honey")
require("tests/scene_image_contract.cpp", "Material::pollen")

# One canonical Fix36 prefab entropy owns reset, paint, CPU classification, and load normalization.
for token in (
    "BEEHIVE_CANONICAL_WIDTH = 640u",
    "BEEHIVE_CANONICAL_ECOSYSTEM_QUEEN = ivec2(512, 232)",
    "BEEHIVE_CANONICAL_SANDBOX_QUEEN = ivec2(512, 234)",
    "BEEHIVE_CANONICAL_SEED = 0xD17A5EEDu",
    "beehivePrefabEntropy",
):
    require("shaders/beehive.glsl", token)
require("shaders/reset.comp", "beehivePrefabEntropy(queen, offset)")
require("shaders/paint.comp", "beehivePrefabEntropy(center, delta)")
for token in (
    "pre_pr19_hive_canonical_width = 640u",
    "pre_pr19_hive_canonical_seed = 0xD17A5EEDu",
    "canonical_pre_pr19_hive_entropy",
):
    require("include/sandhybrid/actor_medium.hpp", token)
require("src/scene_image.cpp", "pre_pr19_hive_hash((y * pre_pr19_hive_canonical_width + x) ^ pre_pr19_hive_canonical_seed)")
require("tests/actor_medium_contract.cpp", "0x95ef5950u")

# Packaged executable runs real Vulkan seed/step/readback checks and reports failure.
for token in (
    "--runtime-acceptance-report",
    "runtime_acceptance_report",
):
    require("src/main.cpp", token)
require("src/app.cpp", "runtime_acceptance ? pre_expansion_world_width : world.width")
require("src/app.cpp", "runtime_acceptance ? pre_expansion_world_height : world.height")
for token in (
    "run_runtime_acceptance",
    "run_acceptance_tile_pass",
    "run_acceptance_macro_pass",
    "run_acceptance_fine_pass",
    "run_acceptance_horizontal_pass",
    "acceptance_width = (std::min)(config.grid_width, 192u)",
    "acceptance_height = (std::min)(config.grid_height, 192u)",
    "macro_liquid_exact_packet",
    "macro_gas_exact_packet",
    "macro_blocked_fine_fallback",
    "half_water_bounded_attraction_merge",
    "half_water_falls_first",
    "half_water_keeps_dripping",
    "supplied_ledge_creates_half_water",
    "ecosystem_hard_coded_hive",
    "sandbox_hard_coded_hive",
    "return passed ? 0 : 3",
    "\\\"backend\\\": \\\"vulkan\\\"",
):
    require("src/vulkan_renderer.cpp", token)
require("src/vulkan_renderer.cpp", "if (config.runtime_acceptance_report.empty())")

# Rendering converts physical fragments to the same logical geometry used by native input.
for token in (
    "uint framebufferWidth",
    "uint framebufferHeight",
    "logicalWindowPixel",
):
    require("shaders/fullscreen.frag", token)
for token in (
    "framebuffer_width = swapchain_extent.width",
    "framebuffer_height = swapchain_extent.height",
    "sizeof(RenderPush) == 256",
):
    require("src/vulkan_renderer.cpp", token)
require("include/sandhybrid/ui_layout.hpp", "framebuffer_to_logical_pointer")
require("tests/ui_layout_contract.cpp", "1920u, 1080u, 1280u, 720u")

# Repository memory and release docs preserve active acceptance instead of overclaiming.
for token in (
    "canonical Fix36 Ecosystem-local entropy",
    "Water Half flag `0x00800000` is reserved state",
    "physical framebuffer coordinates to logical window coordinates",
):
    require("AGENTS.md", token)
for token in (
    "Post-v2.5.16 packaged state-readback recovery",
    "No affected mission becomes COMPLETE",
    "packaged Vulkan state-readback",
):
    require("missioncache.md", token)
require("MISSION_LEDGER.md", "v2.5.17 publication record")
require("VALIDATION.md", "--runtime-acceptance-report")

print("v2.5.19 packaged macro, Half Water, hive, cursor, and stable release contracts valid.")
