#!/usr/bin/env python3
from __future__ import annotations

import re
import runpy
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
app_path = ROOT / "src/app.cpp"
app = app_path.read_text(encoding="utf-8")
replacement = """        const bool over_simulation = epochengine::gui_lib::contains(layout.simulation, pointer);
        const bool mining = shared_state.mining_mode.load(std::memory_order_relaxed);
        const bool character_scene = scene_has_character(scene);
        const bool inspecting = input.inspect_material;
        const bool paint_active = over_simulation && !character_scene && !mining && !inspecting;
        shared_state.primary_down.store(input.primary_down && paint_active,
                                         std::memory_order_relaxed);
        shared_state.secondary_down.store(input.secondary_down && paint_active,
                                           std::memory_order_relaxed);
        // Character scenes always retain mining/shooting. The mode toggle cannot
        // accidentally route player clicks back into world painting.
        const bool tool_active = over_simulation && (character_scene || mining) && !inspecting;
"""
if replacement not in app:
    pattern = re.compile(
        r"        const bool over_simulation = epochengine::gui_lib::contains\(layout\.simulation, pointer\);\n"
        r"        const bool mining = shared_state\.mining_mode\.load\(std::memory_order_relaxed\);\n"
        r"        const bool inspecting = input\.inspect_material;\n"
        r"        shared_state\.primary_down\.store\(.*?\n"
        r"        const bool tool_active = over_simulation && mining && !inspecting;\n",
        re.S,
    )
    app, count = pattern.subn(replacement, app, count=1)
    if count != 1:
        raise RuntimeError(f"Unable to patch current app tool routing: {count} matches")
    app_path.write_text(app, encoding="utf-8")

runpy.run_path(str(ROOT / "tools/apply_screenshot_pass.py"), run_name="__main__")
