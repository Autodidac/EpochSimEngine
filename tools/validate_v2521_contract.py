#!/usr/bin/env python3
"""Validate the v2.5.21 systems, scene, atmosphere, UI, and release contracts."""

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def source(relative: str) -> str:
    return (ROOT / relative).read_text(encoding="utf-8")


def require(relative: str, *tokens: str) -> None:
    text = source(relative)
    missing = [token for token in tokens if token not in text]
    if missing:
        raise SystemExit(f"{relative} missing v2.5.21 contract tokens: {missing}")


def reject(relative: str, *tokens: str) -> None:
    text = source(relative)
    found = [token for token in tokens if token in text]
    if found:
        raise SystemExit(f"{relative} retains rejected v2.5.21 tokens: {found}")


require("CMakeLists.txt", "VERSION 2.5.21", "validate_v2521_contract.py")
require(
    ".github/workflows/ci-release.yml",
    "refs/tags/v2.5.21",
    "SandHybrid-Windows-x64-v2.5.21",
    "SandHybrid-Linux-x64-v2.5.21",
    "gh release create v2.5.21",
    "group: sandhybrid-v2521-",
)
reject(".github/workflows/ci-release.yml", "v2.5.21-test", "prerelease: true")
require(
    "missioncache.md",
    "## v2.5.21 systems, presentation, atmosphere, scene, and machinery recovery",
    "Acceptance rules cached before implementation",
    "less than 3%",
    "`30`, `60`, `120`, and `UNLIMITED`",
)
require(
    "third_party/EpochGui/SNAPSHOT.md",
    "d8decc9ee2e73e0009f1e8c49d86a52db6748b28",
    "v0.88.75",
)

# The photographed historical compact hive is identical in shader reset/tool,
# CPU load normalization, deterministic classification, and packaged readback.
require(
    "shaders/beehive.glsl",
    "BEEHIVE_SHELL_MIN_RADIUS_SQUARED = 24",
    "BEEHIVE_SHELL_MAX_RADIUS_SQUARED = 88",
    "BEEHIVE_CHAMBER_RADIUS_SQUARED = 24",
    "BEEHIVE_EXIT_MAX_X = 10",
    "BEEHIVE_CANONICAL_SEED = 0xD17A5EEDu",
)
require(
    "tools/validate_shader_contracts_legacy.py",
    "BEEHIVE_SHELL_MIN_RADIUS_SQUARED = 24",
    "BEEHIVE_SHELL_MAX_RADIUS_SQUARED = 88",
    "BEEHIVE_EXIT_MAX_X = 10",
)
require(
    "src/scene_image.cpp",
    "beehive_shell_min_radius_squared = 24",
    "beehive_shell_max_radius_squared = 88",
    "beehive_exit_max_x = 10",
)
require(
    "tests/scene_image_contract.cpp",
    "shell_count != 193u",
    "honey_count != 18u",
    "pollen_count != 22u",
)
reject("include/sandhybrid/actor_medium.hpp", "0xD17A55DEu", "dx <= 12", "radius_squared >= 28")

# Actions and presentation caps share one logical sidebar layout. Presentation
# selection never changes the independently scheduled fixed simulation tick.
require(
    "include/sandhybrid/ui_layout.hpp",
    "actions{}, ignite_air{}, keymap{}",
    "settings_fps{}, fps_30{}, fps_60{}, fps_120{}, fps_unlimited{}",
    "ignite_air_action_at",
)
require(
    "tests/ui_layout_contract.cpp",
    "layout.actions.position.y + layout.actions.size.y > layout.keymap.position.y",
    "layout.settings_fps",
    "layout.fps_unlimited",
)
require(
    "include/sandhybrid/shared_state.hpp",
    "presentation_limit{2}",
)
require(
    "src/vulkan_renderer.cpp",
    "const bool present_frame",
    "const std::uint32_t simulation_ticks = simulation_due ? 1u : 0u",
    "active_limit != 0u",
    "max_time_debt_ticks = 2u",
    "scene == Scene::sandbox ? 576u : 571u",
    "mismatches == 0u && shell == 193u",
    "const auto expected_lava = scene == Scene::volcano",
    "stone + lava == expected",
)

# Static normal materials, authoritative-state gas smoothing, and bounded debug.
require(
    "shaders/material_appearance.glsl",
    "if (materialAppearanceClass(cell.material) != 2u) return base;",
    "established metal/ore glint",
)
require("shaders/fullscreen.frag", "vec4 gasPresentation")
require("tools/generate_ui_text.py", "SAMPLED REGION")
require(
    "shaders/debug_stats.comp",
    "uint sampleRegion = pc.reserved & 15u",
    "uint sampleCellCount = sampleWidth * sampleHeight",
)
require(
    "src/vulkan_renderer.cpp",
    "(debug_sample_frame % 60u) == 0u",
    ".reserved = (debug_sample_frame / 60u) & 15u",
)

# Volcano/geology, conservative atmosphere/Vacuum behavior, and combustion rates.
require(
    "shaders/reset.comp",
    "uint volcanoMaterial(ivec2 p)",
    "AUTHORED_WORLD_CELLS.x * 4 / 5",
    "MAT_MAGMA_VENT",
)
require("shaders/terrain_generation.glsl", "terrainHash(tile, 0xb04du) % 16u == 0u")
require("include/sandhybrid/terrain_generation.hpp", "(hash(tile_x, tile_y, 0xb04du) % 16u) == 0u")
require(
    "shaders/move.comp",
    "bool mixAtmospherePair",
    "bool expandAirIntoVacuum",
)
require(
    "tests/atmosphere_contract.cpp",
    "transfer_atmosphere(source, destination, 12'345u)",
    "enrich_atmosphere",
)
require(
    "shaders/chemistry.comp",
    "(randomValue & 15u) == 0u ? MAT_EMBER : MAT_SMOKE",
    "source.age > 260u && (randomValue & 255u) == 0u",
)

# Sluice and players are result-covered and production-backed.
require(
    "shaders/chemistry.comp",
    "resourceMaterial == MAT_SAND",
    "resourceMaterial == MAT_SILT",
    "currentInventory.z == 0u && currentInventory.w == 0u",
    "inventory.w = processSand ? 1u : 2u",
    "% 10u) == 0u",
)
require(
    "tests/machinery_contract.cpp",
    "blocked_sluice",
    "blocked_solid_output",
    "expected_solid",
)
require("include/sandhybrid/scene.hpp", "return scene != Scene::count;")
for token in (
    "Scene::sandbox", "Scene::blank", "Scene::volcano", "Scene::waterworks",
    "Scene::ecosystem", "Scene::engineering_lab", "Scene::gold_mine",
    "Scene::demolition", "Scene::frontier_base",
):
    require("include/sandhybrid/scene_spawn.hpp", token)
require("tests/scene_spawn_contract.cpp", "for (std::uint32_t index = 0u; index < scene_count; ++index)")

print("v2.5.21 systems, scene, atmosphere, hive, UI, machinery, and release contracts valid.")