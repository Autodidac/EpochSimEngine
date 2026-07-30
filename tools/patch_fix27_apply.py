#!/usr/bin/env python3
from pathlib import Path

path = Path('tools/apply_fix27.py')
text = path.read_text(encoding='utf-8')
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
path.write_text(text.replace(old, new, 1), encoding='utf-8', newline='\n')
print('Fix27 tile matcher patched.')
