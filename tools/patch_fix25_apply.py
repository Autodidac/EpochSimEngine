#!/usr/bin/env python3
from pathlib import Path

path = Path('tools/apply_fix25_motion_ecology.py')
text = path.read_text(encoding='utf-8')

replacements = [
    (
        '''"""                } else if (any(notEqual(queenOffset, ivec2(0))) && queenDistanceSquared <= 6 &&
                             (hasNeighbor(p, MAT_HONEY) || hasNeighbor(p, MAT_POLLEN)) &&
                             (randomValue & 4095u) == 0u) {
"""''',
        '''"""                } else if (any(notEqual(queenOffset, ivec2(0))) && queenDistanceSquared <= 6 &&
                            (hasNeighbor(p, MAT_HONEY) || hasNeighbor(p, MAT_POLLEN)) &&
                            (randomValue & 4095u) == 0u) {
"""''',
    ),
    (
        '''"""                if (below.material == MAT_PLANT_STEM &&
                     below.age > 180u && hasFreshMoistureRadius(p, 3) && light[index] > 145u) {
                     uint stage = stateValue(below);
                     if (stage < 3u) {
                         result = makeCell(MAT_PLANT_STEM);
                         setStateValue(result, stage + 1u);
                     } else {
                         result = makeCell(MAT_FLOWER);
                     }
                 }
"""''',
        '''"""                if (below.material == MAT_PLANT_STEM &&
                    below.age > 180u && hasFreshMoistureRadius(p, 3) && light[index] > 145u) {
                    uint stage = stateValue(below);
                    if (stage < 3u) {
                        result = makeCell(MAT_PLANT_STEM);
                        setStateValue(result, stage + 1u);
                    } else {
                        result = makeCell(MAT_FLOWER);
                    }
                }
"""''',
    ),
]

for old, new in replacements:
    count = text.count(old)
    if count != 1:
        raise SystemExit(f'Fix25 apply-script patch expected one match, found {count}')
    text = text.replace(old, new, 1)

path.write_text(text, encoding='utf-8', newline='\n')
print('Fix25 apply-script source contracts patched.')
