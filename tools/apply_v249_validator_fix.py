from pathlib import Path

path = Path("tools/validate_shader_contracts.py")
text = path.read_text(encoding="utf-8")
old = '''    stable_transition = chemistry.split("if (tileHas(tile, TILE_STABLE)", 1)[1].split(
        "if (isStructural(source)", 1
    )[0]
    if "result.aux |= AUX_STRUCTURAL | AUX_SUPPORTED;" not in stable_transition:
        errors.append("settled terrain is supported but never promoted to structural state")
    if "setStateValue(result, 255u)" in stable_transition:
        errors.append("stability qualification resets represented damage instead of preserving it")
'''
new = '''    # Stable/bulk-ready tile state is metadata only. It must never reconstruct
    # loose or damaged cells into structural cells.
    for forbidden in (
        "tileHas(tile, TILE_STABLE) && !isStructural(source)",
        "result.aux |= AUX_STRUCTURAL | AUX_SUPPORTED;",
    ):
        if forbidden in chemistry:
            errors.append(f"stable tile metadata still reconstructs cells: {forbidden!r}")
'''
if old in text:
    path.write_text(text.replace(old, new, 1), encoding="utf-8")
elif new not in text:
    raise SystemExit("obsolete stable-transition validator block was not found")

Path("tools/apply_v249_validator_fix.py").unlink(missing_ok=True)
