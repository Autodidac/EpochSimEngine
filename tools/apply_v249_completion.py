from pathlib import Path
import shutil

root = Path(__file__).resolve().parents[1]


def replace(path: str, old: str, new: str) -> None:
    target = root / path
    text = target.read_text(encoding="utf-8")
    if old not in text:
        if new in text:
            return
        raise RuntimeError(f"Expected source block missing from {path}")
    target.write_text(text.replace(old, new), encoding="utf-8", newline="\n")


replace(
    "shaders/chemistry.comp",
    """    // TILE_STABLE and TILE_BULK_READY are metadata only. They may suppress
    // unnecessary work, but they never reconstruct loose or damaged cells into
    // structural cells. Dislodged stone therefore stays dislodged.
""",
    """    // Granular terrain may qualify for structural stability after settling,
    // preserving the intended soil/sand/silt/salt/ice terrain behavior. Block-
    // capable solids never regain structural state from tile metadata: dislodged
    // stone, metals, glass, machines, and other blocks remain loose fine cells.
    bool stableGranularTerrain = tileHas(tile, TILE_STABLE) && !isStructural(source) &&
        source.material == tile.material && source.material != MAT_EMPTY &&
        !isCellGas(source) && !isCellLiquid(source) &&
        isReconstructableMaterial(source.material) && !isBlockCapable(source.material);
    if (stableGranularTerrain) {
        result.aux |= AUX_STRUCTURAL | AUX_SUPPORTED;
        result.aux &= ~AUX_MOVED;
    }
""",
)

replace(
    "tools/validate_shader_contracts.py",
    """    # Stable/bulk-ready tile state is metadata only. It must never contain the
    # removed stable-region promotion branch. Explicit machine/habitat structural
    # assignments elsewhere remain valid.
    if \"tileHas(tile, TILE_STABLE) && !isStructural(source)\" in chemistry:
        errors.append(\"stable tile metadata still reconstructs loose cells\")
""",
    """    # Stability promotion is valid only for granular terrain. Block-capable
    # solids must remain loose after damage or support loss.
    for token in (
        \"bool stableGranularTerrain = tileHas(tile, TILE_STABLE)\",
        \"isReconstructableMaterial(source.material) && !isBlockCapable(source.material)\",
        \"result.aux |= AUX_STRUCTURAL | AUX_SUPPORTED;\",
    ):
        if token not in chemistry:
            errors.append(f\"granular terrain stability contract missing {token!r}\")
""",
)

replace(
    "tools/validate_shader_contracts.py",
    """    if \"tileHas(tile, TILE_STABLE) && !isStructural(source)\" in chemistry_comp:
        errors.append(\"stable tile metadata still reconstructs damaged cells\")
""",
    """    if \"isReconstructableMaterial(source.material) && !isBlockCapable(source.material)\" not in chemistry_comp:
        errors.append(\"granular terrain stability is missing or block-capable promotion is not excluded\")
""",
)

replace(
    "RELEASE_NOTES_v2.4.9.md",
    """- Stable tile metadata no longer reattaches damaged loose cells as structural cells.
- Released stone and other block-capable solids fall only as individual fine cells.
""",
    """- Settled sand, soil, silt, salt, and ice retain granular terrain stability qualification.
- Stable tile metadata never reattaches damaged block-capable cells as structural cells.
- Released stone and other block-capable solids fall only as individual fine cells.
""",
)

replace(
    "missioncache.md",
    """| MC-100 | PARTIAL | Solid tiles never move wholesale | Full stone and other block-capable solid regions may report `BULK READY` and sleep through tile metadata, but only individual cells may fall after damage or support loss. No macro dispatch may swap a block-capable 8x8 region. At 31 destroyed cells (48.4375%), the remaining 33 cells crumble to fine loose cells. Static contracts pass; packaged runtime acceptance remains required. |
""",
    """| MC-100 | PARTIAL | Solid tiles never move wholesale | Full stone and other block-capable solid regions may report `BULK READY` and sleep through tile metadata, but only individual cells may fall after damage or support loss. No macro dispatch may swap a block-capable 8x8 region. Sand, soil, silt, salt, and ice retain granular stability qualification without granting block-capable reconstruction. At 31 destroyed cells (48.4375%), the remaining 33 cells crumble to fine loose cells. Static contracts pass; packaged runtime acceptance remains required. |
""",
)

chunk_dir = root / "tools" / "v249_patch"
if chunk_dir.exists():
    shutil.rmtree(chunk_dir)
Path(__file__).unlink()
