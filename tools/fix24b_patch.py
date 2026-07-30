#!/usr/bin/env python3
from pathlib import Path

path = Path('tools/apply_sandhybrid_fix24.py')
text = path.read_text(encoding='utf-8')

# The source catalog has a blank line between GROUPS and BLOCK_MATERIALS.
# Replace exactly one group table and fail rather than silently leaving stale IDs.
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

# Teach the static validator about the larger UI, the tile-aligned viewport push
# fields, and the distinction between retired ore IDs and physical shavings.
anchor = "validator = read('tools/validate_shader_contracts.py')\n"
validator_patch = '''validator = read('tools/validate_shader_contracts.py')
validator = validator.replace(
    'for token in ("int[5](78, 78, 104, 136, 104)", "ivec2(12, 12), 4, 0u", "hudTop + 78u"):',
    'for token in ("int[5](78, 78, 104, 136, 104)", "ivec2(12, 15), titleScale, 0u", "hudTop + 112u"):',
)
validator = validator.replace(
    '"tile_rows"],',
    '"tile_rows", "viewport_left", "viewport_top", "viewport_width", "viewport_height"],',
    1,
)
validator = validator.replace(
    '"tileColumns", "tileRows"],',
    '"tileColumns", "tileRows", "viewportLeft", "viewportTop", "viewportWidth", "viewportHeight"],',
    1,
)
validator = validator.replace(
    'if "MAT_ALUMINUM_SHAVINGS" in reset or "MAT_IRON_SHAVINGS" in reset or "MAT_ALUMINUM_SHAVINGS" in actor or "MAT_IRON_SHAVINGS" in actor:',
    'if "MAT_GOLD_ORE" in reset or "MAT_IRON_ORE" in reset or "MAT_GOLD_ORE" in actor or "MAT_IRON_ORE" in actor:',
)
'''
if text.count(anchor) < 1:
    raise SystemExit('validator source anchor not found')
text = text.replace(anchor, validator_patch, 1)

path.write_text(text, encoding='utf-8', newline='\n')
print('Fix24b palette and validator patches applied.')
