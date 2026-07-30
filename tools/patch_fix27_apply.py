#!/usr/bin/env python3
from pathlib import Path

apply_path = Path('tools/apply_fix27.py')
text = apply_path.read_text(encoding='utf-8')
old = '''tiles = one(tiles,
    "    bool previouslyDense = tileHas(previous, TILE_STRUCTURAL) &&\\n                            previous.occupancy >= TILE_STABILITY_OCCUPANCY;\\n    bool denseStructural = structural >= TILE_STABILITY_OCCUPANCY || previouslyDense;\\n    bool damaged = structuralTile && denseStructural &&\\n                   (structural < TILE_CELL_COUNT ||\\n                    (structural > 0u && healthSum / structural < 240u));\\n",
    "    bool previouslyDense = tileHas(previous, TILE_STRUCTURAL) &&\\n                            previous.occupancy >= TILE_STABILITY_OCCUPANCY;\\n    bool reducedDurability = structuralTile && dominantCount < TILE_STABILITY_OCCUPANCY;\\n    bool damaged = structuralTile &&\\n                   (reducedDurability || dominantCount < TILE_CELL_COUNT ||\\n                    (structural > 0u && healthSum / structural < 240u));\\n",
    "tile damage rule")
'''
new = '''tiles = rx(tiles,
    r"    bool previouslyDense = tileHas\\(previous, TILE_STRUCTURAL\\) &&\\s*previous\\.occupancy >= TILE_STABILITY_OCCUPANCY;\\s*bool denseStructural = structural >= TILE_STABILITY_OCCUPANCY \\|\\| previouslyDense;\\s*bool damaged = structuralTile && denseStructural &&\\s*\\(structural < TILE_CELL_COUNT \\|\\|\\s*\\(structural > 0u && healthSum / structural < 240u\\)\\);",
    "    bool previouslyDense = tileHas(previous, TILE_STRUCTURAL) &&\\n                            previous.occupancy >= TILE_STABILITY_OCCUPANCY;\\n    bool reducedDurability = structuralTile && dominantCount < TILE_STABILITY_OCCUPANCY;\\n    bool damaged = structuralTile &&\\n                   (reducedDurability || dominantCount < TILE_CELL_COUNT ||\\n                    (structural > 0u && healthSum / structural < 240u));",
    "tile damage rule")
'''
count = text.count(old)
if count != 1:
    raise SystemExit(f'Fix27 matcher patch expected one block, found {count}')
apply_path.write_text(text.replace(old, new, 1), encoding='utf-8', newline='\n')

validator_path = Path('tools/validate_shader_contracts.py')
validator = validator_path.read_text(encoding='utf-8')
replacements = {
    '"3,cardMaterial"': '"3, cardMaterial"',
    '"2,cardMaterial"': '"2, cardMaterial"',
    '"2,60u"': '"2, 60u"',
    '"2,61u"': '"2, 61u"',
    'for token in ("previouslyDense", "previous.occupancy >= TILE_STABILITY_OCCUPANCY",\n                   "structuralTile && previouslyDense && structural < TILE_COLLAPSE_OCCUPANCY"):\n':
        'for token in ("previouslyDense", "previous.occupancy >= TILE_STABILITY_OCCUPANCY",\n                   "dominantCount < TILE_MIN_COHESIVE_CELLS",\n                   "structuralTile ? dominantCount"):\n',
}
for previous, replacement in replacements.items():
    matches = validator.count(previous)
    if matches != 1:
        raise SystemExit(f'Fix27 validator patch expected one {previous!r}, found {matches}')
    validator = validator.replace(previous, replacement, 1)
validator_path.write_text(validator, encoding='utf-8', newline='\n')
print('Fix27 matchers and global validator patched.')
