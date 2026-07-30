#!/usr/bin/env python3
import re
from pathlib import Path

path = Path('tools/apply_fix25_motion_ecology.py')
text = path.read_text(encoding='utf-8')

queen_pattern = re.compile(
    r'("""\s*\} else if \(any\(notEqual\(queenOffset, ivec2\(0\)\)\) && queenDistanceSquared <= 6 &&\n)'
    r'[ \t]+(\(hasNeighbor\(p, MAT_HONEY\) \|\| hasNeighbor\(p, MAT_POLLEN\)\) &&\n)'
    r'[ \t]+(\(randomValue & 4095u\) == 0u\) \{\n""")'
)
text, queen_count = queen_pattern.subn(
    lambda match: match.group(1) + '                           ' + match.group(2) +
                  '                           ' + match.group(3),
    text,
    count=1,
)
if queen_count != 1:
    raise SystemExit(f'Fix25 queen source contract patch found {queen_count} matches')

stem_pattern = re.compile(
    r'"""\s*if \(below\.material == MAT_PLANT_STEM &&\n'
    r'[ \t]+below\.age > 180u && hasFreshMoistureRadius\(p, 3\) && light\[index\] > 145u\) \{\n'
    r'[ \t]+uint stage = stateValue\(below\);\n'
    r'[ \t]+if \(stage < 3u\) \{\n'
    r'[ \t]+result = makeCell\(MAT_PLANT_STEM\);\n'
    r'[ \t]+setStateValue\(result, stage \+ 1u\);\n'
    r'[ \t]+\} else \{\n'
    r'[ \t]+result = makeCell\(MAT_FLOWER\);\n'
    r'[ \t]+\}\n'
    r'[ \t]+\}\n"""'
)
stem_replacement = '''"""                if (below.material == MAT_PLANT_STEM &&
                    below.age > 180u && hasFreshMoistureRadius(p, 3) && light[index] > 145u) {
                    uint stage = stateValue(below);
                    if (stage < 3u) {
                        result = makeCell(MAT_PLANT_STEM);
                        setStateValue(result, stage + 1u);
                    } else {
                        result = makeCell(MAT_FLOWER);
                    }
                }
"""'''
text, stem_count = stem_pattern.subn(stem_replacement, text, count=1)
if stem_count != 1:
    raise SystemExit(f'Fix25 stem source contract patch found {stem_count} matches')

path.write_text(text, encoding='utf-8', newline='\n')
print('Fix25 apply-script source contracts patched.')
