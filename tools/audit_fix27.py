#!/usr/bin/env python3
from pathlib import Path

root = Path(__file__).resolve().parents[1]
files = {
    "scene_hpp": (root / "include/epoch/sand/scene_image.hpp").read_text(encoding="utf-8"),
    "scene_cpp": (root / "src/scene_image.cpp").read_text(encoding="utf-8"),
    "shared": (root / "include/epoch/sand/shared_state.hpp").read_text(encoding="utf-8"),
    "window": (root / "include/epoch/sand/window.hpp").read_text(encoding="utf-8"),
    "win": (root / "src/window_win32.cpp").read_text(encoding="utf-8"),
    "xcb": (root / "src/window_xcb.cpp").read_text(encoding="utf-8"),
    "app": (root / "src/app.cpp").read_text(encoding="utf-8"),
    "layout": (root / "include/epoch/sand/ui_layout.hpp").read_text(encoding="utf-8"),
    "renderer": (root / "src/vulkan_renderer.cpp").read_text(encoding="utf-8"),
    "tiles_h": (root / "shaders/tiles.glsl").read_text(encoding="utf-8"),
    "tiles": (root / "shaders/tiles.comp").read_text(encoding="utf-8"),
    "chem": (root / "shaders/chemistry.comp").read_text(encoding="utf-8"),
    "frag": (root / "shaders/fullscreen.frag").read_text(encoding="utf-8"),
    "generator": (root / "tools/generate_ui_text.py").read_text(encoding="utf-8"),
    "cmake": (root / "CMakeLists.txt").read_text(encoding="utf-8"),
    "test": (root / "tests/scene_image_contract.cpp").read_text(encoding="utf-8"),
}
required = {
    "scene_hpp": ["struct SceneCell", "load_scene_ppm", "save_scene_ppm", "write_scene_material_key"],
    "scene_cpp": ["P6", "minimum_cohesive_cells = 32u", "full_strength_cells = 52u",
                  "material_key.ppm", "nearest material"],
    "shared": ["save_scene_image", "load_scene_image"],
    "window": ["bool save_scene{}", "bool load_scene{}"],
    "win": ["case VK_F5", "case VK_F9", ".save_scene =", ".load_scene ="],
    "xcb": ["keysym_f5", "keysym_f9", ".save_scene =", ".load_scene ="],
    "app": ["layout.save_scene", "layout.load_scene", "layout.eraser", "Material::empty"],
    "layout": ["eraser_height = 24u", "keymap_height = 100u", "eraser{}", "keymap{}",
               "material_card ="],
    "renderer": ["Buffer scene_staging_buffer{}", "load_scene_ppm", "save_scene_ppm",
                 "pending_scene_export", "scene_image_exists", "Loaded moddable scene image"],
    "tiles_h": ["TILE_MIN_COHESIVE_CELLS = 32u"],
    "tiles": ["dominantCount < TILE_MIN_COHESIVE_CELLS", "structuralTile ? dominantCount"],
    "chem": ["minorityStructural", "limitedHealth", "TILE_MIN_COHESIVE_CELLS"],
    "frag": ["eraserTop", "keymapTop", "renderPc.selectedMaterial == MAT_EMPTY",
             "uint leftIds[6]", "uint rightIds[5]"],
    "generator": ["SAVE", "LOAD", "ERASER", "KEYMAP", "F5 SAVE PPM", "F9 LOAD PPM"],
    "cmake": ["src/scene_image.cpp", "epoch_sand_scene_image_contract", "scenes/README.txt"],
    "test": ["31 stone cells", "40 glass cells", "glass_health < 64u"],
}
errors = []
for name, tokens in required.items():
    for token in tokens:
        if token not in files[name]:
            errors.append(f"{name}: missing {token!r}")

forbidden = {
    "tiles": ["previouslyDense && structural < TILE_COLLAPSE_OCCUPANCY"],
    "frag": ["uint controlsStart = renderPc.windowHeight - renderPc.paletteHeight"],
    "scene_cpp": ["aux_wet"],
}
for name, tokens in forbidden.items():
    for token in tokens:
        if token in files[name]:
            errors.append(f"{name}: forbidden legacy contract remains: {token!r}")

if errors:
    raise SystemExit("\n".join(errors))
print("Fix27 audit passed: PPM scene I/O, 32-pixel cohesion, reduced durability, eraser, and keymap.")
