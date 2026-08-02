from pathlib import Path

path = Path("tools/validate_shader_contracts.py")
text = path.read_text(encoding="utf-8")

old_metadata = '''    # Stable/bulk-ready tile state is metadata only. It must never reconstruct
    # loose or damaged cells into structural cells.
    for forbidden in (
        "tileHas(tile, TILE_STABLE) && !isStructural(source)",
        "result.aux |= AUX_STRUCTURAL | AUX_SUPPORTED;",
    ):
        if forbidden in chemistry:
            errors.append(f"stable tile metadata still reconstructs cells: {forbidden!r}")
'''
new_metadata = '''    # Stable/bulk-ready tile state is metadata only. It must never contain the
    # removed stable-region promotion branch. Explicit machine/habitat structural
    # assignments elsewhere remain valid.
    if "tileHas(tile, TILE_STABLE) && !isStructural(source)" in chemistry:
        errors.append("stable tile metadata still reconstructs loose cells")
'''
if old_metadata in text:
    text = text.replace(old_metadata, new_metadata, 1)
elif new_metadata not in text:
    raise SystemExit("metadata-only validator block was not found")

old_contract = '''    for forbidden in ("tileHas(tile, TILE_STABLE) && !isStructural(source)",
                      "result.aux |= AUX_STRUCTURAL | AUX_SUPPORTED"):
        if forbidden in chemistry_comp:
            errors.append(f"stable tile metadata still reconstructs damaged cells: {forbidden!r}")
'''
new_contract = '''    if "tileHas(tile, TILE_STABLE) && !isStructural(source)" in chemistry_comp:
        errors.append("stable tile metadata still reconstructs damaged cells")
'''
if old_contract in text:
    text = text.replace(old_contract, new_contract, 1)
elif new_contract not in text:
    raise SystemExit("solid-policy validator scope block was not found")

path.write_text(text, encoding="utf-8")
Path("tools/apply_v249_zz_validator_scope.py").unlink(missing_ok=True)
