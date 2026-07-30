#!/usr/bin/env python3
from pathlib import Path

move_path = Path('shaders/move.comp')
move = move_path.read_text(encoding='utf-8')
old = 'isReconstructableMaterial(cell.material)'
new = '''(isBlockCapable(cell.material) || cell.material == MAT_DIRT ||
                             cell.material == MAT_SAND || cell.material == MAT_SILT ||
                             cell.material == MAT_SALT || cell.material == MAT_ICE)'''
count = move.count(old)
if count != 1:
    raise SystemExit(f'Fix26 movement compile patch expected one match, found {count}')
move_path.write_text(move.replace(old, new, 1), encoding='utf-8', newline='\n')

frag_path = Path('shaders/fullscreen.frag')
frag = frag_path.read_text(encoding='utf-8')
frag = frag.replace('const uint bx[5]=uint[5](', 'uint bx[5]=uint[5](', 1)
frag = frag.replace('const uint bw[5]=uint[5](', 'uint bw[5]=uint[5](', 1)
frag_path.write_text(frag, encoding='utf-8', newline='\n')
print('Fix26 GLSL compile contracts patched.')
