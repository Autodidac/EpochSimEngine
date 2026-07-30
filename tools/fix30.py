from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]

def edit(path: str, pattern: str, replacement: str) -> None:
    file = ROOT / path
    text = file.read_text(encoding="utf-8")
    updated, count = re.subn(pattern, replacement, text, count=1, flags=re.S)
    if count != 1:
        raise SystemExit(f"{path}: expected one match for {pattern[:64]!r}, found {count}")
    file.write_text(updated, encoding="utf-8", newline="\n")

reset_helpers = r'''
const uint BEE_FORMATION_COUNT = 200u;

ivec2 beeFormationOffset(uint slot) {
    uint lane = slot % 3u;
    uint laneIndex = slot / 3u;
    uint laneCount = lane == 2u ? 66u : 67u;
    float angleOffset = lane == 0u ? 0.0 : (lane == 1u ? 0.7 : 1.4);
    float angle = 6.28318530718 * float(laneIndex) / float(laneCount) + angleOffset;
    float baseRadius = lane == 0u ? 18.0 : (lane == 1u ? 30.0 : 44.0);
    float amplitude = lane == 0u ? 3.5 : (lane == 1u ? 5.0 : 7.0);
    float phase = lane == 0u ? 0.0 : (lane == 1u ? 1.7 : 3.2);
    float radius = baseRadius + sin(angle * 3.0 + baseRadius * 0.14 + phase) * amplitude;
    return ivec2(round(vec2(cos(angle), sin(angle)) * radius));
}

int beeFormationSlot(ivec2 offset) {
    int distanceSquared = offset.x * offset.x + offset.y * offset.y;
    if (distanceSquared < 120 || distanceSquared > 3200) return -1;
    for (uint slot = 0u; slot < BEE_FORMATION_COUNT; ++slot)
        if (all(equal(offset, beeFormationOffset(slot)))) return int(slot);
    return -1;
}

'''
edit("shaders/reset.comp", r"uint ecosystemMaterial\(ivec2 p\) \{",
     reset_helpers + "uint ecosystemMaterial(ivec2 p) {")

reset_swarm = r'''    // Canonical hive and exactly 200 deterministic bee slots. The slots reproduce
    // the three authored wavy bands without hash-density drift between resets.
    ivec2 queen = ivec2(width - 104, height - 136);
    ivec2 q = p - queen;
    int q2 = q.x * q.x + q.y * q.y;
    if (q2 >= 28 && q2 < 108) material = MAT_BEE_NEST;
    if (q2 == 0) material = MAT_QUEEN_BEE;
    else if (q.x >= 1 && q.x <= 12 && abs(q.y) <= 1) material = MAT_EMPTY;
    else if (q2 < 28) {
        uint chamber = hash32(indexOf(p) ^ pc.seed ^ 0xb33u);
        material = (chamber & 3u) == 0u ? MAT_EMPTY
            : ((chamber & 4u) == 0u ? MAT_HONEY : MAT_POLLEN);
    }

    int formationSlot = beeFormationSlot(q);
    if (material == MAT_EMPTY && formationSlot >= 0) material = MAT_BEE;

'''
edit("shaders/reset.comp",
     r"    // Canonical new hive\..*?(?=    for \(int flower = 0; flower < 6; \+\+flower\))",
     reset_swarm)

reset_init = r'''    if (scene == SCENE_ECOSYSTEM && material == MAT_BEE) {
        ivec2 queen = ivec2(int(pc.width) - 104, int(pc.height) - 136);
        int slot = beeFormationSlot(p - queen);
        if (slot >= 0) {
            // AUX_PLANT_STEM is the existing authored-swarm bit. Bits 8..15 retain
            // a stable formation slot so each bee can leave and rejoin its own lane.
            cell.aux |= AUX_PLANT_STEM | AUX_BEE_FED;
            cell.aux = (cell.aux & ~0x0000ff00u) | (uint(slot) << 8u);
            setStateValue(cell, 96u + uint(slot % 160));
        }
    }

'''
edit("shaders/reset.comp",
     r"    if \(scene == SCENE_ECOSYSTEM && material == MAT_BEE\) \{.*?(?=    // Machine controllers)",
     reset_init)

move_bees = r'''uint beeFormationSlot(Cell bee) { return (bee.aux >> 8u) & 255u; }
bool isAuthoredBee(Cell bee) { return (bee.aux & AUX_BEE_SWARM) != 0u; }
bool isAuthoredForager(Cell bee) {
    return isAuthoredBee(bee) && (beeFormationSlot(bee) % 5u) == 0u;
}

int beeTargetSignal(Cell bee, ivec2 p) {
    bool carryingPollen = (bee.aux & AUX_BEE_POLLEN) != 0u;
    bool resting = stateValue(bee) > 0u;
    if (carryingPollen || resting)
        return regionalTargetSignal(p, MAT_QUEEN_BEE, MAT_BEE_NEST);
    int flowerSignal = regionalTargetSignal(p, MAT_FLOWER, MAT_FLOWER);
    if ((bee.aux & AUX_BEE_FED) == 0u)
        return max(flowerSignal, regionalTargetSignal(p, MAT_HONEY, MAT_QUEEN_BEE));
    return flowerSignal;
}

int beeWaveVertical(Cell bee, ivec2 p) {
    uint lane = ((bee.aux >> 8u) & 1u) * 3u;
    uint phase = (movePc.step / 2u + uint(max(p.x, 0)) * 2u + lane) % 32u;
    if (phase < 8u) return -1;
    if (phase < 16u) return 0;
    if (phase < 24u) return 1;
    return 0;
}

int beeWaveHorizontal(Cell bee, ivec2 p) {
    uint lane = ((bee.aux >> 9u) & 3u) * 5u;
    uint phase = (movePc.step / 3u + uint(max(p.y, 0)) * 2u + lane) % 64u;
    return phase < 32u ? 1 : -1;
}

ivec2 authoredBeeTravelTarget(Cell bee) {
    ivec2 hiveCenter = ivec2(int(movePc.width) - 104, int(movePc.height) - 136);
    if ((bee.aux & AUX_BEE_POLLEN) != 0u || (bee.aux & AUX_BEE_FED) == 0u)
        return hiveCenter;
    uint flower = (beeFormationSlot(bee) / 5u) % 6u;
    return ivec2(int(movePc.width) - 240 + int(flower) * 26,
                 int(movePc.height) - 70);
}

bool authoredBeeTravelling(Cell bee) {
    if ((bee.aux & AUX_BEE_POLLEN) != 0u || (bee.aux & AUX_BEE_FED) == 0u) return true;
    return isAuthoredForager(bee) && stateValue(bee) == 0u;
}

bool authoredBeeOrbitAllowed(Cell bee, ivec2 sourcePosition,
                             ivec2 targetPosition, uint randomValue) {
    ivec2 center = ivec2(int(movePc.width) - 104, int(movePc.height) - 136);
    ivec2 sourceRadial = sourcePosition - center;
    ivec2 targetRadial = targetPosition - center;
    int sourceRadiusSquared = dot(sourceRadial, sourceRadial);
    int targetRadiusSquared = dot(targetRadial, targetRadial);

    uint lane = beeFormationSlot(bee) % 3u;
    float angle = atan(float(sourceRadial.y), float(sourceRadial.x));
    float baseRadius = lane == 0u ? 18.0 : (lane == 1u ? 30.0 : 44.0);
    float amplitude = lane == 0u ? 3.5 : (lane == 1u ? 5.0 : 7.0);
    float phase = lane == 0u ? 0.0 : (lane == 1u ? 1.7 : 3.2);
    float desiredRadius = baseRadius +
        sin(angle * 3.0 + baseRadius * 0.14 + phase - float(movePc.step) * 0.012) * amplitude;
    int desiredRadiusSquared = int(round(desiredRadius * desiredRadius));
    int tolerance = lane == 0u ? 72 : (lane == 1u ? 120 : 190);

    if (sourceRadiusSquared < desiredRadiusSquared - tolerance &&
        targetRadiusSquared != sourceRadiusSquared) return targetRadiusSquared > sourceRadiusSquared;
    if (sourceRadiusSquared > desiredRadiusSquared + tolerance &&
        targetRadiusSquared != sourceRadiusSquared) return targetRadiusSquared < sourceRadiusSquared;
    if (targetRadiusSquared < desiredRadiusSquared - tolerance * 2 ||
        targetRadiusSquared > desiredRadiusSquared + tolerance * 2) return false;

    ivec2 delta = targetPosition - sourcePosition;
    int radialX = sourceRadial.x > 0 ? 1 : (sourceRadial.x < 0 ? -1 : 0);
    int radialY = sourceRadial.y > 0 ? 1 : (sourceRadial.y < 0 ? -1 : 0);
    ivec2 tangent = ivec2(-radialY, radialX);
    int tangentScore = dot(delta, tangent) * 10;
    if (delta.y == beeWaveVertical(bee, sourcePosition)) tangentScore += 3;
    if (delta.x == beeWaveHorizontal(bee, sourcePosition)) tangentScore += 2;
    return tangentScore > 0 || (tangentScore == 0 && (randomValue & 31u) == 0u);
}

bool beeMoveAllowed(Cell bee, ivec2 sourcePosition, ivec2 targetPosition, uint randomValue) {
    if ((bee.aux & AUX_MOVED) != 0u) return false;
    bool sourceHazard = adjacentHazard(sourcePosition);
    bool targetHazard = adjacentHazard(targetPosition);
    if (sourceHazard != targetHazard) return sourceHazard;

    ivec2 delta = targetPosition - sourcePosition;
    if (isAuthoredBee(bee)) {
        if (authoredBeeTravelling(bee)) {
            ivec2 travelTarget = authoredBeeTravelTarget(bee);
            ivec2 sourceDelta = sourcePosition - travelTarget;
            ivec2 targetDelta = targetPosition - travelTarget;
            int sourceDistance = dot(sourceDelta, sourceDelta);
            int targetDistance = dot(targetDelta, targetDelta);
            if (sourceDistance != targetDistance) return targetDistance < sourceDistance;
            if (delta.x == beeWaveHorizontal(bee, sourcePosition) &&
                delta.y == beeWaveVertical(bee, sourcePosition)) return true;
            return (randomValue & 7u) == 0u;
        }
        return authoredBeeOrbitAllowed(bee, sourcePosition, targetPosition, randomValue);
    }

    int sourceSignal = beeTargetSignal(bee, sourcePosition);
    int targetSignal = beeTargetSignal(bee, targetPosition);
    if (sourceSignal != targetSignal) return targetSignal > sourceSignal;
    int desiredVertical = beeWaveVertical(bee, sourcePosition);
    int desiredHorizontal = beeWaveHorizontal(bee, sourcePosition);
    if (delta.x == desiredHorizontal && delta.y == desiredVertical) return true;
    if (delta.x == desiredHorizontal && delta.y == 0) return true;
    if (delta.y == desiredVertical && delta.x == 0) return (randomValue & 1u) == 0u;
    return (randomValue & 15u) == 0u;
}

'''
edit("shaders/move.comp",
     r"int beeTargetSignal\(Cell bee, ivec2 p\) \{.*?(?=bool magneticMoveAllowed)",
     move_bees)

chem_helpers = r'''uint beeFormationSlot(Cell bee) { return (bee.aux >> 8u) & 255u; }
bool isAuthoredBee(Cell bee) { return (bee.aux & AUX_PLANT_STEM) != 0u; }
bool isAuthoredForager(Cell bee) {
    return isAuthoredBee(bee) && (beeFormationSlot(bee) % 5u) == 0u;
}

ivec2 beeHoneyTarget(ivec2 beePosition, Cell bee) {
    uint start = hash32(indexOf(beePosition) ^ bee.aux ^ 0x10bee10u) & 7u;
    for (uint offset = 0u; offset < 8u; ++offset) {
        ivec2 candidate = beePosition + neighborOffsets[(start + offset) & 7u];
        if (at(candidate).material == MAT_HONEY) return candidate;
    }
    return beePosition;
}

bool hungryBeeTargetsHoney(ivec2 honeyPosition) {
    for (uint i = 0u; i < 8u; ++i) {
        ivec2 beePosition = honeyPosition - neighborOffsets[i];
        Cell bee = at(beePosition);
        if (bee.material == MAT_BEE && (bee.aux & AUX_BEE_POLLEN) == 0u &&
            (bee.aux & AUX_BEE_FED) == 0u &&
            all(equal(beeHoneyTarget(beePosition, bee), honeyPosition))) return true;
    }
    return false;
}

'''
edit("shaders/chemistry.comp", r"int neighborTemperature\(ivec2 p\) \{",
     chem_helpers + "int neighborTemperature(ivec2 p) {")

chem_bees = r'''    // Exactly 200 authored bees retain stable slots. Forty deterministic foragers
    // leave on cooldown, collect pollen, deposit it, drink 26/255 honey, and rejoin.
    if (source.material == MAT_BEE) {
        bool authoredBee = isAuthoredBee(source);
        uint restTimer = stateValue(source);
        if (restTimer > 0u) setStateValue(result, restTimer - 1u);

        if (nearFire || nearLava || hasNeighbor(p, MAT_LIGHTNING)) {
            result = makeCell(MAT_ASH);
        } else if (nearAcid && (randomValue & 3u) == 0u) {
            result = makeCell(MAT_WASTE);
        } else if ((nearSmoke || hasNeighbor(p, MAT_DIRTY_STEAM) || hasNeighbor(p, MAT_RADIATION)) &&
                   (randomValue & 127u) == 0u) {
            result = makeCell(MAT_WASTE);
        } else {
            bool canForage = !authoredBee || (isAuthoredForager(source) && restTimer == 0u);
            if ((source.aux & AUX_BEE_POLLEN) == 0u && (source.aux & AUX_BEE_FED) != 0u &&
                canForage && hasNeighbor(p, MAT_FLOWER)) {
                result.aux |= AUX_BEE_POLLEN;
                result.aux &= ~AUX_BEE_FED;
                setStateValue(result, 0u);
                result.age = min(result.age, 1200u);
            }

            if ((source.aux & AUX_BEE_POLLEN) != 0u &&
                (hasNeighbor(p, MAT_QUEEN_BEE) || hasNeighbor(p, MAT_BEE_NEST))) {
                ivec2 target = beeDepositTarget(p, source);
                uint targetMaterial = at(target).material;
                if (targetMaterial == MAT_EMPTY || targetMaterial == MAT_POLLEN) {
                    result.aux &= ~(AUX_BEE_POLLEN | AUX_BEE_FED);
                    setStateValue(result, 0u);
                }
            }

            if ((result.aux & AUX_BEE_POLLEN) == 0u && (result.aux & AUX_BEE_FED) == 0u) {
                ivec2 honeyTarget = beeHoneyTarget(p, source);
                if (any(notEqual(honeyTarget, p))) {
                    result.aux |= AUX_BEE_FED;
                    setStateValue(result, authoredBee
                        ? 160u + (beeFormationSlot(source) % 80u) : 240u);
                    result.age = min(result.age, 600u);
                }
            }
            if (source.age > 220000u && (randomValue & 8191u) == 0u)
                result = makeCell(MAT_WASTE);
        }
    } else if (source.material == MAT_POLLEN) {
        if ((hasNeighbor(p, MAT_QUEEN_BEE) || hasNeighbor(p, MAT_BEE_NEST)) &&
            source.age > 90u && (randomValue & 63u) == 0u) {
            result = makeCell(MAT_HONEY);
            setStateValue(result, 255u);
        }
    } else if (source.material == MAT_HONEY) {
        if (stateValue(result) == 0u) setStateValue(result, 255u);
        if (hungryBeeTargetsHoney(p)) {
            uint remaining = stateValue(result);
            uint portion = min(remaining, 26u);
            if (remaining <= portion) result = makeCell(MAT_EMPTY);
            else setStateValue(result, remaining - portion);
        } else if (hasNeighbor(p, MAT_BEE) && source.age > 7200u &&
                   (randomValue & 32767u) == 0u) {
            result = makeCell(MAT_BEESWAX);
        }
'''
edit("shaders/chemistry.comp",
     r"    // Bee cycle:.*?(?=    \} else if \(source\.material == MAT_BEESWAX\) \{)",
     chem_bees)

print("Applied SandHybrid Fix30 bee formation and lifecycle patch.")
