#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def text(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


checks = {
    "shared camera zoom": ("include/epoch/sand/shared_state.hpp", "camera_zoom{1}"),
    "shared camera center": ("include/epoch/sand/shared_state.hpp", "camera_center_x{320}"),
    "shared brush shape": ("include/epoch/sand/shared_state.hpp", "brush_shape{0}"),
    "cursor editor layout": ("include/epoch/sand/ui_layout.hpp", "cursor_editor_height = 92u"),
    "mouse-centered zoom": ("src/app.cpp", "zoom_at_pointer"),
    "wheel zoom": ("src/app.cpp", "input.wheel_delta != 0 && over_simulation"),
    "camera-aware cursor": ("src/vulkan_renderer.cpp", "view.origin_x +"),
    "camera push": ("src/vulkan_renderer.cpp", ".view_origin_x = view.origin_x"),
    "packed brush shape": ("src/vulkan_renderer.cpp", "shape << 16u"),
    "paint circle": ("shaders/paint.comp", "brushShape == 0u"),
    "paint square": ("shaders/paint.comp", "brushShape == 1u"),
    "paint horizontal": ("shaders/paint.comp", "brushShape == 2u"),
    "paint vertical": ("shaders/paint.comp", "abs(delta.x) <= 1"),
    "render camera": ("shaders/fullscreen.frag", "renderPc.viewOriginX"),
    "render cursor shapes": ("shaders/fullscreen.frag", "renderPc.brushShape == 2u"),
    "cursor panel": ("shaders/fullscreen.frag", "uint cursorBottom = cursorTop + 92u"),
    "bounded authored swarm": ("shaders/reset.comp", "(swarm & 7u) == 0u"),
    "swarm marker": ("shaders/reset.comp", "AUX_PLANT_STEM | AUX_BEE_FED"),
    "orbit controller": ("shaders/move.comp", "AUX_BEE_SWARM"),
    "tangent orbit": ("shaders/move.comp", "ivec2 tangent"),
    "bounded hive shell": ("shaders/chemistry.comp", "countWithin(p, MAT_BEE_NEST, 5) < 36u"),
    "bounded worker spawn": ("shaders/chemistry.comp", "countWithin(p, MAT_BEE, 6) < 4u"),
    "slow honey use": ("shaders/chemistry.comp", "randomValue & 8191u"),
    "wheel label": ("tools/generate_ui_text.py", '"WHEEL ZOOM"'),
    "hive label": ("tools/generate_ui_text.py", '"HIVE CELLS"'),
}

failed: list[str] = []
for label, (path, token) in checks.items():
    if token not in text(path):
        failed.append(f"{label}: missing {token!r} in {path}")

forbidden = {
    "old wheel brush": ("tools/generate_ui_text.py", '"WHEEL BRUSH"'),
    "old dense swarm": ("shaders/reset.comp", "(swarm & 3u) != 0u"),
    "old runaway hive": ("shaders/chemistry.comp", "queenDistanceSquared <= 21 && (randomValue & 7u) != 0u"),
}
for label, (path, token) in forbidden.items():
    if token in text(path):
        failed.append(f"{label}: forbidden token remains in {path}: {token!r}")

if failed:
    print("Fix29 audit failed:")
    for item in failed:
        print(f" - {item}")
    raise SystemExit(1)

print("Fix29 camera, cursor, and bee contracts valid.")
