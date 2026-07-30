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

layout_path = Path('include/epoch/sand/ui_layout.hpp')
layout = layout_path.read_text(encoding='utf-8')
old_layout = 'auto tc=(std::max)(1u,(grid_width+7u)/8u); auto tr=(std::max)(1u,(grid_height+7u)/8u);'
new_layout = 'auto tc=(std::max)(1u,(grid_width+cells_per_tile-1u)/cells_per_tile); auto tr=(std::max)(1u,(grid_height+cells_per_tile-1u)/cells_per_tile);'
count = layout.count(old_layout)
if count != 1:
    raise SystemExit(f'Fix26 layout compile patch expected one match, found {count}')
layout_path.write_text(layout.replace(old_layout, new_layout, 1), encoding='utf-8', newline='\n')

app_path = Path('src/app.cpp')
app = app_path.read_text(encoding='utf-8')
old_app = '        const bool character_scene = scene_has_character(scene);\n'
count = app.count(old_app)
if count != 1:
    raise SystemExit(f'Fix26 app compile patch expected one match, found {count}')
app_path.write_text(app.replace(old_app, '', 1), encoding='utf-8', newline='\n')

test_path = Path('tests/material_contract.cpp')
test = test_path.read_text(encoding='utf-8')
old_test = '''    const auto compact = epoch::sand::ui::make_layout(480u, 320u);
    if (compact.simulation.size.y <= 0.0f) return 7;
    if (compact.previous_scene.size.x != 0.0f || compact.next_scene.size.x != 0.0f ||
        compact.reset_scene.size.x != 0.0f) return 8;
    if (compact.mode_toggle.position.x < 0.0f || compact.debug_toggle.position.x < 0.0f) return 9;
'''
new_test = '''    const auto compact = epoch::sand::ui::make_layout(480u, 320u);
    if (compact.simulation.size.y <= 0.0f || compact.status.size.x < 300.0f) return 7;
    if (compact.previous_scene.size.x <= 0.0f || compact.next_scene.size.x <= 0.0f ||
        compact.reset_scene.size.x <= 0.0f) return 8;
    if (compact.mode_toggle.position.x < compact.simulation.size.x ||
        compact.debug_toggle.position.x < compact.simulation.size.x ||
        compact.material_card.position.x < compact.simulation.size.x) return 9;
'''
count = test.count(old_test)
if count != 1:
    raise SystemExit(f'Fix26 material-contract patch expected one match, found {count}')
test_path.write_text(test.replace(old_test, new_test, 1), encoding='utf-8', newline='\n')
print('Fix26 GLSL, C++, and sidebar-test contracts patched.')
