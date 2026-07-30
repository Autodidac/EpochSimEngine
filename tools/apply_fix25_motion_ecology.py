#!/usr/bin/env python3
from __future__ import annotations

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def replace_once(path: Path, old: str, new: str) -> None:
    text = path.read_text(encoding="utf-8")
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{path}: expected one replacement, found {count}")
    path.write_text(text.replace(old, new, 1), encoding="utf-8", newline="\n")


materials = ROOT / "shaders/materials.glsl"
replace_once(
    materials,
    """    else if (material == MAT_SMELTER) temperature = 180;
    else if (material == MAT_PLANT_STEM) temperature = 900;
""",
    """    else if (material == MAT_SMELTER) temperature = 180;
    else if (material == MAT_PLANT_STEM) temperature = 20;
""",
)
replace_once(
    materials,
    """    else if (material == MAT_ANT || material == MAT_BEETLE) aux |= 255u;
    else if (material == MAT_PLANT_STEM) aux |= AUX_CHARGED | 72u;
""",
    """    else if (material == MAT_ANT || material == MAT_BEETLE)
        aux |= 1u + (hash32(seed ^ step ^ material) & 1u);
    else if (material == MAT_PLANT_STEM) aux |= AUX_PLANT_STEM | 1u;
""",
)
replace_once(
    materials,
    """    return material == MAT_STONE || material == MAT_CRYSTAL || material == MAT_GRASS ||
           material == MAT_WOOD || material == MAT_PLASTIC || material == MAT_ACID_RESISTANT_PLASTIC ||
""",
    """    return material == MAT_STONE || material == MAT_CRYSTAL || material == MAT_GRASS ||
           material == MAT_PLANT_STEM || material == MAT_WOOD ||
           material == MAT_PLASTIC || material == MAT_ACID_RESISTANT_PLASTIC ||
""",
)
replace_once(
    materials,
    """    return material == MAT_GRASS || material == MAT_FLOWER || material == MAT_OIL ||
""",
    """    return material == MAT_GRASS || material == MAT_PLANT_STEM ||
           material == MAT_FLOWER || material == MAT_OIL ||
""",
)

tiles = ROOT / "shaders/tiles.comp"
replace_once(
    tiles,
    """    bool hot = false;
    bool moving = false;
    bool reacting = false;
""",
    """    bool hot = false;
    bool moving = false;
    bool reacting = false;
    bool activeContent = false;
""",
)
replace_once(
    tiles,
    """            Cell cell = cells[indexOf(p)];
            if (cell.material == MAT_EMPTY || isCellGas(cell) || isCellLiquid(cell)) continue;
            ++occupied;
            if (cell.material < MATERIAL_COUNT) ++counts[cell.material];
""",
    """            Cell cell = cells[indexOf(p)];
            if (cell.material == MAT_EMPTY) continue;
            if (isCellGas(cell) || isCellLiquid(cell)) {
                activeContent = true;
                moving = true;
                continue;
            }
            ++occupied;
            if (cell.material < MATERIAL_COUNT) ++counts[cell.material];
            bool activeAgent = cell.material == MAT_BEE || cell.material == MAT_ANT ||
                               cell.material == MAT_BEETLE || cell.material == MAT_SEED ||
                               cell.material == MAT_POLLEN;
            bool activeLoose = !isStructural(cell) &&
                               !isReconstructableMaterial(cell.material) &&
                               !isCellImmovable(cell);
            activeContent = activeContent || activeAgent || activeLoose;
""",
)
replace_once(
    tiles,
    """            moving = moving || (cell.aux & AUX_MOVED) != 0u || cell.age < 8u;
""",
    """            moving = moving || activeAgent || activeLoose ||
                     (cell.aux & AUX_MOVED) != 0u || cell.age < 8u;
""",
)
replace_once(
    tiles,
    """    bool sleeping = terrainStable && !damaged && !moving && !hot && !reacting &&
                     dominantCount == stableCells;
""",
    """    bool sleeping = terrainStable && !damaged && !moving && !hot && !reacting &&
                     !activeContent && dominantCount == stableCells;
""",
)

move = ROOT / "shaders/move.comp"
replace_once(
    move,
    """    return material == MAT_STONE || material == MAT_CRYSTAL || material == MAT_GRASS ||
           material == MAT_WOOD || material == MAT_PLASTIC ||
""",
    """    return material == MAT_STONE || material == MAT_CRYSTAL || material == MAT_GRASS ||
           material == MAT_PLANT_STEM || material == MAT_WOOD || material == MAT_PLASTIC ||
""",
)
old_bee = """bool beeMoveAllowed(Cell bee, ivec2 sourcePosition, ivec2 targetPosition, uint randomValue) {
    bool sourceHazard = adjacentHazard(sourcePosition);
    bool targetHazard = adjacentHazard(targetPosition);
    if (sourceHazard != targetHazard) return sourceHazard;

    bool carryingPollen = (bee.aux & AUX_BEE_POLLEN) != 0u;
    bool resting = stateValue(bee) > 0u;
    uint first = carryingPollen || resting ? MAT_QUEEN_BEE : MAT_FLOWER;
    uint second = carryingPollen || resting ? MAT_BEE_NEST : MAT_FLOWER;
    bool sourceTarget = adjacentContains(sourcePosition, first, second);
    bool targetTarget = adjacentContains(targetPosition, first, second);
    if (sourceTarget != targetTarget) return targetTarget;

    if (!carryingPollen && !resting && (bee.aux & AUX_BEE_FED) == 0u) {
        bool sourceFood = adjacentContains(sourcePosition, MAT_HONEY, MAT_QUEEN_BEE);
        bool targetFood = adjacentContains(targetPosition, MAT_HONEY, MAT_QUEEN_BEE);
        if (sourceFood != targetFood) return targetFood;
    }
    return (randomValue & 3u) == 0u;
}
"""
new_bee = """bool targetMaterial(uint material, uint first, uint second) {
    return material == first || material == second;
}

int localTargetSignal(ivec2 p, uint first, uint second) {
    int score = adjacentContains(p, first, second) ? 32 : 0;
    score += targetMaterial(sampleAt(p + ivec2(2, 0)).material, first, second) ? 14 : 0;
    score += targetMaterial(sampleAt(p + ivec2(-2, 0)).material, first, second) ? 14 : 0;
    score += targetMaterial(sampleAt(p + ivec2(0, 2)).material, first, second) ? 14 : 0;
    score += targetMaterial(sampleAt(p + ivec2(0, -2)).material, first, second) ? 14 : 0;
    score += targetMaterial(sampleAt(p + ivec2(2, 2)).material, first, second) ? 9 : 0;
    score += targetMaterial(sampleAt(p + ivec2(-2, 2)).material, first, second) ? 9 : 0;
    score += targetMaterial(sampleAt(p + ivec2(2, -2)).material, first, second) ? 9 : 0;
    score += targetMaterial(sampleAt(p + ivec2(-2, -2)).material, first, second) ? 9 : 0;
    score += targetMaterial(sampleAt(p + ivec2(4, 0)).material, first, second) ? 5 : 0;
    score += targetMaterial(sampleAt(p + ivec2(-4, 0)).material, first, second) ? 5 : 0;
    score += targetMaterial(sampleAt(p + ivec2(0, 4)).material, first, second) ? 5 : 0;
    score += targetMaterial(sampleAt(p + ivec2(0, -4)).material, first, second) ? 5 : 0;
    return score;
}

int beeTargetSignal(Cell bee, ivec2 p) {
    bool carryingPollen = (bee.aux & AUX_BEE_POLLEN) != 0u;
    bool resting = stateValue(bee) > 0u;
    if (carryingPollen || resting) {
        return localTargetSignal(p, MAT_QUEEN_BEE, MAT_BEE_NEST);
    }
    int flowerSignal = localTargetSignal(p, MAT_FLOWER, MAT_FLOWER);
    if ((bee.aux & AUX_BEE_FED) == 0u) {
        int foodSignal = localTargetSignal(p, MAT_HONEY, MAT_QUEEN_BEE);
        return max(flowerSignal, foodSignal);
    }
    return flowerSignal;
}

int beeWaveVertical(Cell bee, ivec2 p) {
    uint phase = (movePc.step / 3u + uint(p.x) * 3u + uint(p.y) +
                  ((bee.aux >> 8u) & 15u)) & 15u;
    if (phase < 4u) return -1;
    if (phase < 8u) return 0;
    if (phase < 12u) return 1;
    return 0;
}

bool beeMoveAllowed(Cell bee, ivec2 sourcePosition, ivec2 targetPosition, uint randomValue) {
    if ((bee.aux & AUX_MOVED) != 0u) return false;
    bool sourceHazard = adjacentHazard(sourcePosition);
    bool targetHazard = adjacentHazard(targetPosition);
    if (sourceHazard != targetHazard) return sourceHazard;

    int sourceSignal = beeTargetSignal(bee, sourcePosition);
    int targetSignal = beeTargetSignal(bee, targetPosition);
    if (sourceSignal != targetSignal) return targetSignal > sourceSignal;

    ivec2 delta = targetPosition - sourcePosition;
    int desiredVertical = beeWaveVertical(bee, sourcePosition);
    if (delta.y == desiredVertical) return true;
    if (delta.x != 0 && (randomValue & 1u) == 0u) return true;
    return (randomValue & 3u) == 0u;
}
"""
replace_once(move, old_bee, new_bee)
old_insects = """bool isInsect(uint material) { return material == MAT_ANT || material == MAT_BEETLE; }

bool insectGrounded(ivec2 p) {
    Cell below = sampleAt(p + ivec2(0, 1));
    return below.material != MAT_EMPTY && !isCellGas(below);
}
"""
new_insects = """bool isInsect(uint material) { return material == MAT_ANT || material == MAT_BEETLE; }

bool insectGrounded(ivec2 p) {
    Cell below = sampleAt(p + ivec2(0, 1));
    return below.material != MAT_EMPTY && !isCellGas(below) && !isCellLiquid(below);
}

int insectTargetSignal(Cell insect, ivec2 p) {
    if (insect.material == MAT_ANT) {
        return localTargetSignal(p, MAT_WASTE, MAT_FERTILIZER) +
               localTargetSignal(p, MAT_DIRT, MAT_GRASS) / 2;
    }
    return localTargetSignal(p, MAT_WASTE, MAT_WOOD) +
           localTargetSignal(p, MAT_FERTILIZER, MAT_MUD) / 2;
}

bool insectMoveAllowed(Cell insect, ivec2 sourcePosition, ivec2 targetPosition, uint randomValue) {
    if ((insect.aux & AUX_MOVED) != 0u) return false;
    int sourceSignal = insectTargetSignal(insect, sourcePosition);
    int targetSignal = insectTargetSignal(insect, targetPosition);
    if (sourceSignal != targetSignal) return targetSignal > sourceSignal;

    ivec2 delta = targetPosition - sourcePosition;
    uint directionState = stateValue(insect);
    int preferredDirection = directionState == 2u ? -1 :
        (directionState == 1u ? 1 : (((insect.aux >> 8u) & 1u) != 0u ? 1 : -1));
    if (delta.x == preferredDirection) return true;
    return (randomValue & 7u) == 0u;
}
"""
replace_once(move, old_insects, new_insects)
old_sleep = """bool pairSleeping(ivec2 a, ivec2 b) {
    TileState tileA = tiles[tileIndex(a, movePc.width)];
    TileState tileB = tiles[tileIndex(b, movePc.width)];
    return tileHas(tileA, TILE_SLEEPING) && tileHas(tileB, TILE_SLEEPING);
}
"""
new_sleep = """bool sleepSafe(Cell cell) {
    if (cell.material == MAT_EMPTY || isStructural(cell)) return true;
    if (cell.material == MAT_BEE || isInsect(cell.material) ||
        cell.material == MAT_SEED || cell.material == MAT_POLLEN) return false;
    if (isCellGas(cell) || isCellLiquid(cell) || isCellPowder(cell) || isLooseSolid(cell)) return false;
    return isCellImmovable(cell);
}

bool pairSleeping(ivec2 a, ivec2 b) {
    TileState tileA = tiles[tileIndex(a, movePc.width)];
    TileState tileB = tiles[tileIndex(b, movePc.width)];
    if (!tileHas(tileA, TILE_SLEEPING) || !tileHas(tileB, TILE_SLEEPING)) return false;
    return sleepSafe(moveAt(a)) && sleepSafe(moveAt(b));
}
"""
replace_once(move, old_sleep, new_sleep)
replace_once(
    move,
    """    if (a.material == MAT_CARBON_DIOXIDE &&
""",
    """    if (isInsect(a.material) && b.material == MAT_EMPTY && !insectGrounded(top) &&
        (a.aux & AUX_MOVED) == 0u) {
        swapCells(top, bottom);
        return;
    }

    if (a.material == MAT_CARBON_DIOXIDE &&
""",
)
replace_once(
    move,
    """    Cell below = sampleAt(sourcePosition + ivec2(0, 1));
    bool cohesiveMud = source.material == MAT_MUD &&
""",
    """    Cell below = sampleAt(sourcePosition + ivec2(0, 1));
    if (!upward && isInsect(source.material) && target.material == MAT_EMPTY &&
        !insectGrounded(sourcePosition) && (source.aux & AUX_MOVED) == 0u) {
        swapCells(sourcePosition, targetPosition);
        return;
    }
    if (upward && isInsect(source.material) && target.material == MAT_EMPTY &&
        insectGrounded(sourcePosition) && insectGrounded(targetPosition) &&
        insectMoveAllowed(source, sourcePosition, targetPosition, randomValue)) {
        swapCells(sourcePosition, targetPosition);
        return;
    }

    bool cohesiveMud = source.material == MAT_MUD &&
""",
)
replace_once(
    move,
    """    if (isInsect(a.material) && b.material == MAT_EMPTY && insectGrounded(left) && (a.age & 1u) == 0u) {
        swapCells(left, right);
        return;
    }
    if (isInsect(b.material) && a.material == MAT_EMPTY && insectGrounded(right) && (b.age & 1u) == 0u) {
        swapCells(left, right);
        return;
    }
""",
    """    if (isInsect(a.material) && b.material == MAT_EMPTY &&
        insectGrounded(left) && insectGrounded(right) &&
        insectMoveAllowed(a, left, right, randomValue)) {
        swapCells(left, right);
        return;
    }
    if (isInsect(b.material) && a.material == MAT_EMPTY &&
        insectGrounded(right) && insectGrounded(left) &&
        insectMoveAllowed(b, right, left, randomValue >> 1u)) {
        swapCells(left, right);
        return;
    }
""",
)

chemistry = ROOT / "shaders/chemistry.comp"
old_seed = """bool seedHasGrowingConditions(ivec2 seedPosition) {
    Cell seed = at(seedPosition);
    if (seed.material != MAT_SEED || seed.age < 240u) return false;
    uint soil = at(seedPosition + ivec2(0, 1)).material;
    return (soil == MAT_DIRT || soil == MAT_GRASS || soil == MAT_MUD) &&
           hasFreshMoistureRadius(seedPosition, 2) &&
           !hasNeighbor(seedPosition, MAT_SALTWATER) &&
           !hasNeighbor(seedPosition, MAT_ACID) &&
           light[indexOf(seedPosition)] > 145u;
}
"""
new_seed = """bool seedHasGrowingConditions(ivec2 seedPosition) {
    Cell seed = at(seedPosition);
    if (seed.material != MAT_SEED || seed.age < 90u) return false;
    uint soil = at(seedPosition + ivec2(0, 1)).material;
    bool fertileSoil = soil == MAT_DIRT || soil == MAT_GRASS || soil == MAT_MUD ||
                       soil == MAT_FERTILIZER;
    bool moisture = hasFreshMoistureRadius(seedPosition, 4) ||
                    hasWithin(seedPosition, MAT_FERTILIZER, 3);
    return fertileSoil && moisture &&
           !hasNeighbor(seedPosition, MAT_SALTWATER) &&
           !hasNeighbor(seedPosition, MAT_ACID) &&
           light[indexOf(seedPosition)] > 105u;
}

ivec2 flowerSeedTarget(ivec2 flowerPosition, Cell flower) {
    uint choice = hash32(indexOf(flowerPosition) ^ flower.age ^ pc.seed ^ 0xf10a3u) & 3u;
    return flowerPosition + cardinalOffsets[choice];
}

bool flowerDropsSeed(ivec2 targetPosition) {
    for (uint i = 0u; i < 4u; ++i) {
        ivec2 flowerPosition = targetPosition - cardinalOffsets[i];
        Cell flower = at(flowerPosition);
        if (flower.material == MAT_FLOWER && flower.age > 900u &&
            (cellHash(flowerPosition, 0x5eedu) & 1023u) == 0u &&
            all(equal(flowerSeedTarget(flowerPosition, flower), targetPosition))) {
            return true;
        }
    }
    return false;
}
"""
replace_once(chemistry, old_seed, new_seed)
replace_once(
    chemistry,
    """        if (hasNeighbor(p, MAT_INSECT_HABITAT) && (randomValue & 4095u) == 0u) {
            result = makeCell((randomValue & 8192u) == 0u ? MAT_ANT : MAT_BEETLE);
""",
    """        if (flowerDropsSeed(p)) {
            result = makeCell(MAT_SEED);
            commitResult(index, source, result);
            return;
        }
        if (hasNeighbor(p, MAT_INSECT_HABITAT) && (randomValue & 1023u) == 0u) {
            result = makeCell((randomValue & 2048u) == 0u ? MAT_ANT : MAT_BEETLE);
""",
)
replace_once(
    chemistry,
    """                } else if (any(notEqual(queenOffset, ivec2(0))) && queenDistanceSquared <= 6 &&
                            (hasNeighbor(p, MAT_HONEY) || hasNeighbor(p, MAT_POLLEN)) &&
                            (randomValue & 4095u) == 0u) {
""",
    """                } else if (any(notEqual(queenOffset, ivec2(0))) && queenDistanceSquared <= 6 &&
                            (hasNeighbor(p, MAT_HONEY) || hasNeighbor(p, MAT_POLLEN)) &&
                            (randomValue & 255u) == 0u) {
""",
)
replace_once(
    chemistry,
    """                if (below.material == MAT_PLANT_STEM &&
                    below.age > 180u && hasFreshMoistureRadius(p, 3) && light[index] > 145u) {
                    uint stage = stateValue(below);
                    if (stage < 3u) {
                        result = makeCell(MAT_PLANT_STEM);
                        setStateValue(result, stage + 1u);
                    } else {
                        result = makeCell(MAT_FLOWER);
                    }
                }
""",
    """                bool stemMoisture = hasFreshMoistureRadius(p, 4) ||
                                    hasWithin(p, MAT_FERTILIZER, 3);
                if (below.material == MAT_PLANT_STEM &&
                    below.age > 60u && stemMoisture && light[index] > 105u) {
                    uint stage = stateValue(below);
                    if (stage < 3u) {
                        result = makeCell(MAT_PLANT_STEM);
                        setStateValue(result, stage + 1u);
                    } else {
                        result = makeCell(MAT_FLOWER);
                    }
                }
""",
)
old_plants = """    // Seed growth is staged. The seed becomes the first grass stem segment;
    // subsequent segments grow upward and the flower exists only above grass.
    if (source.material == MAT_SEED && seedHasGrowingConditions(p)) {
        result = makeCell(MAT_PLANT_STEM);
        setStateValue(result, 1u);
    } else if (source.material == MAT_DIRT) {
        bool exposedSurface = at(p + ivec2(0, -1)).material == MAT_EMPTY;
        if (exposedSurface && light[index] > 185u && hasFreshMoistureRadius(p, 3) &&
            hasNeighbor(p, MAT_GRASS) && source.age > 240u && (randomValue & 255u) == 0u) {
            result = makeCell(MAT_GRASS);
        }
    } else if (source.material == MAT_GRASS) {
        if (nearSaltwater || nearAcid || nearFire || nearLava || light[index] < 18u) {
            if (source.age > 600u && (randomValue & 127u) == 0u) {
                result = makeCell(nearFire || nearLava ? MAT_ASH : MAT_DIRT);
            }
        } else if ((source.aux & AUX_PLANT_STEM) == 0u && !hasFreshMoistureRadius(p, 4) &&
                   source.age > 7200u && (randomValue & 1023u) == 0u) {
            result = makeCell(MAT_DIRT);
        }
    } else if (source.material == MAT_FLOWER) {
        Cell support = at(p + ivec2(0, 1));
        bool validStem = support.material == MAT_PLANT_STEM;
        if (!validStem || nearSaltwater || nearAcid || nearFire || nearLava || light[index] < 30u) {
            result = makeCell(nearFire || nearLava ? MAT_ASH : MAT_WASTE);
        }
    }
"""
new_plants = """    // Grass is a surface cover on dirt, never a vertical dirt-like stem.
    // Seeds independently grow a short stem before a flower can bloom.
    if (source.material == MAT_SEED && seedHasGrowingConditions(p)) {
        result = makeCell(MAT_PLANT_STEM);
        setStateValue(result, 1u);
    } else if (source.material == MAT_DIRT) {
        bool exposedSurface = at(p + ivec2(0, -1)).material == MAT_EMPTY;
        bool moisture = hasFreshMoistureRadius(p, 4) || hasWithin(p, MAT_FERTILIZER, 3);
        bool grassFrontier = hasWithin(p, MAT_GRASS, 3);
        uint spreadMask = grassFrontier ? 63u : 4095u;
        if (exposedSurface && light[index] > 145u && moisture &&
            source.age > 120u && (randomValue & spreadMask) == 0u) {
            result = makeCell(MAT_GRASS);
        }
    } else if (source.material == MAT_GRASS) {
        if (nearSaltwater || nearAcid || nearFire || nearLava || light[index] < 18u) {
            if (source.age > 600u && (randomValue & 127u) == 0u) {
                result = makeCell(nearFire || nearLava ? MAT_ASH : MAT_DIRT);
            }
        } else if (!hasFreshMoistureRadius(p, 5) && !hasWithin(p, MAT_FERTILIZER, 3) &&
                   source.age > 7200u && (randomValue & 1023u) == 0u) {
            result = makeCell(MAT_DIRT);
        }
    } else if (source.material == MAT_PLANT_STEM) {
        Cell support = at(p + ivec2(0, 1));
        bool validSupport = support.material == MAT_PLANT_STEM || support.material == MAT_GRASS ||
                            support.material == MAT_DIRT || support.material == MAT_MUD ||
                            support.material == MAT_FERTILIZER;
        if (!validSupport || nearSaltwater || nearAcid || nearFire || nearLava || light[index] < 24u) {
            result = makeCell(nearFire || nearLava ? MAT_ASH : MAT_WASTE);
        } else {
            result.aux |= AUX_PLANT_STEM;
            result.aux &= ~AUX_CHARGED;
        }
    } else if (source.material == MAT_FLOWER) {
        Cell support = at(p + ivec2(0, 1));
        bool validStem = support.material == MAT_PLANT_STEM;
        if (!validStem || nearSaltwater || nearAcid || nearFire || nearLava || light[index] < 30u) {
            result = makeCell(nearFire || nearLava ? MAT_ASH : MAT_WASTE);
        }
    }
"""
replace_once(chemistry, old_plants, new_plants)

validator = ROOT / "tools/validate_shader_contracts.py"
validator_text = validator.read_text(encoding="utf-8")
marker = """    if errors:
        print("Shader contract validation failed:", file=sys.stderr)
"""
if validator_text.count(marker) != 1:
    raise RuntimeError("validator final error marker not found exactly once")
checks = """    motion_ecology_contracts = {
        "tiles": (tiles, ("activeContent", "!activeContent", "activeAgent", "activeLoose")),
        "movement": (movement, ("sleepSafe", "localTargetSignal", "beeWaveVertical",
                                 "insectMoveAllowed", "MAT_PLANT_STEM")),
        "chemistry": (chemistry, ("flowerDropsSeed", "stemMoisture", "grassFrontier",
                                  "source.material == MAT_PLANT_STEM")),
        "materials": (materials, ("MAT_PLANT_STEM) temperature = 20",
                                  "AUX_PLANT_STEM | 1u")),
    }
    for contract, (text, tokens) in motion_ecology_contracts.items():
        for token in tokens:
            if token not in text:
                errors.append(f"{contract} motion/ecology contract missing {token!r}")
    for forbidden in ("MAT_PLANT_STEM) temperature = 900", "AUX_CHARGED | 72u"):
        if forbidden in materials:
            errors.append(f"plant stem still aliases obsolete projectile state: {forbidden!r}")
    if "bool sleeping = terrainStable" in tiles and "!activeContent" not in tiles:
        errors.append("tile sleeping still ignores dynamic biology and fluids")
    if "if ((bee.aux & AUX_MOVED) != 0u) return false;" not in movement:
        errors.append("bees can move repeatedly in one simulation tick")
    if "if ((insect.aux & AUX_MOVED) != 0u) return false;" not in movement:
        errors.append("insects can move repeatedly in one simulation tick")

"""
validator.write_text(validator_text.replace(marker, checks + marker, 1), encoding="utf-8", newline="\n")

audit = ROOT / "tools/audit_ecology_motion.py"
audit.write_text(
    """#!/usr/bin/env python3
from pathlib import Path

root = Path(__file__).resolve().parents[1]
files = {
    "materials": (root / "shaders/materials.glsl").read_text(encoding="utf-8"),
    "tiles": (root / "shaders/tiles.comp").read_text(encoding="utf-8"),
    "move": (root / "shaders/move.comp").read_text(encoding="utf-8"),
    "chemistry": (root / "shaders/chemistry.comp").read_text(encoding="utf-8"),
}
errors = []
required = {
    "materials": ["MAT_PLANT_STEM) temperature = 20", "AUX_PLANT_STEM | 1u"],
    "tiles": ["activeContent", "!activeContent", "activeAgent", "activeLoose"],
    "move": ["sleepSafe", "beeWaveVertical", "insectMoveAllowed", "localTargetSignal"],
    "chemistry": ["flowerDropsSeed", "grassFrontier", "stemMoisture",
                  "source.material == MAT_PLANT_STEM"],
}
for name, tokens in required.items():
    for token in tokens:
        if token not in files[name]:
            errors.append(f"{name}: missing {token}")
forbidden = {
    "materials": ["MAT_PLANT_STEM) temperature = 900", "AUX_CHARGED | 72u"],
    "move": ["bool pairSleeping(ivec2 a, ivec2 b) {\\n    TileState"],
}
for name, tokens in forbidden.items():
    for token in tokens:
        if token in files[name]:
            errors.append(f"{name}: forbidden legacy contract remains: {token}")
if errors:
    raise SystemExit("\\n".join(errors))
print("Ecology and motion contracts passed: dynamic tiles, bees, insects, grass, seeds, stems, and flowers.")
""",
    encoding="utf-8",
    newline="\n",
)

print("SandHybrid Fix25 motion/ecology correction applied.")
