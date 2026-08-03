from pathlib import Path

path = Path("tools/validate_shader_contracts.py")
text = path.read_text(encoding="utf-8")
text = text.replace('        "inventoryCount",\n', '        "actor.iron",\n')
text = text.replace('        ".descriptorCount = 17",', '        ".descriptorCount = 16",')
text = text.replace(
    '        "if (map_mode)",\n        "if (player_controls)",\n',
    '        "if (route_to_player)",\n')
path.write_text(text, encoding="utf-8")
