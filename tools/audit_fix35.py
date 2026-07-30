#!/usr/bin/env python3
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]


def text(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def require(path: str, needle: str) -> None:
    if needle not in text(path):
        raise SystemExit(f"{path}: missing Fix35 contract: {needle}")


swarm = text("shaders/bee_swarm.glsl")
match = re.search(r"BEE_INITIAL_PACKED\[BEE_FORMATION_COUNT\].*?uint\[\]\((.*?)\);", swarm, re.S)
if not match:
    raise SystemExit("bee_swarm.glsl: packed hidden-mask table missing")
values = [int(value) for value in re.findall(r"(\d+)u", match.group(1))]
if len(values) != 200 or len(set(values)) != 200:
    raise SystemExit(f"bee_swarm.glsl: expected 200 unique initial slots, got {len(values)}/{len(set(values))}")
require("shaders/bee_swarm.glsl", "BEE_SWARM_BIOHAZARD_TICKS = 1800u")
require("shaders/bee_swarm.glsl", "BEE_SWARM_ALTERNATE_TICKS = 600u")
require("shaders/bee_swarm.glsl", "beeBiohazardTargetOffset")
require("shaders/bee_swarm.glsl", "beeHaloTargetOffset")
require("shaders/bee_swarm.glsl", "beeCloudTargetOffset")
require("shaders/bee_swarm.glsl", "beeSwarmTarget")
require("shaders/bee_swarm.glsl", "beeOrbitTarget")

require("shaders/paint.comp", "Exact Fix29 compact hive")
require("shaders/paint.comp", "distanceSquared >= 28 && distanceSquared < 108")
require("shaders/paint.comp", "beeFormationSlotFromOffset(delta)")
require("shaders/paint.comp", "beePackMetadata(cell.aux, center")
require("src/vulkan_renderer.cpp", "? 64u")
require("src/vulkan_renderer.cpp", "if (material == Material::bee_nest) return 12u;")

reset = text("shaders/reset.comp")
if "MAT_LAVA" in reset:
    raise SystemExit("reset.comp: authored scene lava remains; scenes must use magma vents")
require("shaders/reset.comp", "Fix29 compact hive plus a hidden moving swarm mask")
require("shaders/reset.comp", "brickRect")
require("shaders/reset.comp", "brickFrame")
require("shaders/reset.comp", "brickStair")

require("shaders/move.comp", "return beeOrbitTarget(bee.aux, movePc.step);")
require("shaders/move.comp", "boundedSidestep")
if "targetDistance <= 49" in text("shaders/move.comp"):
    raise SystemExit("move.comp: old 49-cell bee deadlock bound remains")
require("shaders/move.comp", "Dense agents circulate around the moving hidden-mask target")

require("include/epoch/sand/shared_state.hpp", "std::atomic_bool fill_region")
require("include/epoch/sand/window.hpp", "bool middle_down")
require("include/epoch/sand/window.hpp", "bool fill")
require("src/window_win32.cpp", "WM_MBUTTONDOWN")
require("src/window_win32.cpp", "case 'F':")
require("src/window_xcb.cpp", "button->detail == 2")
require("src/window_xcb.cpp", "keysym_f")
require("src/app.cpp", "Four-to-one damping")
require("src/app.cpp", "input.middle_down")
require("src/app.cpp", "shared_state.fill_region.store")

require("src/vulkan_renderer.cpp", "void fill_connected_region")
require("src/vulkan_renderer.cpp", "download_scene_cells")
require("src/vulkan_renderer.cpp", "is_block_material(replacement)")
require("src/vulkan_renderer.cpp", "complete aligned bricks")
require("src/vulkan_renderer.cpp", "state.fill_region.exchange")

require("tools/generate_ui_text.py", '"MMB DRAG PAN", "F FILL"')
require("shaders/fullscreen.frag", "keymapBottom = keymapTop + 126u")
require("shaders/fullscreen.frag", "uint leftIds[7]")
require("shaders/fullscreen.frag", "uint rightIds[6]")

print("Fix35 audit passed: hidden weighted swarm states, Fix29 live colony prefab, magma-vent scenes, slow pan, fill, and readable keymap.")
