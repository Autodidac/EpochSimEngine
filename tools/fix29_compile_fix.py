#!/usr/bin/env python3
from pathlib import Path

root = Path(__file__).resolve().parents[1]

fullscreen_path = root / "shaders/fullscreen.frag"
fullscreen = fullscreen_path.read_text(encoding="utf-8")
replacements = {
    "uint half = contentWidth / 2u;": "uint halfWidth = contentWidth / 2u;",
    "contentLeft + half / 2u - 8u": "contentLeft + halfWidth / 2u - 8u",
    "contentLeft + half + 39u": "contentLeft + halfWidth + 39u",
    "contentLeft + half + half / 2u - 8u": "contentLeft + halfWidth + halfWidth / 2u - 8u",
    "contentLeft + half - 25u": "contentLeft + halfWidth - 25u",
    "contentLeft + half + 13u": "contentLeft + halfWidth + 13u",
}
for old, new in replacements.items():
    if old not in fullscreen:
        raise RuntimeError(f"fullscreen compiler fix missing expected token: {old}")
    fullscreen = fullscreen.replace(old, new)
fullscreen_path.write_text(fullscreen, encoding="utf-8", newline="\n")

move_path = root / "shaders/move.comp"
move = move_path.read_text(encoding="utf-8")
old = "int tangentScore = dot(delta, tangent) * 8;"
new = "int tangentScore = (delta.x * tangent.x + delta.y * tangent.y) * 8;"
if move.count(old) != 1:
    raise RuntimeError(f"move compiler fix expected one integer dot expression, found {move.count(old)}")
move_path.write_text(move.replace(old, new, 1), encoding="utf-8", newline="\n")
print("Fix29 GLSL compiler diagnostics corrected.")
