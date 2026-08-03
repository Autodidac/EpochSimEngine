from pathlib import Path

shader = Path("shaders/fullscreen.frag")
text = shader.read_text(encoding="utf-8")
replacements = {
    "const uint palettePanelHeight = 136u;": "const uint palettePanelHeight = 124u;",
    "uint keymapBottom = keymapTop + 126u;": "uint keymapBottom = keymapTop + 98u;",
    "uint cursorBottom = cursorTop + 120u;": "uint cursorBottom = cursorTop + 112u;",
    "uint controlTop = cursorTop + 89u;": "uint controlTop = cursorTop + 85u;",
    "y < controlTop + 26u": "y < controlTop + 24u",
    "borderPixel(x, y, left, controlTop, right, controlTop + 26u)":
        "borderPixel(x, y, left, controlTop, right, controlTop + 24u)",
}
for old, new in replacements.items():
    if old not in text:
        raise SystemExit(f"shader layout anchor missing: {old}")
    text = text.replace(old, new, 1)
shader.write_text(text, encoding="utf-8")

validator = Path("tools/validate_shader_contracts.py")
text = validator.read_text(encoding="utf-8")
anchor = '    for token in ("mediumCell", "stateEdge", "utilityLabels[3] = uint[3](67u, 159u, 108u)",\n                  "debugScale"):\n'
replacement = '    for token in ("mediumCell", "stateEdge", "utilityLabels[3] = uint[3](67u, 159u, 108u)",\n                  "debugScale", "palettePanelHeight = 124u",\n                  "keymapBottom = keymapTop + 98u",\n                  "cursorBottom = cursorTop + 112u",\n                  "controlTop = cursorTop + 85u"):\n'
if anchor not in text:
    raise SystemExit("validator shader-layout anchor missing")
validator.write_text(text.replace(anchor, replacement, 1), encoding="utf-8")
