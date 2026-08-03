from pathlib import Path

path = Path("tools/validate_shader_contracts.py")
text = path.read_text(encoding="utf-8")
replacements = {
    '"status_height = 126u"': '"status_height = 208u"',
    '"group_tabs_height = 112u"': '"group_tabs_height = 96u"',
    '"palette_items_height = 136u"': '"palette_items_height = 124u"',
}
for old, new in replacements.items():
    if old not in text:
        raise SystemExit(f"validator dimension anchor missing: {old}")
    text = text.replace(old, new, 1)
path.write_text(text, encoding="utf-8")
