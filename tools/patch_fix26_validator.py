#!/usr/bin/env python3
from pathlib import Path

path = Path('tools/validate_shader_contracts.py')
text = path.read_text(encoding='utf-8')
old = '''    for token in ("status_height = 72u", "group_tabs_height = 48u", "palette_items_height = 76u"):
        if token not in ui_layout:
            errors.append(f"large UI layout contract missing {token!r}")
    for token in ("int[5](78, 78, 104, 136, 104)", "ivec2(12, 15), titleScale, 0u", "hudTop + 112u"):
        if token not in fullscreen:
            errors.append(f"large UI shader contract missing {token!r}")
'''
new = '''    for token in (
        "preferred_sidebar_width = 384u",
        "status_height = 126u",
        "group_tabs_height = 112u",
        "palette_items_height = 136u",
        "material_card",
    ):
        if token not in ui_layout:
            errors.append(f"compact sidebar layout contract missing {token!r}")
    for token in (
        "sidebarWidth",
        "groupMaterialCount(renderPc.selectedGroup)",
        "materialPixel(pixel",
        "3,cardMaterial",
        "cardPixel(pixel",
        "2,cardMaterial",
        "2,60u",
        "2,61u",
    ):
        if token not in fullscreen:
            errors.append(f"compact sidebar shader contract missing {token!r}")
'''
count = text.count(old)
if count != 1:
    raise SystemExit(f'Fix26 validator UI patch expected one match, found {count}')
text = text.replace(old, new, 1)
old_arch = '''        "renderer": (renderer, ("renderPc.inspectMode", "renderPc.debugMode", "gasPresentation", "TILE_CANDIDATE")),
'''
new_arch = '''        "renderer": (renderer, ("renderPc.inspectMode", "renderPc.debugMode", "gasPresentation", "sidebarWidth", "cardPixel")),
'''
count = text.count(old_arch)
if count != 1:
    raise SystemExit(f'Fix26 validator renderer patch expected one match, found {count}')
text = text.replace(old_arch, new_arch, 1)
path.write_text(text, encoding='utf-8', newline='\n')
print('Fix26 shader validator updated for compact sidebar contracts.')
