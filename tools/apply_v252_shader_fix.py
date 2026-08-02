#!/usr/bin/env python3
from pathlib import Path

root = Path(__file__).resolve().parents[1]
path = root / "shaders/reset.comp"
text = path.read_text(encoding="utf-8")
old = "ivec2 patch = worldPosition / 64;"
new = "ivec2 geologyPatch = worldPosition / 64;"
if old in text:
    text = text.replace(old, new, 1)
elif new not in text:
    raise SystemExit("reset.comp geology patch declaration not found")
text = text.replace("patch.x", "geologyPatch.x").replace("patch.y", "geologyPatch.y")
if "ivec2 patch" in text:
    raise SystemExit("reserved GLSL patch identifier remains")
path.write_text(text, encoding="utf-8")
Path(__file__).unlink(missing_ok=True)
