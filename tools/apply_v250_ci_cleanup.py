from pathlib import Path

path = Path("tools/validate_shader_contracts.py")
text = path.read_text(encoding="utf-8")
old = 'ROOT / ".github/workflows/v249-ci.yml"'
new = 'ROOT / ".github/workflows/v250-ci.yml"'
if old in text:
    path.write_text(text.replace(old, new, 1), encoding="utf-8")
elif new not in text:
    raise SystemExit("tools/validate_shader_contracts.py: release workflow contract not found")

Path("tools/apply_v250_ci_cleanup.py").unlink(missing_ok=True)
