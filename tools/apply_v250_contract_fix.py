from pathlib import Path

path = Path("tests/public_api_contract.cpp")
text = path.read_text(encoding="utf-8")
old = "static_assert(sandhybrid::library_api_version == 1u);"
new = "static_assert(sandhybrid::library_api_version == 2u);"
if old in text:
    path.write_text(text.replace(old, new, 1), encoding="utf-8")
elif new not in text:
    raise SystemExit("tests/public_api_contract.cpp: API version contract not found")

Path("tools/apply_v250_contract_fix.py").unlink(missing_ok=True)
