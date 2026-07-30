#!/usr/bin/env python3
from pathlib import Path
import re

root = Path(__file__).resolve().parents[1]
script_path = root / "tools/fix29.py"
script = script_path.read_text(encoding="utf-8")
start_token = "fullscreen = one(fullscreen, '''    uint gridX = min"
end_token = "fullscreen = one(fullscreen, '        if (local.x == 0"
start = script.find(start_token)
end = script.find(end_token, start)
if start < 0 or end < 0:
    raise RuntimeError("Unable to locate the brittle fullscreen camera mapping patch.")
script_path.write_text(script[:start] + script[end:], encoding="utf-8", newline="\n")

shader_path = root / "shaders/fullscreen.frag"
shader = shader_path.read_text(encoding="utf-8")
pattern = r"uint gridX = min\(renderPc\.gridWidth - 1u,\s*simulationX \* renderPc\.gridWidth / max\(renderPc\.viewportWidth, 1u\)\);\s*uint gridY = min\(renderPc\.gridHeight - 1u,\s*simulationY \* renderPc\.gridHeight / simulationHeight\);"
replacement = """uint gridX = min(renderPc.gridWidth - 1u, renderPc.viewOriginX +
                      simulationX * max(renderPc.viewWidth, 1u) / max(renderPc.viewportWidth, 1u));
    uint gridY = min(renderPc.gridHeight - 1u, renderPc.viewOriginY +
                      simulationY * max(renderPc.viewHeight, 1u) / simulationHeight);"""
shader, count = re.subn(pattern, replacement, shader, count=1, flags=re.S)
if count != 1:
    raise RuntimeError(f"fullscreen camera mapping: expected one regex match, found {count}")
shader_path.write_text(shader, encoding="utf-8", newline="\n")

validator_path = root / "tools/validate_shader_contracts.py"
validator = validator_path.read_text(encoding="utf-8")
old_cpp = '''             "tile_rows", "viewport_left", "viewport_top", "viewport_width", "viewport_height"],'''
new_cpp = '''             "tile_rows", "viewport_left", "viewport_top", "viewport_width", "viewport_height",
             "view_origin_x", "view_origin_y", "view_width", "view_height", "brush_shape"],'''
old_glsl = '''             "sceneCount", "miningMode", "inspectMode", "debugMode", "tileColumns", "tileRows", "viewportLeft", "viewportTop", "viewportWidth", "viewportHeight"],'''
new_glsl = '''             "sceneCount", "miningMode", "inspectMode", "debugMode", "tileColumns", "tileRows", "viewportLeft", "viewportTop", "viewportWidth", "viewportHeight",
             "viewOriginX", "viewOriginY", "viewWidth", "viewHeight", "brushShape"],'''
if validator.count(old_cpp) != 1 or validator.count(old_glsl) != 1:
    raise RuntimeError("Unable to update the inherited RenderPush validator contract.")
validator = validator.replace(old_cpp, new_cpp, 1).replace(old_glsl, new_glsl, 1)
validator_path.write_text(validator, encoding="utf-8", newline="\n")
print("Fix29 camera mapping and validator contracts prepared.")
