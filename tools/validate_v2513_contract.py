#!/usr/bin/env python3
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]


def text(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def require(path: str, token: str) -> None:
    if token not in text(path):
        raise SystemExit(f"{path}: missing {token!r}")


# Canonical v2.5.13 history remains present while current release policy is
# owned by the newest release contract.
require("CHANGELOG.md", "## 2.5.13")
require("CMakeLists.txt", "VERSION 2.5.16")
if list(ROOT.glob("RELEASE_NOTES_v*.md")):
    raise SystemExit("versioned release-note files remain; CHANGELOG.md must own history")

# The first 28 P0 IDs are cached and each resolves to an active row.
cache = text("missioncache.md")
priority = next(line for line in cache.splitlines()
                if line.startswith("- **P0 / primary release gate:**"))
first_28 = re.findall(r"MC-\d{3}", priority)[:28]
expected = [
    "MC-012", "MC-013", "MC-017", "MC-018", "MC-026", "MC-031", "MC-032",
    "MC-038", "MC-039", "MC-068", "MC-074", "MC-079", "MC-084", "MC-097",
    "MC-104", "MC-105", "MC-106", "MC-107", "MC-112", "MC-113", "MC-114",
    "MC-115", "MC-116", "MC-118", "MC-119", "MC-120", "MC-122", "MC-123",
]
if first_28 != expected:
    raise SystemExit(f"first P0 tranche changed unexpectedly: {first_28}")
for mission in expected:
    if not re.search(rf"^\| {mission} \|", cache, re.MULTILINE):
        raise SystemExit(f"{mission} has no active mission row")

# Half Water and media packet fixes.
for token in (
    "canonical_air_state = 54u",
    "half_water_attraction_min_cells = 2u",
    "half_water_attraction_max_cells = 4u",
    "half_water_can_sleep",
    "medium_packet_tries_macro",
    "medium_packet_needs_fine_fallback",
):
    require("include/sandhybrid/simulation_policy.hpp", token)
require("shaders/move.comp", "material == MAT_ATMOSPHERE && volume == 0u ? 54u : volume")
for token in (
    "halfWaterPartnerVisible",
    "halfWaterAttractionPending",
    "bool macroLiquid = fullLiquid && (moving || liquidEnclosed)",
    "bool macroGas = fullGas && (moving || gasEnclosed)",
    "fineFallbackMedium",
):
    require("shaders/tiles.comp", token)

# Hard pause/reset prevents hidden state progression or queued action leakage.
for token in (
    ".step = 0u",
    "state.primary_down.store(false",
    "state.fill_region.store(false",
    "state.ignite_air.store(false",
    "if (map_visible && !paused && !reset_this_frame",
):
    require("src/vulkan_renderer.cpp", token)

# Every tranche mission resolves to explicit deterministic/source evidence.
evidence = {
    "MC-012": (("tests/behavior_contract.cpp", ("medium_packet_tries_macro",)),),
    "MC-013": (("shaders/tiles.comp", ("bool macroLiquid = fullLiquid && (moving || liquidEnclosed)",)),),
    "MC-017": (("tests/behavior_contract.cpp", ("half_water_can_sleep",)),),
    "MC-018": (("tests/packet_transaction_contract.cpp", ("blocked", "commit")),),
    "MC-026": (("tests/atmosphere_contract.cpp", ("transfer_atmosphere", "respire")),),
    "MC-031": (("tests/actor_medium_contract.cpp", ("drowning", "suffocating")),),
    "MC-032": (("tools/audit_ecology_motion.py", ("BEE_COLONY_MAX = 100u",)),),
    "MC-038": (("tests/scene_image_contract.cpp", ("queen", "structural")),),
    "MC-039": (("tests/actor_medium_contract.cpp", ("impulse_x", "impulse_y")),),
    "MC-068": (("tests/input_routing_contract.cpp", ("camera_wasd_enabled",)),
               ("tests/behavior_contract.cpp", ("map_view_width",))),
    "MC-074": (("tests/world_layout_contract.cpp", ("authored_scene_origin",)),),
    "MC-079": (("tests/behavior_contract.cpp", ("canonical_air_state == 54u",)),),
    "MC-084": (("tests/behavior_contract.cpp", ("half_water_attraction_distance",)),),
    "MC-097": (("tests/behavior_contract.cpp", ("effective_wet_density",)),),
    "MC-104": (("tests/packet_transaction_contract.cpp", ("blocked", "commit")),),
    "MC-105": (("tests/scene_image_contract.cpp", ("structural",)),),
    "MC-106": (("tests/atmosphere_contract.cpp", ("make_earth_atmosphere", "pressure_units")),),
    "MC-107": (("tests/machinery_contract.cpp", ("direction", "blocked")),
               ("tests/actor_medium_contract.cpp", ("ActorOccupancy",))),
    "MC-112": (("tests/material_contract.cpp", ("iron_ore", "is_block_material")),),
    "MC-113": (("tests/machinery_contract.cpp", ("blocked", "direction")),),
    "MC-114": (("tests/world_layout_contract.cpp", ("resident_substrate",)),),
    "MC-115": (("tests/scene_image_contract.cpp", ("legacy", "load_scene_ppm")),),
    "MC-116": (("tools/validate_mission_cache.py", ("missing_priority", "duplicate_priority")),),
    "MC-118": (("tests/terrain_generation_contract.cpp", ("sand_trap", "iron_ore")),),
    "MC-119": (("tests/input_routing_contract.cpp", ("player_wasd_enabled", "camera_wasd_enabled")),
               ("tests/ui_layout_contract.cpp", ("make_map_overlay_viewport",))),
    "MC-120": (("tests/input_routing_contract.cpp", ("edge_pan_direction",)),),
    "MC-122": (("tests/ui_layout_contract.cpp", ("designer_blueprints", "inventory_slot_rect")),),
    "MC-123": (("missioncache.md", ("Cross-system packaged runtime acceptance",)),),
}
if list(evidence) != expected:
    raise SystemExit("first-28 evidence matrix does not exactly match P0 tranche order")
for mission, checks in evidence.items():
    for path, tokens in checks:
        for token in tokens:
            require(path, token)

require("tools/audit_ecology_motion.py", "BEE_COLONY_MAX = 100u")
require("tools/audit_fix34.py", "BEE_INITIAL_PACKED")
require("missioncache.md", "Cross-system packaged runtime acceptance")

# Every material identifier used by a shader must exist in the canonical ID table.
material_ids = set(re.findall(r"const uint (MAT_[A-Z0-9_]+)", text("shaders/material_ids.glsl")))
for shader in sorted((ROOT / "shaders").iterdir()):
    if shader.suffix not in {".comp", ".frag", ".vert", ".glsl"}:
        continue
    references = set(re.findall(r"\b(MAT_[A-Z0-9_]+)\b", shader.read_text(encoding="utf-8")))
    undefined = sorted(references - material_ids)
    if undefined:
        raise SystemExit(f"{shader.relative_to(ROOT)}: undefined material IDs: {undefined}")
require("shaders/fullscreen.frag",
        "cell.material == MAT_LIGHTNING || cell.material == MAT_EMBER")
require("src/vulkan_renderer.cpp",
        ".day_cycle_steps = sandhybrid::policy::day_cycle_steps,")

print("Phase-one first-28 P0 recovery contracts valid.")
