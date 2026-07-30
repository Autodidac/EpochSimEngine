#!/usr/bin/env python3
from pathlib import Path

path = Path('tools/apply_fix26_systems.py')
text = path.read_text(encoding='utf-8')
old = '''frag=one(frag,"            cell.material == MAT_PLANT_STEM || cell.material == MAT_HYDROGEN ||\\n","            cell.material == MAT_HYDROGEN ||\\n","stem danger")'''
new = '''frag=rx(frag,r"\\s*cell\\.material == MAT_PLANT_STEM \\|\\|\\s*cell\\.material == MAT_HYDROGEN \\|\\|\\n","            cell.material == MAT_HYDROGEN ||\\n","stem danger")'''
count = text.count(old)
if count != 1:
    raise SystemExit(f'Fix26 apply patch expected one match, found {count}')
path.write_text(text.replace(old, new, 1), encoding='utf-8', newline='\n')
print('Fix26 apply matcher patched.')
