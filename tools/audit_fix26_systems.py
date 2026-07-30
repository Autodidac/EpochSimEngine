#!/usr/bin/env python3
from pathlib import Path

root = Path(__file__).resolve().parents[1]
files = {
    "app": (root / "src/app.cpp").read_text(encoding="utf-8"),
    "win": (root / "src/window_win32.cpp").read_text(encoding="utf-8"),
    "xcb": (root / "src/window_xcb.cpp").read_text(encoding="utf-8"),
    "layout": (root / "include/epoch/sand/ui_layout.hpp").read_text(encoding="utf-8"),
    "renderer": (root / "src/vulkan_renderer.cpp").read_text(encoding="utf-8"),
    "move": (root / "shaders/move.comp").read_text(encoding="utf-8"),
    "chem": (root / "shaders/chemistry.comp").read_text(encoding="utf-8"),
    "frag": (root / "shaders/fullscreen.frag").read_text(encoding="utf-8"),
    "reset": (root / "shaders/reset.comp").read_text(encoding="utf-8"),
    "materials": (root / "shaders/materials.glsl").read_text(encoding="utf-8"),
    "generator": (root / "tools/generate_ui_text.py").read_text(encoding="utf-8"),
}
required = {
    "win": [
        "case VK_SPACE:\n                self->jump = true",
        "case 'P':",
        "impl_->jump = key_down(VK_SPACE)",
    ],
    "xcb": ["keysym_p", "impl_->jump = true", "impl_->toggle_pause = true"],
    "app": ["paint_active = over_simulation && !mining", "tool_active = over_simulation && mining"],
    "layout": ["preferred_sidebar_width = 384u", "group_tab_rect", "palette_item_rect", ".material_card"],
    "renderer": ["std::array<std::int32_t, 9>", "layout.status.size.x"],
    "move": ["sourceCovered", "secondDrop", "hiveSignal", "orbit", "stabilizedRegion", "sourceWet"],
    "chem": [
        "countWithin(p, MAT_BEE, 6) < 12u",
        "setStateValue(result, 480u)",
        "randomValue & 2047u",
        "source.material == MAT_WASTE &&",
        "machineConsumption",
        "MAT_ALUMINUM_SHAVINGS) return 1",
        "freshContact",
    ],
    "frag": [
        "sidebarWidth",
        "materialPixel(pixel",
        "3,cardMaterial",
        "cardPixel(pixel",
        "2,cardMaterial",
        "2,60u",
        "2,61u",
        "1,64u",
    ],
    "reset": ["tankLeft=22", "trenchY=groundY-5", "ivec2 queen", "MAT_CARBON_DIOXIDE", "MAT_HYDROGEN"],
    "materials": ["wetDarken"],
    "generator": ["SPACE JUMP", "P PAUSE", "LMB USE", "RMB DROP ERASE", "ALT CELL CARD"],
}
errors = []
for name, tokens in required.items():
    for token in tokens:
        if token not in files[name]:
            errors.append(f"{name}: missing {token}")

forbidden = {
    "win": ["case VK_SPACE:\n                self->toggle_pause = true", "impl_->jump = impl_->move_up"],
    "app": ["(character_scene || mining)"],
    "frag": ["uint controlsStart = renderPc.windowHeight - renderPc.paletteHeight", "Exact Alt inspection card"],
}
for name, tokens in forbidden.items():
    for token in tokens:
        if token in files[name]:
            errors.append(f"{name}: forbidden legacy contract remains: {token}")

if errors:
    raise SystemExit("\n".join(errors))
print("Fix26 systems audit passed: controls, build mode, sidebar/card UI, water, soil, bricks, bees, insects, factory, ecosystem.")
