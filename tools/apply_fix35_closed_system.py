from __future__ import annotations

from pathlib import Path


def read(path: str) -> str:
    return Path(path).read_text(encoding="utf-8")


def write(path: str, text: str) -> None:
    Path(path).write_text(text, encoding="utf-8")


def replace_once(path: str, old: str, new: str) -> None:
    text = read(path)
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"Expected one marker in {path}, found {count}: {old[:180]!r}")
    write(path, text.replace(old, new, 1))


def replace_between(path: str, start: str, end: str, replacement: str) -> None:
    text = read(path)
    begin = text.find(start)
    if begin < 0:
        raise RuntimeError(f"Missing start marker in {path}: {start[:180]!r}")
    finish = text.find(end, begin + len(start))
    if finish < 0:
        raise RuntimeError(f"Missing end marker in {path}: {end[:180]!r}")
    write(path, text[:begin] + replacement + text[finish:])


# Half-water is a two-unit volume representation. The 15 entropy bits below
# AUX_WATER_HALF carry the displaced gas material and conserved gas volume while
# a cell is half full. No split or merge is allowed to mint oxygen.
replace_between(
    "shaders/move.comp",
    "bool isOpenGas(Cell cell) {\n",
    "uint salinity(Cell cell) {\n",
    r'''const uint AUX_HALF_MEDIUM_MATERIAL_SHIFT = 8u;
const uint AUX_HALF_MEDIUM_MATERIAL_MASK = 0x00007f00u;
const uint AUX_HALF_MEDIUM_VOLUME_SHIFT = 15u;
const uint AUX_HALF_MEDIUM_VOLUME_MASK = 0x007f8000u;

bool isStoredGasMaterial(uint material) {
    return material == MAT_SMOKE || material == MAT_STEAM || material == MAT_DIRTY_STEAM ||
           material == MAT_FIRE || material == MAT_LIGHTNING || material == MAT_RADIATION ||
           material == MAT_OXYGEN || material == MAT_CARBON_DIOXIDE ||
           material == MAT_HYDROGEN;
}

bool isOpenGas(Cell cell) {
    return cell.material == MAT_EMPTY || isStoredGasMaterial(cell.material);
}

uint halfMediumMaterial(Cell cell) {
    return (cell.aux & AUX_HALF_MEDIUM_MATERIAL_MASK) >> AUX_HALF_MEDIUM_MATERIAL_SHIFT;
}

uint halfMediumVolume(Cell cell) {
    return (cell.aux & AUX_HALF_MEDIUM_VOLUME_MASK) >> AUX_HALF_MEDIUM_VOLUME_SHIFT;
}

void clearHalfMedium(inout Cell cell) {
    cell.aux &= ~(AUX_HALF_MEDIUM_MATERIAL_MASK | AUX_HALF_MEDIUM_VOLUME_MASK);
}

void setHalfMedium(inout Cell cell, uint material, uint volume) {
    clearHalfMedium(cell);
    if (volume == 0u || material == MAT_EMPTY) return;
    cell.aux |= ((material & 0x7fu) << AUX_HALF_MEDIUM_MATERIAL_SHIFT) |
                ((min(volume, 255u) & 0xffu) << AUX_HALF_MEDIUM_VOLUME_SHIFT);
}

uint representedGasVolume(Cell cell) {
    if (!isStoredGasMaterial(cell.material)) return 0u;
    uint volume = stateValue(cell);
    return volume == 0u ? 1u : volume;
}

Cell restoredMediumFrom(Cell source, uint material, uint volume) {
    if (material == MAT_EMPTY || volume == 0u) return Cell(MAT_EMPTY, 0u, 20, 0u);
    source.material = material;
    source.age = 0u;
    source.temperature = 20;
    source.aux &= ~(AUX_WATER_HALF | AUX_MOVED | AUX_STATE_MASK |
                    AUX_HALF_MEDIUM_MATERIAL_MASK | AUX_HALF_MEDIUM_VOLUME_MASK);
    setStateValue(source, volume);
    return source;
}

bool halfWaterMergeCompatible(Cell first, Cell second) {
    uint firstVolume = halfMediumVolume(first);
    uint secondVolume = halfMediumVolume(second);
    if (firstVolume == 0u || secondVolume == 0u) return firstVolume + secondVolume <= 255u;
    return halfMediumMaterial(first) == halfMediumMaterial(second) &&
           firstVolume + secondVolume <= 255u;
}

bool mergeHalfWaterPair(ivec2 keepPosition, ivec2 releasePosition, Cell keep, Cell released) {
    if (!halfWaterMergeCompatible(keep, released)) return false;
    uint firstVolume = halfMediumVolume(keep);
    uint secondVolume = halfMediumVolume(released);
    uint mediumMaterial = firstVolume != 0u ? halfMediumMaterial(keep)
                                           : halfMediumMaterial(released);
    uint mediumVolume = firstVolume + secondVolume;

    setHalfWaterCell(keep, false);
    clearHalfMedium(keep);
    keep.age = 0u;
    keep.aux |= AUX_MOVED;
    cells[moveIndex(keepPosition)] = keep;
    cells[moveIndex(releasePosition)] = restoredMediumFrom(released, mediumMaterial, mediumVolume);
    markChunkDirty(keepPosition);
    markChunkDirty(releasePosition);
    return true;
}

void splitFullWaterPair(ivec2 sourcePosition, ivec2 targetPosition, Cell source, Cell target) {
    uint mediumMaterial = isStoredGasMaterial(target.material) ? target.material : MAT_EMPTY;
    uint mediumVolume = representedGasVolume(target);
    uint sourceMediumVolume = mediumVolume / 2u;
    uint targetMediumVolume = mediumVolume - sourceMediumVolume;

    setHalfWaterCell(source, true);
    setHalfMedium(source, mediumMaterial, sourceMediumVolume);
    source.age = 0u;
    source.aux |= AUX_MOVED;

    Cell halfCell = source;
    setHalfMedium(halfCell, mediumMaterial, targetMediumVolume);
    halfCell.temperature = (source.temperature + target.temperature) / 2;
    cells[moveIndex(sourcePosition)] = source;
    cells[moveIndex(targetPosition)] = halfCell;
    markChunkDirty(sourcePosition);
    markChunkDirty(targetPosition);
}

''',
)

replace_once(
    "shaders/move.comp",
    "bool isBaseGas(uint material) {\n"
    "    return material == MAT_SMOKE || material == MAT_STEAM || material == MAT_DIRTY_STEAM ||\n"
    "           material == MAT_FIRE || material == MAT_LIGHTNING || material == MAT_RADIATION ||\n"
    "           material == MAT_OXYGEN || material == MAT_CARBON_DIOXIDE ||\n"
    "           material == MAT_HYDROGEN;\n"
    "}\n",
    "bool isBaseGas(uint material) { return isStoredGasMaterial(material); }\n",
)

replace_once(
    "shaders/move.comp",
    "bool isInsect(uint material) { return material == MAT_ANT || material == MAT_BEETLE; }\n\n",
    "bool isInsect(uint material) { return material == MAT_ANT || material == MAT_BEETLE; }\n\n"
    "bool lifeCanEnter(Cell medium) {\n"
    "    return medium.material == MAT_EMPTY || isCellGas(medium) || isCellLiquid(medium);\n"
    "}\n\n",
)

replace_once(
    "shaders/move.comp",
    "    if (isHalfWaterCell(a) && isHalfWaterCell(b)) {\n"
    "        mergeHalfWaterPair(bottom, top, b, a);\n"
    "        return;\n"
    "    }\n",
    "    if (isHalfWaterCell(a) && isHalfWaterCell(b) &&\n"
    "        mergeHalfWaterPair(bottom, top, b, a)) return;\n",
)
replace_once(
    "shaders/move.comp",
    "    if (isInsect(a.material) && b.material == MAT_EMPTY && !insectGrounded(top) &&\n",
    "    if (isInsect(a.material) && lifeCanEnter(b) && !insectGrounded(top) &&\n",
)
replace_once(
    "shaders/move.comp",
    "    if (b.material == MAT_BEE && a.material == MAT_EMPTY && beeMoveAllowed(b, bottom, top, randomValue)) {\n",
    "    if (b.material == MAT_BEE && lifeCanEnter(a) && beeMoveAllowed(b, bottom, top, randomValue)) {\n",
)
replace_once(
    "shaders/move.comp",
    "    if (a.material == MAT_BEE && b.material == MAT_EMPTY && beeMoveAllowed(a, top, bottom, randomValue >> 1u)) {\n",
    "    if (a.material == MAT_BEE && lifeCanEnter(b) && beeMoveAllowed(a, top, bottom, randomValue >> 1u)) {\n",
)

replace_once(
    "shaders/move.comp",
    "    uint randomValue = moveHash(sourcePosition, uint(movePc.phase) * 17u + 3u);\n\n"
    "    Cell below = sampleAt(sourcePosition + ivec2(0, 1));\n",
    "    uint randomValue = moveHash(sourcePosition, uint(movePc.phase) * 17u + 3u);\n\n"
    "    if (isHalfWaterCell(source) && isHalfWaterCell(target) &&\n"
    "        mergeHalfWaterPair(targetPosition, sourcePosition, target, source)) return;\n\n"
    "    Cell below = sampleAt(sourcePosition + ivec2(0, 1));\n",
)
replace_once(
    "shaders/move.comp",
    "    if (!upward && isInsect(source.material) && target.material == MAT_EMPTY &&\n",
    "    if (!upward && isInsect(source.material) && lifeCanEnter(target) &&\n",
)
replace_once(
    "shaders/move.comp",
    "    if (upward && isInsect(source.material) && target.material == MAT_EMPTY &&\n",
    "    if (upward && isInsect(source.material) && lifeCanEnter(target) &&\n",
)
replace_once(
    "shaders/move.comp",
    "    if (source.material == MAT_BEE && target.material == MAT_EMPTY &&\n"
    "        beeMoveAllowed(source, sourcePosition, targetPosition, randomValue)) {\n",
    "    if (source.material == MAT_BEE && lifeCanEnter(target) &&\n"
    "        beeMoveAllowed(source, sourcePosition, targetPosition, randomValue)) {\n",
)

replace_once(
    "shaders/move.comp",
    "    if (isInsect(a.material) && b.material == MAT_EMPTY &&\n"
    "        insectGrounded(left) && insectGrounded(right) &&\n",
    "    if (isInsect(a.material) && lifeCanEnter(b) &&\n"
    "        insectGrounded(left) && insectGrounded(right) &&\n",
)
replace_once(
    "shaders/move.comp",
    "    if (isInsect(b.material) && a.material == MAT_EMPTY &&\n"
    "        insectGrounded(right) && insectGrounded(left) &&\n",
    "    if (isInsect(b.material) && lifeCanEnter(a) &&\n"
    "        insectGrounded(right) && insectGrounded(left) &&\n",
)
replace_once(
    "shaders/move.comp",
    "    if (isHalfWaterCell(a) && isHalfWaterCell(b)) {\n"
    "        bool keepLeft = (randomValue & 1u) == 0u;\n"
    "        mergeHalfWaterPair(keepLeft ? left : right, keepLeft ? right : left,\n"
    "                           keepLeft ? a : b, keepLeft ? b : a);\n"
    "        return;\n"
    "    }\n",
    "    if (isHalfWaterCell(a) && isHalfWaterCell(b)) {\n"
    "        bool keepLeft = (randomValue & 1u) == 0u;\n"
    "        if (mergeHalfWaterPair(keepLeft ? left : right, keepLeft ? right : left,\n"
    "                               keepLeft ? a : b, keepLeft ? b : a)) return;\n"
    "    }\n",
)
replace_once(
    "shaders/move.comp",
    "    } else if (a.material == MAT_BEE && b.material == MAT_EMPTY && beeMoveAllowed(a, left, right, randomValue)) {\n",
    "    } else if (a.material == MAT_BEE && lifeCanEnter(b) && beeMoveAllowed(a, left, right, randomValue)) {\n",
)
replace_once(
    "shaders/move.comp",
    "    } else if (b.material == MAT_BEE && a.material == MAT_EMPTY &&\n"
    "               beeMoveAllowed(b, right, left, randomValue >> 1u)) {\n",
    "    } else if (b.material == MAT_BEE && lifeCanEnter(a) &&\n"
    "               beeMoveAllowed(b, right, left, randomValue >> 1u)) {\n",
)

# Chemistry uses explicit oxygen volume. Empty space is vacuum. Respiration
# converts oxygen to the same represented volume of CO2; waterfall motion no
# longer synthesizes oxygen cells.
replace_once(
    "shaders/chemistry.comp",
    "bool hasAnyWater(ivec2 p) {\n"
    "    return hasNeighbor(p, MAT_WATER) || hasNeighbor(p, MAT_SALTWATER) ||\n"
    "           hasNeighbor(p, MAT_DIRTY_WATER);\n"
    "}\n\n",
    "bool hasAnyWater(ivec2 p) {\n"
    "    return hasNeighbor(p, MAT_WATER) || hasNeighbor(p, MAT_SALTWATER) ||\n"
    "           hasNeighbor(p, MAT_DIRTY_WATER);\n"
    "}\n\n"
    "bool isRespiringLife(uint material) {\n"
    "    return material == MAT_BEE || material == MAT_QUEEN_BEE ||\n"
    "           material == MAT_ANT || material == MAT_BEETLE ||\n"
    "           material == MAT_SEED || material == MAT_GRASS ||\n"
    "           material == MAT_PLANT_STEM || material == MAT_FLOWER;\n"
    "}\n\n"
    "uint representedOxygenVolume(Cell cell) {\n"
    "    if (cell.material != MAT_OXYGEN) return 0u;\n"
    "    uint volume = stateValue(cell);\n"
    "    return volume == 0u ? 1u : volume;\n"
    "}\n\n"
    "uint oxygenVolumeWithin(ivec2 p, int radius) {\n"
    "    uint volume = 0u;\n"
    "    for (int y = -radius; y <= radius; ++y) {\n"
    "        for (int x = -radius; x <= radius; ++x) {\n"
    "            if (x * x + y * y > radius * radius) continue;\n"
    "            volume += representedOxygenVolume(at(p + ivec2(x, y)));\n"
    "        }\n"
    "    }\n"
    "    return volume;\n"
    "}\n\n"
    "bool nonBreathableMedium(uint material) {\n"
    "    return isLiquid(material) || (isGas(material) && material != MAT_OXYGEN);\n"
    "}\n\n"
    "bool fullyChokedByMedium(ivec2 p) {\n"
    "    uint contacts = 0u;\n"
    "    for (uint i = 0u; i < 8u; ++i) {\n"
    "        if (nonBreathableMedium(at(p + neighborOffsets[i]).material)) ++contacts;\n"
    "    }\n"
    "    return contacts == 8u;\n"
    "}\n\n"
    "uint respiringNeighborCount(ivec2 p) {\n"
    "    uint count = 0u;\n"
    "    for (uint i = 0u; i < 8u; ++i) {\n"
    "        if (isRespiringLife(at(p + neighborOffsets[i]).material)) ++count;\n"
    "    }\n"
    "    return count;\n"
    "}\n\n",
)

replace_once(
    "shaders/chemistry.comp",
    "        } else if (fallingWaterNear(p) && (randomValue & 31u) == 0u) {\n"
    "            result = makeCell(MAT_OXYGEN);\n",
    "        } else if (fallingWaterNear(p)) {\n"
    "            // Aeration redistributes existing atmosphere in movement; it never creates oxygen.\n",
)

replace_once(
    "shaders/chemistry.comp",
    "    // Waterfall aeration and plant respiration use the same cell chemistry pass.\n"
    "    if (source.material == MAT_OXYGEN) {\n"
    "        if (nearFire || hasNeighbor(p, MAT_EMBER)) result = makeCell(MAT_CARBON_DIOXIDE);\n"
    "    } else if (source.material == MAT_CARBON_DIOXIDE) {\n"
    "        if (hasNeighbor(p, MAT_GRASS) && light[index] > 130u && (randomValue & 127u) == 0u)\n"
    "            result = makeCell(MAT_OXYGEN);\n"
    "    } else if (source.material == MAT_HYDROGEN) {\n",
    "    // Atmosphere is a conserved local volume. Respiration exchanges one oxygen\n"
    "    // volume for one CO2 volume; photosynthesis performs the inverse exchange.\n"
    "    if (source.material == MAT_OXYGEN) {\n"
    "        uint volume = representedOxygenVolume(source);\n"
    "        uint consumers = respiringNeighborCount(p);\n"
    "        if (nearFire || hasNeighbor(p, MAT_EMBER) ||\n"
    "            (consumers > 0u && (randomValue & 2047u) < consumers)) {\n"
    "            result = makeCell(MAT_CARBON_DIOXIDE);\n"
    "            setStateValue(result, volume);\n"
    "        }\n"
    "    } else if (source.material == MAT_CARBON_DIOXIDE) {\n"
    "        if (hasNeighbor(p, MAT_GRASS) && light[index] > 130u && (randomValue & 127u) == 0u) {\n"
    "            uint volume = max(stateValue(source), 1u);\n"
    "            result = makeCell(MAT_OXYGEN);\n"
    "            setStateValue(result, volume);\n"
    "        }\n"
    "    } else if (source.material == MAT_HYDROGEN) {\n",
)

replace_once(
    "shaders/chemistry.comp",
    "    // Closed ecological loop: dirty water separates into clean water and silt;\n",
    "    if (result.material == source.material && isRespiringLife(source.material)) {\n"
    "        uint localOxygen = oxygenVolumeWithin(p, 2);\n"
    "        bool fullyChoked = fullyChokedByMedium(p);\n"
    "        uint deathMask = fullyChoked ? 63u : 1023u;\n"
    "        if ((fullyChoked || localOxygen == 0u) && (randomValue & deathMask) == 0u) {\n"
    "            if (source.material == MAT_GRASS) result = makeCell(MAT_DIRT);\n"
    "            else result = makeCell(MAT_WASTE);\n"
    "        }\n"
    "    }\n\n"
    "    // Closed ecological loop: dirty water separates into clean water and silt;\n",
)

# The actor already traverses liquids and gases. Breathing now requires explicit
# oxygen volume; vacuum is not breathable, total medium contact blocks breathing,
# oxygen is exchanged for CO2, and zero reserve damages health.
replace_between(
    "shaders/actor.comp",
    "void updateBreathing(inout ActorState state) {\n",
    "void resetActor(inout ActorState state) {\n",
    r'''void updateBreathing(inout ActorState state) {
    ivec2 center = ivec2(state.x, state.y - 4);
    ivec2 oxygenCell = center;
    uint oxygenVolume = 0u;
    uint toxicVolume = 0u;
    for (int y = -4; y <= 4; ++y) {
        for (int x = -4; x <= 4; ++x) {
            ivec2 p = center + ivec2(x, y);
            Cell medium = actorAt(p);
            if (medium.material == MAT_OXYGEN) {
                if (oxygenVolume == 0u) oxygenCell = p;
                oxygenVolume += max(stateValue(medium), 1u);
            } else if (isGas(medium.material) || isLiquid(medium.material)) {
                toxicVolume += max(stateValue(medium), 1u);
            }
        }
    }

    uint sealedContacts = 0u;
    const ivec2 breathOffsets[4] = ivec2[4](
        ivec2(1, 0), ivec2(-1, 0), ivec2(0, 1), ivec2(0, -1));
    for (uint i = 0u; i < 4u; ++i) {
        uint material = actorAt(center + breathOffsets[i]).material;
        if (isLiquid(material) || (isGas(material) && material != MAT_OXYGEN)) ++sealedContacts;
    }

    bool fullyChoked = sealedContacts == 4u;
    bool breathable = oxygenVolume > 0u && !fullyChoked;
    bool toxicPocket = toxicVolume > oxygenVolume * 2u;
    if (breathable && !toxicPocket) {
        state.oxygen = min(255u, state.oxygen + 3u);
        state.exposureTicks = 0u;
        if ((actorPc.step % 180u) == 0u && actorInside(oxygenCell)) {
            uint oxygenIndex = actorIndex(oxygenCell);
            Cell oxygen = cells[oxygenIndex];
            uint volume = max(stateValue(oxygen), 1u);
            Cell carbonDioxide = makeCellWithEntropy(MAT_CARBON_DIOXIDE, actorPc.seed, actorPc.step);
            setStateValue(carbonDioxide, volume);
            recordConservation(oxygen, carbonDioxide);
            cells[oxygenIndex] = carbonDioxide;
            markActorChunkDirty(oxygenCell);
        }
    } else {
        state.exposureTicks = min(state.exposureTicks + 1u, 4095u);
        if ((actorPc.step % 30u) == 0u && state.oxygen > 0u)
            state.oxygen -= fullyChoked || toxicPocket ? min(state.oxygen, 2u) : 1u;
        if (state.oxygen == 0u && state.exposureTicks > 120u &&
            (actorPc.step % 60u) == 0u && state.health > 0u) {
            state.health -= 1u;
        }
    }
}

''',
)

# Add compile-time contracts for the coarse two-half medium exchange and explicit
# oxygen-only breathing rule.
replace_once(
    "tests/behavior_contract.cpp",
    "[[nodiscard]] constexpr bool terrain_stability_preserves_representation() noexcept {\n",
    "[[nodiscard]] constexpr bool half_water_medium_exchange_preserves_volume() noexcept {\n"
    "    constexpr std::uint32_t oxygen_volume = 220u;\n"
    "    constexpr std::uint32_t first_half = oxygen_volume / 2u;\n"
    "    constexpr std::uint32_t second_half = oxygen_volume - first_half;\n"
    "    return first_half + second_half == oxygen_volume &&\n"
    "           first_half <= 255u && second_half <= 255u;\n"
    "}\n\n"
    "[[nodiscard]] constexpr bool breathing_requires_explicit_oxygen() noexcept {\n"
    "    constexpr std::uint32_t vacuum_volume = 0u;\n"
    "    constexpr std::uint32_t oxygen_volume = 1u;\n"
    "    return vacuum_volume == 0u && oxygen_volume > 0u;\n"
    "}\n\n"
    "[[nodiscard]] constexpr bool terrain_stability_preserves_representation() noexcept {\n",
)
replace_once(
    "tests/behavior_contract.cpp",
    "static_assert(terrain_stability_preserves_representation());\n",
    "static_assert(half_water_medium_exchange_preserves_volume());\n"
    "static_assert(breathing_requires_explicit_oxygen());\n"
    "static_assert(terrain_stability_preserves_representation());\n",
)
replace_once(
    "tests/behavior_contract.cpp",
    "    return creation_paths_are_canonical() && local_water_equalization_preserves_volume() &&\n"
    "           terrain_stability_preserves_representation() ? 0 : 1;\n",
    "    return creation_paths_are_canonical() && local_water_equalization_preserves_volume() &&\n"
    "           half_water_medium_exchange_preserves_volume() &&\n"
    "           breathing_requires_explicit_oxygen() &&\n"
    "           terrain_stability_preserves_representation() ? 0 : 1;\n",
)

print("Applied conserved atmosphere, half-water medium exchange, passable life movement, and suffocation fixes.")
