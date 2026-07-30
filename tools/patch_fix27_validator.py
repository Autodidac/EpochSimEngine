#!/usr/bin/env python3
from pathlib import Path

path = Path('tools/validate_shader_contracts.py')
text = path.read_text(encoding='utf-8')
replacements = {
    '"3,cardMaterial"': '"3, cardMaterial"',
    '"2,cardMaterial"': '"2, cardMaterial"',
    '"2,60u"': '"2, 60u"',
    '"2,61u"': '"2, 61u"',
    'for token in ("previouslyDense", "previous.occupancy >= TILE_STABILITY_OCCUPANCY",\n                   "structuralTile && previouslyDense && structural < TILE_COLLAPSE_OCCUPANCY"):\n':
        'for token in ("previouslyDense", "previous.occupancy >= TILE_STABILITY_OCCUPANCY",\n                   "dominantCount < TILE_MIN_COHESIVE_CELLS",\n                   "structuralTile ? dominantCount"):\n',
}
for old, new in replacements.items():
    count = text.count(old)
    if count != 1:
        raise SystemExit(f'Fix27 validator patch expected one {old!r}, found {count}')
    text = text.replace(old, new, 1)
path.write_text(text, encoding='utf-8', newline='\n')
print('Global validator migrated to Fix27 sidebar and cohesion contracts.')
