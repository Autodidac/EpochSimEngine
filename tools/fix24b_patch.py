#!/usr/bin/env python3
from pathlib import Path

path = Path('tools/apply_sandhybrid_fix24.py')
text = path.read_text(encoding='utf-8')
old_head = '''catalog = re.sub(
    r"GROUPS = \\[.*?\\]\\nBLOCK_MATERIALS",
'''
new_head = '''catalog, group_replacements = re.subn(
    r"GROUPS = \\[.*?\\]\\s*BLOCK_MATERIALS",
'''
if text.count(old_head) != 1:
    raise SystemExit(f'expected one old group-regeneration head, found {text.count(old_head)}')
text = text.replace(old_head, new_head, 1)
old_tail = '''    count=1,
    flags=re.S,
)
write(catalog_path, catalog)
'''
new_tail = '''    count=1,
    flags=re.S,
)
if group_replacements != 1:
    raise RuntimeError(f'material palette group regeneration failed: {group_replacements}')
write(catalog_path, catalog)
'''
head_index = text.index(new_head)
tail_index = text.find(old_tail, head_index)
if tail_index < 0:
    raise SystemExit('group-regeneration tail not found')
text = text[:tail_index] + new_tail + text[tail_index + len(old_tail):]
path.write_text(text, encoding='utf-8', newline='\n')
print('Fix24b palette-regeneration patch applied.')
