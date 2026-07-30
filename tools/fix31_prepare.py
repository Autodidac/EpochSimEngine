#!/usr/bin/env python3
from pathlib import Path

path = Path(__file__).with_name("fix31.py")
text = path.read_text(encoding="utf-8")
start_marker = 'replace_once(\n    "shaders/tiles.comp",'
end_marker = 'replace_once(\n    "shaders/reset.comp",'
start = text.find(start_marker)
end = text.find(end_marker, start)
if start < 0 or end < 0:
    raise SystemExit("Unable to isolate delegated tiles.comp patch section.")
text = text[:start] + 'print("Delegating tiles.comp edits to fix31_tiles.py")\n\n' + text[end:]
path.write_text(text, encoding="utf-8", newline="\n")
print("Prepared Fix31 patch orchestration.")
