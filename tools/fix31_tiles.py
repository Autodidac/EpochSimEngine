#!/usr/bin/env python3
from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
path = ROOT / "shaders/tiles.comp"
text = path.read_text(encoding="utf-8")


def literal(old: str, new: str) -> None:
    global text
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"tiles.comp literal match count {count}: {old[:60]!r}")
    text = text.replace(old, new, 1)


def regex(pattern: str, replacement: str) -> None:
    global text
    text, count = re.subn(pattern, replacement, text, count=1, flags=re.S)
    if count != 1:
        raise SystemExit(f"tiles.comp regex match count {count}: {pattern[:60]!r}")


literal(
    "    bool activeContent = false;\n",
    """    bool activeContent = false;
    bool hasQueen = false;
    bool hasHive = false;
    bool hasFlower = false;
    bool hasHoney = false;
    bool hasMigratingQueen = false;
    bool beeHazard = false;
    ivec2 queenLocal = ivec2(0);
    ivec2 flowerLocal = ivec2(0);
    ivec2 honeyLocal = ivec2(0);
    uint beeCount = 0u;
""",
)
literal(
    """            Cell cell = cells[indexOf(p)];
            if (cell.material == MAT_EMPTY) continue;
""",
    """            Cell cell = cells[indexOf(p)];
            if (cell.material == MAT_EMPTY) continue;
            ivec2 local = ivec2(x, y);
            if (cell.material == MAT_QUEEN_BEE) {
                if (!hasQueen) queenLocal = local;
                hasQueen = true;
            }
            if (cell.material == MAT_BEE_NEST) hasHive = true;
            if (cell.material == MAT_FLOWER) {
                if (!hasFlower) flowerLocal = local;
                hasFlower = true;
            }
            if (cell.material == MAT_HONEY) {
                if (!hasHoney) honeyLocal = local;
                hasHoney = true;
            }
            if (cell.material == MAT_BEE) {
                ++beeCount;
                if ((cell.aux & AUX_CHARGED) != 0u) {
                    if (!hasMigratingQueen) queenLocal = local;
                    hasMigratingQueen = true;
                }
            }
            beeHazard = beeHazard || cell.material == MAT_FIRE || cell.material == MAT_LAVA ||
                        cell.material == MAT_ACID || cell.material == MAT_SMOKE ||
                        cell.material == MAT_DIRTY_STEAM || cell.material == MAT_LIGHTNING ||
                        cell.material == MAT_RADIATION;
""",
)
regex(
    r"""            bool activeAgent = cell\.material == MAT_BEE \|\| cell\.material == MAT_ANT \|\|
\s*cell\.material == MAT_BEETLE \|\| cell\.material == MAT_SEED \|\|
\s*cell\.material == MAT_POLLEN;""",
    """            bool activeAgent = cell.material == MAT_BEE || cell.material == MAT_QUEEN_BEE ||
                                cell.material == MAT_BEE_NEST || cell.material == MAT_ANT ||
                                cell.material == MAT_BEETLE || cell.material == MAT_SEED ||
                                cell.material == MAT_POLLEN;""",
)
literal(
    "                             previous.occupancy >= TILE_STABILITY_OCCUPANCY;\n",
    "                             tileOccupancy(previous) >= TILE_STABILITY_OCCUPANCY;\n",
)
literal(
    """    if (damaged) flags |= TILE_DAMAGED;

    uint stableCells = structuralTile ? dominantCount : occupied;
""",
    """    if (damaged) flags |= TILE_DAMAGED;
    if (hasQueen) flags |= TILE_HAS_QUEEN;
    if (hasHive) flags |= TILE_HAS_HIVE;
    if (hasFlower) flags |= TILE_HAS_FLOWER;
    if (hasHoney) flags |= TILE_HAS_HONEY;
    if (beeCount > 0u) flags |= TILE_HAS_BEES;
    if (hasMigratingQueen) flags |= TILE_HAS_MIGRATING_QUEEN;
    if (beeHazard) flags |= TILE_BEE_HAZARD;

    uint stableCells = structuralTile ? dominantCount : occupied;
""",
)
regex(
    r"""    tiles\[index\] = TileState\(dominant,\s*structuralTile \? dominantCount : occupied,
\s*flags, packTileCounters\(stableTicks, cooldown\)\);""",
    """    uint packedOccupancy = packTileOccupancy(
        structuralTile ? dominantCount : occupied,
        queenLocal, flowerLocal, honeyLocal, beeCount);
    tiles[index] = TileState(dominant, packedOccupancy,
                             flags, packTileCounters(stableTicks, cooldown));""",
)

path.write_text(text, encoding="utf-8", newline="\n")
print("Applied robust Fix31 tiles.comp metadata patch.")
