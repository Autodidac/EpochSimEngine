#!/usr/bin/env python3
from pathlib import Path

root = Path(__file__).resolve().parents[1]
path = root / "tools/validate_shader_contracts.py"
text = path.read_text(encoding="utf-8")

text = text.replace(
    'for retired in ("MAT_METAL", "MAT_GOLD_ORE", "MAT_IRON_ORE", "MAT_ALLY_BOT", "MAT_ENEMY_BOT", "MAT_BOT_FABRICATOR"): ',
    'for retired in ("MAT_METAL", "MAT_GOLD_ORE", "MAT_ALLY_BOT", "MAT_ENEMY_BOT", "MAT_BOT_FABRICATOR"): ',
)
text = text.replace(
    'if "MAT_GOLD_ORE" in reset or "MAT_IRON_ORE" in reset or "MAT_GOLD_ORE" in actor or "MAT_IRON_ORE" in actor:\n        errors.append("ore blocks remain in authored scenes or player mining")',
    'if "MAT_GOLD_ORE" in reset or "MAT_GOLD_ORE" in actor:\n        errors.append("retired Gold Ore blocks remain in authored scenes or player mining")',
)
text = text.replace(
    'legacy_tokens = ("MAT_BEE_NEST", "bee_nest", "Bee nest", "MAT_IRON_SHAVINGS", "iron_shavings", "Iron shavings")',
    'legacy_tokens = ("MAT_BEE_" + "NEST", "bee_" + "nest", "Bee " + "nest", "MAT_IRON_" + "SHAVINGS", "iron_" + "shavings", "Iron " + "shavings")',
)
text = text.replace(
    '            source_text = source_path.read_text(encoding="utf-8")\n            for token in legacy_tokens:',
    '            if source_path.resolve() == Path(__file__).resolve():\n                continue\n            source_text = source_path.read_text(encoding="utf-8")\n            for token in legacy_tokens:',
)

for obsolete in (
    '"MAT_IRON_ORE" in reset or ',
    ' or "MAT_IRON_ORE" in actor',
    '"MAT_IRON_ORE", ',
):
    if obsolete in text:
        raise SystemExit(f"obsolete Iron Ore rejection remains: {obsolete}")

path.write_text(text, encoding="utf-8")
Path(__file__).unlink(missing_ok=True)
