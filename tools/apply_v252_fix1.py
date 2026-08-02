#!/usr/bin/env python3
from pathlib import Path

root = Path(__file__).resolve().parents[1]
path = root / "tools/validate_shader_contracts.py"
text = path.read_text(encoding="utf-8")
old = 'ROOT / ".github/workflows/v250-ci.yml"'
new = 'ROOT / ".github/workflows/ci-release.yml"'
if old in text:
    path.write_text(text.replace(old, new), encoding="utf-8")
elif new not in text:
    raise SystemExit("release workflow validator reference not found")
Path(__file__).unlink(missing_ok=True)
