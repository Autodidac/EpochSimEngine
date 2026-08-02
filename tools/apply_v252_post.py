#!/usr/bin/env python3
from pathlib import Path

root = Path(__file__).resolve().parents[1]
path = root / "src/scene_image.cpp"
text = path.read_text(encoding="utf-8")
text = text.replace("sizeof(Rgb))", "sizeof(Rgb8))")
if "sizeof(Rgb)" in text:
    raise SystemExit("legacy scene RGB type remains")
path.write_text(text, encoding="utf-8")
Path(__file__).unlink(missing_ok=True)
